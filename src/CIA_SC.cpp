/*
 *  CIA_SC.cpp - Single-cycle 6526 emulation
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
 *  - The EmulateCycle() function is called for every emulated Phi2 clock
 *    cycle. It counts down the timers and triggers interrupts if necessary.
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
 *  - Some small incompatibilities with the timers.
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

	tod_10ths = tod_sec = tod_min = 0; tod_hr = 1;
	ltc_10ths = ltc_sec = ltc_min = 0; ltc_hr = 1;
	alm_10ths = alm_sec = alm_min = alm_hr = 0;

	sdr = icr = cra = crb = int_mask = 0;

	tod_counter = 0;
	tod_halted = true;
	tod_latched = false;
	tod_alarm = false;

	tb_cnt_phi2 = tb_cnt_ta = false;

	has_new_cra = has_new_crb = false;
	ta_toggle = tb_toggle = false;
	ta_state = tb_state = T_STOP;
	ta_output = tb_output = 0;

	irq_delay = 0;
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

	s->has_new_cra = has_new_cra;
	s->has_new_crb = has_new_crb;
	s->ta_toggle = ta_toggle;
	s->tb_toggle = tb_toggle;
	s->ta_state = ta_state;
	s->tb_state = tb_state;
	s->new_cra = new_cra;
	s->new_crb = new_crb;
	s->ta_output = ta_output;
	s->tb_output = tb_output;

	s->irq_delay = irq_delay;
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

	tb_cnt_phi2 = ((crb & 0x60) == 0x00);
	tb_cnt_ta = ((crb & 0x40) == 0x40);		// Ignore CNT, which is pulled high

	has_new_cra = s->has_new_cra;
	has_new_crb = s->has_new_crb;
	ta_toggle = s->ta_toggle;
	tb_toggle = s->tb_toggle;
	ta_state = s->ta_state;
	tb_state = s->tb_state;
	new_cra = s->new_cra;
	new_crb = s->new_crb;
	ta_output = s->ta_output;
	tb_output = s->tb_output;

	irq_delay = s->irq_delay;
}

void MOS6526_2::SetState(const MOS6526State * s)
{
	MOS6526::SetState(s);

	uint8_t inv_out = ~PAOut();
	IECLines = inv_out & 0x38;
}


/*
 *  Output TA/TB to PB6/7
 */

uint8_t MOS6526::timer_on_pb(uint8_t byte)
{
	if (cra & 0x02) {

		// TA output to PB6
		if ((cra & 0x04) ? ta_toggle : (ta_output & 1)) {
			byte |= 0x40;
		} else {
			byte &= 0xbf;
		}
	}

	if (crb & 0x02) {

		// TB output to PB7
		if ((crb & 0x04) ? tb_toggle : (tb_output & 1)) {
			byte |= 0x80;
		} else {
			byte &= 0x7f;
		}
	}

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
			ret = (ret | (prb & ddrb)) & Joystick1;

			if ((cra | crb) & 0x02) {	// TA/TB output to PB enabled?
				ret = timer_on_pb(ret);
			}

			return ret;
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
		case 0: {	// Port A: IEC
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
 *  Emulate CIA for one cycle
 */

void MOS6526::EmulateCycle()
{
	// Shift delay lines
	ta_output <<= 1;
	tb_output <<= 1;
	irq_delay <<= 1;

	// Timer A state machine
	switch (ta_state) {
		case T_WAIT_THEN_COUNT:
			ta_state = T_COUNT;		// fall through
		case T_STOP:
			goto ta_idle;
		case T_LOAD_THEN_STOP:
			ta = latcha;			// Reload timer
			ta_state = T_STOP;
			goto ta_idle;
		case T_LOAD_THEN_COUNT:
			ta = latcha;			// Reload timer
			ta_state = T_COUNT;
			goto ta_idle;
		case T_LOAD_THEN_WAIT_THEN_COUNT:
			ta_state = T_WAIT_THEN_COUNT;
			ta = latcha;			// Reload timer
			goto ta_idle;
		case T_COUNT:
			goto ta_count;
		case T_COUNT_THEN_STOP:
			ta_state = T_STOP;
			goto ta_count;
	}

	// Count timer A
ta_count:
	if ((cra & 0x20) == 0) {
		if (ta != 0) {	// Decrement timer if != 0
			--ta;
		}
		if (ta == 0) {	// Timer expired?
			if (ta_state != T_STOP) {
ta_interrupt:
				ta = latcha;			// Reload timer
				icr |= 1;				// Raise timer A interrupt
				ta_toggle = !ta_toggle;	// Toggle PB6 output

				if (cra & 8) {			// One-shot?
					cra &= 0xfe;		// Yes, stop timer
					new_cra &= 0xfe;
					ta_state = T_LOAD_THEN_STOP;	// Reload in next cycle
				} else {
					ta_state = T_LOAD_THEN_COUNT;	// No, delay one cycle (and reload)
				}
			}

			ta_output |= 1;
		}
	}

	// Delayed write to CRA?
ta_idle:
	if (has_new_cra) {
		switch (ta_state) {
			case T_STOP:
			case T_LOAD_THEN_STOP:
				if (new_cra & 1) {			// Timer started, wasn't running
					ta_toggle = true;		// Starting the timer resets the toggle bit
					if (new_cra & 0x10) {	// Force load
						ta_state = T_LOAD_THEN_WAIT_THEN_COUNT;
					} else {				// No force load
						ta_state = T_WAIT_THEN_COUNT;
					}
				} else {					// Timer stopped, was already stopped
					if (new_cra & 0x10) {	// Force load
						ta_state = T_LOAD_THEN_STOP;
					}
				}
				break;
			case T_COUNT:
				if (new_cra & 1) {			// Timer started, was already running
					if (new_cra & 0x10) {	// Force load
						ta_state = T_LOAD_THEN_WAIT_THEN_COUNT;
					}
				} else {					// Timer stopped, was running
					if (new_cra & 0x10) {	// Force load
						ta_state = T_LOAD_THEN_STOP;
					} else if ((cra & 0x20) == 0) {	// No force load
						ta_state = T_COUNT_THEN_STOP;
					} else {
						ta_state = T_STOP;
					}
				}
				break;
			case T_LOAD_THEN_COUNT:
			case T_WAIT_THEN_COUNT:
				if (new_cra & 1) {
					if (new_cra & 8) {		// One-shot?
						new_cra &= 0xfe;	// Yes, stop timer
						ta_state = T_STOP;
					} else if (new_cra & 0x10) {	// Force load
						ta_state = T_LOAD_THEN_WAIT_THEN_COUNT;
					}
				} else {
					ta_state = T_STOP;
				}
				break;
		}
		cra = new_cra & 0xef;	// Clear force load
		has_new_cra = false;
	}

	// Timer B state machine
	switch (tb_state) {
		case T_WAIT_THEN_COUNT:
			tb_state = T_COUNT;		// fall through
		case T_STOP:
			goto tb_idle;
		case T_LOAD_THEN_STOP:
			tb = latchb;			// Reload timer
			tb_state = T_STOP;
			goto tb_idle;
		case T_LOAD_THEN_COUNT:
			tb = latchb;			// Reload timer
			tb_state = T_COUNT;
			goto tb_idle;
		case T_LOAD_THEN_WAIT_THEN_COUNT:
			tb_state = T_WAIT_THEN_COUNT;
			tb = latchb;			// Reload timer
			goto tb_idle;
		case T_COUNT:
			goto tb_count;
		case T_COUNT_THEN_STOP:
			tb_state = T_STOP;
			goto tb_count;
	}

	// Count timer B
tb_count:
	if (tb_cnt_phi2 || tb_cnt_ta) {
		if (tb != 0) {	// Decrement timer if != 0
			if (tb_cnt_phi2 || (ta_output & 4)) {	// Cascaded mode takes two cycles to count
				--tb;
			}
		}
		if (tb == 0) {	// Timer expired?
			if (tb_state != T_STOP) {
tb_interrupt:
				if (!tb_cnt_ta || (ta_output & 0x0c) == 0) {	// Cascaded mode takes two cycles to reset while tb == 0
					tb = latchb;			// Reload timer
					icr |= 2;				// Raise timer B interrupt
					tb_toggle = !tb_toggle;	// Toggle PB7 output

					if (crb & 8) {			// One-shot?
						crb &= 0xfe;		// Yes, stop timer
						new_crb &= 0xfe;
						tb_state = T_LOAD_THEN_STOP;	// Reload in next cycle
					} else {
						tb_state = T_LOAD_THEN_COUNT;	// No, delay one cycle (and reload)
					}
				}
			}

			tb_output |= 1;
		}
	}

	// Delayed write to CRB?
tb_idle:
	if (has_new_crb) {
		switch (tb_state) {
			case T_STOP:
			case T_LOAD_THEN_STOP:
				if (new_crb & 1) {			// Timer started, wasn't running
					tb_toggle = true;		// Starting the timer resets the toggle bit
					if (new_crb & 0x10) {	// Force load
						tb_state = T_LOAD_THEN_WAIT_THEN_COUNT;
					} else {				// No force load
						tb_state = T_WAIT_THEN_COUNT;
					}
				} else {					// Timer stopped, was already stopped
					if (new_crb & 0x10) {	// Force load
						tb_state = T_LOAD_THEN_STOP;
					}
				}
				break;
			case T_COUNT:
				if (new_crb & 1) {			// Timer started, was already running
					if (new_crb & 0x10) {	// Force load
						tb_state = T_LOAD_THEN_WAIT_THEN_COUNT;
					}
				} else {					// Timer stopped, was running
					if (new_crb & 0x10) {	// Force load
						tb_state = T_LOAD_THEN_STOP;
					} else if (tb_cnt_phi2) {	// No force load
						tb_state = T_COUNT_THEN_STOP;
					} else {
						tb_state = T_STOP;
					}
				}
				break;
			case T_LOAD_THEN_COUNT:
			case T_WAIT_THEN_COUNT:
				if (new_crb & 1) {
					if (new_crb & 8) {		// One-shot?
						new_crb &= 0xfe;	// Yes, stop timer
						tb_state = T_STOP;
					} else if (new_crb & 0x10) {	// Force load
						tb_state = T_LOAD_THEN_WAIT_THEN_COUNT;
					}
				} else {
					tb_state = T_STOP;
				}
				break;
		}
		crb = new_crb & 0xef;	// Clear force load
		has_new_crb = false;
		tb_cnt_phi2 = ((crb & 0x60) == 0x00);
		tb_cnt_ta = ((crb & 0x40) == 0x40);	// Ignore CNT, which is pulled high
	}

	// Update IRQ status
	if (icr & int_mask) {
		irq_delay |= 1;
	}
	if (irq_delay & 2) {	// One cycle of IRQ delay
		if ((icr & 0x80) == 0) {
			icr |= 0x80;
			trigger_irq();
		}
	} else {
		clear_irq();
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
