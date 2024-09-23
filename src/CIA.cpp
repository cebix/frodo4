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
 *  - The TOD clock should not be stopped on a read access, but latched.
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

	ta = tb = 0xffff;
	latcha = latchb = 1;

	tod_10ths = tod_sec = tod_min = tod_hr = 0;
	alm_10ths = alm_sec = alm_min = alm_hr = 0;

	sdr = icr = cra = crb = int_mask = 0;

	tod_halt = ta_cnt_phi2 = tb_cnt_phi2 = tb_cnt_ta = false;
	tod_divider = 0;
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

	s->ta_lo = ta & 0xff;
	s->ta_hi = ta >> 8;
	s->tb_lo = tb & 0xff;
	s->tb_hi = tb >> 8;
	s->latcha = latcha;
	s->latchb = latchb;
	s->cra = cra;
	s->crb = crb;

	s->tod_10ths = tod_10ths;
	s->tod_sec = tod_sec;
	s->tod_min = tod_min;
	s->tod_hr = tod_hr;
	s->alm_10ths = alm_10ths;
	s->alm_sec = alm_sec;
	s->alm_min = alm_min;
	s->alm_hr = alm_hr;

	s->sdr = sdr;

	s->int_flags = icr;
	s->int_mask = int_mask;

	s->ta_int_next_cycle = false;
	s->tb_int_next_cycle = false;
	s->has_new_cra = false;
	s->has_new_crb = false;
	s->ta_toggle = false;
	s->tb_toggle = false;
	s->ta_state = (cra & 1) ? T_COUNT : T_STOP;
	s->tb_state = (crb & 1) ? T_COUNT : T_STOP;
	s->new_cra = 0;
	s->new_crb = 0;
	s->ta_output = 0;
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

	ta = (s->ta_hi << 8) | s->ta_lo;
	tb = (s->tb_hi << 8) | s->tb_lo;
	latcha = s->latcha;
	latchb = s->latchb;
	cra = s->cra;
	crb = s->crb;

	tod_10ths = s->tod_10ths;
	tod_sec = s->tod_sec;
	tod_min = s->tod_min;
	tod_hr = s->tod_hr;
	alm_10ths = s->alm_10ths;
	alm_sec = s->alm_sec;
	alm_min = s->alm_min;
	alm_hr = s->alm_hr;

	sdr = s->sdr;

	icr = s->int_flags;
	int_mask = s->int_mask;

	tod_halt = false;
	ta_cnt_phi2 = ((cra & 0x21) == 0x01);
	tb_cnt_phi2 = ((crb & 0x61) == 0x01);
	tb_cnt_ta = ((crb & 0x41) == 0x41);		// Ignore CNT, which is pulled high
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

uint8_t MOS6526::timer_on_pb(uint8_t byte)
{
	// Not emulated
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
	if (ta_cnt_phi2) {
		ta = tmp = ta - cycles;		// Decrement timer

		if (tmp > 0xffff) {			// Underflow?
			ta = latcha;			// Reload timer

			if (cra & 8) {			// One-shot?
				cra &= 0xfe;
				ta_cnt_phi2 = false;
			}
			set_int_flag(1);
			if (tb_cnt_ta) {		// Timer B counting underflows of Timer A?
				tb = tmp = tb - 1;	// tmp = --tb doesn't work
				if (tmp > 0xffff) goto tb_underflow;
			}
		}
	}

	// Timer B
	if (tb_cnt_phi2) {
		tb = tmp = tb - cycles;		// Decrement timer

		if (tmp > 0xffff) {			// Underflow?
tb_underflow:
			tb = latchb;

			if (crb & 8) {			// One-shot?
				crb &= 0xfe;
				tb_cnt_phi2 = false;
				tb_cnt_ta = false;
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
	uint8_t lo, hi;

	// Decrement frequency divider
	if (tod_divider) {
		tod_divider--;
	} else {

		// Reload divider according to 50/60 Hz flag
		if (cra & 0x80) {
			tod_divider = 4;
		} else {
			tod_divider = 5;
		}

		// 1/10 seconds
		tod_10ths++;
		if (tod_10ths > 9) {
			tod_10ths = 0;

			// Seconds
			lo = (tod_sec & 0x0f) + 1;
			hi = tod_sec >> 4;
			if (lo > 9) {
				lo = 0;
				hi++;
			}
			if (hi > 5) {
				tod_sec = 0;

				// Minutes
				lo = (tod_min & 0x0f) + 1;
				hi = tod_min >> 4;
				if (lo > 9) {
					lo = 0;
					hi++;
				}
				if (hi > 5) {
					tod_min = 0;

					// Hours
					lo = (tod_hr & 0x0f) + 1;
					hi = (tod_hr >> 4) & 1;
					tod_hr &= 0x80;		// Keep AM/PM flag
					if (lo > 9) {
						lo = 0;
						hi++;
					}
					tod_hr |= (hi << 4) | lo;
					if ((tod_hr & 0x1f) > 0x11) {
						tod_hr = (tod_hr & 0x80) ^ 0x80;
					}
				} else {
					tod_min = (hi << 4) | lo;
				}
			} else {
				tod_sec = (hi << 4) | lo;
			}
		}

		// Alarm time reached? Trigger interrupt if enabled
		if (tod_10ths == alm_10ths && tod_sec == alm_sec &&
			tod_min == alm_min && tod_hr == alm_hr) {
			set_int_flag(4);
		}
	}
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
