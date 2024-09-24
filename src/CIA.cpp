/*
 *  CIA.cpp - 6526 emulation
 *
 *  Frodo Copyright (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Notes:
 * ------
 *
 *  - The EmulateLine() function is called for every emulated raster line.
 *    It counts down the timers and triggers interrupts if necessary.
 *  - The TOD clocks are counted by CountTOD() during the VBlank, so the
 *    input frequency is 50Hz.
 *  - The fields KeyMatrix and RevMatrix contain one bit for each key on the
 *    C64 keyboard (0: key pressed, 1: key released). KeyMatrix is used for
 *    normal keyboard polling (PRA->PRB), RevMatrix for reversed polling
 *    (PRB->PRA).
 *
 * Incompatibilities:
 * ------------------
 *
 *  - The SDR interrupt is faked.
 */

#include "sysdeps.h"

#include "CIA.h"
#include "CPUC64.h"
#include "CPU1541.h"
#include "VIC.h"
#include "Prefs.h"


/*
 *  Reset the CIA
 */

void MOS6526::Reset()
{
	pra = prb = ddra = ddrb = 0;

	tod_10ths = tod_sec = tod_min = 0; tod_hr = 1;
	ltc_10ths = ltc_sec = ltc_min = 0; ltc_hr = 1;
	alm_10ths = alm_sec = alm_min = alm_hr = 0;

	sdr = icr = cra = crb = int_mask = 0;

	ta.counter   = tb.counter   = 0xffff;
	ta.latch     = tb.latch     = 1;
	ta.pb_toggle = tb.pb_toggle = false;

	tod_counter = 0;
	tod_halted = true;
	tod_latched = false;
	tod_alarm = false;
}

void MOS6526_1::Reset()
{
	MOS6526::Reset();

	// Clear keyboard matrix and joystick states
	for (int i=0; i<8; i++) {
		KeyMatrix[i] = RevMatrix[i] = 0xff;
	}

	Joystick1 = Joystick2 = 0xff;
	prev_lp = 0x10;
}

void MOS6526_2::Reset()
{
	MOS6526::Reset();

	// VA14/15 = 0
	the_vic->ChangedVA(0);

	// IEC
	IECLines = 0x38;	// DATA, CLK, ATN high
}


/*
 *  Get CIA state
 */

void MOS6526::GetState(MOS6526State * s) const
{
	s->pra = pra;
	s->prb = prb;
	s->ddra = ddra;
	s->ddrb = ddrb;

	s->ta_lo = ta.counter & 0xff;
	s->ta_hi = ta.counter >> 8;
	s->tb_lo = tb.counter & 0xff;
	s->tb_hi = tb.counter >> 8;
	s->ta_latch = ta.latch;
	s->tb_latch = tb.latch;

	s->cra = cra;
	s->crb = crb;

	s->tod_10ths = tod_10ths;
	s->tod_sec = tod_sec;
	s->tod_min = tod_min;
	s->tod_hr = tod_hr;
	s->ltc_10ths = ltc_10ths;
	s->ltc_sec = ltc_sec;
	s->ltc_min = ltc_min;
	s->ltc_hr = ltc_hr;
	s->alm_10ths = alm_10ths;
	s->alm_sec = alm_sec;
	s->alm_min = alm_min;
	s->alm_hr = alm_hr;

	s->sdr = sdr;

	s->int_flags = icr;
	s->int_mask = int_mask;

	s->tod_counter = tod_counter;
	s->tod_halted = tod_halted;
	s->tod_latched = tod_latched;
	s->tod_alarm = tod_alarm;

	s->ta_pb_toggle = ta.pb_toggle;
	s->tb_pb_toggle = tb.pb_toggle;
	s->ta_output = false;
	s->tb_output = false;
	s->ta_count_delay = 0;
	s->tb_count_delay = 0;
	s->ta_load_delay = 0;
	s->tb_load_delay = 0;
	s->ta_oneshot_delay = 0;
	s->tb_oneshot_delay = 0;

	s->irq_delay = 0;
	s->read_icr = false;
}


/*
 *  Restore CIA state
 */

void MOS6526::SetState(const MOS6526State * s)
{
	pra = s->pra;
	prb = s->prb;
	ddra = s->ddra;
	ddrb = s->ddrb;

	ta.counter = (s->ta_hi << 8) | s->ta_lo;
	tb.counter = (s->tb_hi << 8) | s->tb_lo;
	ta.latch = s->ta_latch;
	tb.latch = s->tb_latch;

	cra = s->cra;
	crb = s->crb;

	tod_10ths = s->tod_10ths;
	tod_sec = s->tod_sec;
	tod_min = s->tod_min;
	tod_hr = s->tod_hr;
	ltc_10ths = s->ltc_10ths;
	ltc_sec = s->ltc_sec;
	ltc_min = s->ltc_min;
	ltc_hr = s->ltc_hr;
	alm_10ths = s->alm_10ths;
	alm_sec = s->alm_sec;
	alm_min = s->alm_min;
	alm_hr = s->alm_hr;

	sdr = s->sdr;

	icr = s->int_flags;
	int_mask = s->int_mask;

	tod_counter = s->tod_counter;
	tod_halted = s->tod_halted;
	tod_latched = s->tod_latched;
	tod_alarm = s->tod_alarm;

	ta.pb_toggle = s->ta_pb_toggle;
	tb.pb_toggle = s->tb_pb_toggle;
}

void MOS6526_2::SetState(const MOS6526State * s)
{
	MOS6526::SetState(s);

	uint8_t inv_out = ~pra & ddra;
	IECLines = inv_out & 0x38;
}


/*
 *  Output TA/TB to PB6/7
 */

uint8_t MOS6526::timer_on_pb(uint8_t byte) const
{
	// TODO: Not emulated
	return byte;
}


/*
 *  Read from register (CIA 1)
 */

uint8_t MOS6526_1::ReadRegister(uint8_t reg)
{
	switch (reg) {
		case 0: {	// Port A: Handle keyboard and joysticks
			uint8_t ret = PAOut(), tst = PBOut() & Joystick1;
			if (!(tst & 0x01)) ret &= RevMatrix[0];	// AND all active columns
			if (!(tst & 0x02)) ret &= RevMatrix[1];
			if (!(tst & 0x04)) ret &= RevMatrix[2];
			if (!(tst & 0x08)) ret &= RevMatrix[3];
			if (!(tst & 0x10)) ret &= RevMatrix[4];
			if (!(tst & 0x20)) ret &= RevMatrix[5];
			if (!(tst & 0x40)) ret &= RevMatrix[6];
			if (!(tst & 0x80)) ret &= RevMatrix[7];
			return ret & Joystick2;
		}
		case 1: {	// Port B: Handle keyboard and joysticks
			uint8_t ret = ~ddrb, tst = PAOut() & Joystick2;
			if (!(tst & 0x01)) ret &= KeyMatrix[0];	// AND all active rows
			if (!(tst & 0x02)) ret &= KeyMatrix[1];
			if (!(tst & 0x04)) ret &= KeyMatrix[2];
			if (!(tst & 0x08)) ret &= KeyMatrix[3];
			if (!(tst & 0x10)) ret &= KeyMatrix[4];
			if (!(tst & 0x20)) ret &= KeyMatrix[5];
			if (!(tst & 0x40)) ret &= KeyMatrix[6];
			if (!(tst & 0x80)) ret &= KeyMatrix[7];
			return (ret | (prb & ddrb)) & Joystick1;
		}
		default:
			return read_register(reg);
	}
}


/*
 *  Read from register (CIA 2)
 */

uint8_t MOS6526_2::ReadRegister(uint8_t reg)
{
	switch (reg) {
		case 0: {	// Port A: IEC port
			uint8_t in = ((the_cpu_1541->CalcIECLines() & 0x30) << 2)	// DATA and CLK from bus
			           | 0x3f;											// Other lines high
			SetPAIn(in);
			break;
		}
		case 1: {	// Port B: User port
			SetPBIn(0);
			break;
		}
	}

	return read_register(reg);
}


/*
 *  Write to register (CIA 1)
 */

// Write to port B, check for lightpen interrupt
inline void MOS6526_1::check_lp()
{
	uint8_t new_lp = PBOut() & 0x10;
	if (new_lp != prev_lp) {
		the_vic->TriggerLightpen();
		prev_lp = new_lp;
	}
}

void MOS6526_1::WriteRegister(uint8_t reg, uint8_t byte)
{
	write_register(reg, byte);

	switch (reg) {
		case 1:	// Port B: Handle VIC lightpen input
		case 3:
			check_lp();
			break;
	}
}


/*
 *  Write to register (CIA 2)
 */

// Write to port A, check for VIC bank change and IEC lines
inline void MOS6526_2::write_pa(uint8_t inv_out)
{
	the_vic->ChangedVA(inv_out & 3);

	uint8_t old_lines = IECLines;
	IECLines = inv_out & 0x38;

	if ((IECLines ^ old_lines) & 0x08) {	// ATN changed
		if (old_lines & 0x08) {				// ATN 1->0
			the_cpu_1541->TriggerIECInterrupt();
		}
	}
}

void MOS6526_2::WriteRegister(uint8_t reg, uint8_t byte)
{
	write_register(reg, byte);

	switch (reg) {
		case 0:	// Port A: Handle VIC bank and IEC port
		case 2:
			write_pa(~PAOut());
			break;
	}
}


/*
 *  Emulate CIA for one raster line
 */

void MOS6526::EmulateLine(int cycles)
{
	unsigned long tmp;

	// Timer A
	if ((cra & 0x21) == 0x01) {		// Counting Phi2 and started?
		ta.counter = tmp = ta.counter - cycles;		// Decrement timer

		if (tmp > 0xffff) {			// Underflow?
			ta.counter = ta.latch;	// Reload timer

			if (cra & 8) {			// One-shot?
				cra &= ~1;			// Stop timer
			}

			set_int_flag(1);

			if ((crb & 0x41) == 0x41) {		// Timer B counting underflows of Timer A and started?
				tb.counter = tmp = tb.counter - 1;
				if (tmp > 0xffff) goto tb_underflow;
			}
		}
	}

	// Timer B
	if ((crb & 0x61) == 0x01) {		// Count Phi2 and started?
		tb.counter = tmp = tb.counter - cycles;		// Decrement timer

		if (tmp > 0xffff) {			// Underflow?
tb_underflow:
			tb.counter = tb.latch;	// Reload timer

			if (crb & 8) {			// One-shot?
				crb &= ~1;			// Stop timer
			}

			set_int_flag(2);
		}
	}
}


/*
 *  Count CIA TOD clock (called during VBlank)
 */

void MOS6526::CountTOD()
{
	if (tod_halted)
		return;

	// Increment TOD counter
	unsigned tod_compare = (cra & 0x80) ? 4 : 5;
	if (tod_counter != tod_compare) {
		++tod_counter;
		if (tod_counter > 5) {
			tod_counter = 0;
		}
		return;
	}

	// Count TOD
	tod_counter = 0;

	uint8_t lo, hi;

	// 1/10 seconds
	tod_10ths = (tod_10ths + 1) & 0x0f;
	if (tod_10ths == 10) {
		tod_10ths = 0;

		// Seconds
		lo = (tod_sec + 1) & 0x0f;
		hi = tod_sec >> 4;
		if (lo == 10) {
			lo = 0;
			hi = (hi + 1) & 0x07;
		}
		if (hi == 6) {
			tod_sec = 0;

			// Minutes
			lo = (tod_min + 1) & 0x0f;
			hi = tod_min >> 4;
			if (lo == 10) {
				lo = 0;
				hi = (hi + 1) & 0x07;
			}
			if (hi == 6) {
				tod_min = 0;

				// Hours
				lo = (tod_hr + 1) & 0x0f;
				hi = (tod_hr >> 4) & 1;
				if (((hi << 4) | lo) == 10) {
					lo = 0;
					hi = (hi + 1) & 1;
				}
				tod_hr = (tod_hr & 0x80) | (hi << 4) | lo;
				if ((tod_hr & 0x1f) == 0x13) {
					tod_hr = (tod_hr & 0x80) | 1;	// 1 AM follows 12 AM, 1 PM follows 12 PM
				} else if ((tod_hr & 0x1f) == 0x12) {
					tod_hr ^= 0x80;					// 12 AM follows 11 PM, 12 PM follows 11 AM
				}
			} else {
				tod_min = (hi << 4) | lo;
			}
		} else {
			tod_sec = (hi << 4) | lo;
		}
	}

	// Check for alarm
	check_tod_alarm();
}


/*
 *  Interrupt functions
 */

// Trigger IRQ (CIA 1)
void MOS6526_1::trigger_irq()
{
	the_cpu->TriggerCIAIRQ();
}

// Clear IRQ (CIA 1)
void MOS6526_1::clear_irq()
{
	the_cpu->ClearCIAIRQ();
}

// Trigger NMI (CIA 2)
void MOS6526_2::trigger_irq()
{
	the_cpu->TriggerNMI();
}

// Clear NMI (CIA 2)
void MOS6526_2::clear_irq()
{
	the_cpu->ClearNMI();
}
