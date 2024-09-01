/*
 *  CIA.h - 6526 emulation
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

#ifndef _CIA_H
#define _CIA_H

#include "Prefs.h"


class MOS6510;
class MOS6502_1541;
class MOS6569;
struct MOS6526State;


class MOS6526 {
public:
	MOS6526(MOS6510 *CPU);

	void Reset();
	void GetState(MOS6526State *cs);
	void SetState(const MOS6526State *cs);
#ifdef FRODO_SC
	void CheckIRQs();
	void EmulateCycle();
#else
	void EmulateLine(int cycles);
#endif
	void CountTOD();
	virtual void TriggerInterrupt(int bit)=0;

protected:
	MOS6510 *the_cpu;	// Pointer to 6510

	uint8_t pra, prb, ddra, ddrb;

	uint16_t ta, tb, latcha, latchb;

	uint8_t tod_10ths, tod_sec, tod_min, tod_hr;
	uint8_t alm_10ths, alm_sec, alm_min, alm_hr;

	uint8_t sdr, icr, cra, crb;
	uint8_t int_mask;

	int tod_divider;	// TOD frequency divider

	bool tod_halt,		// Flag: TOD halted
		 ta_cnt_phi2,	// Flag: Timer A is counting Phi 2
		 tb_cnt_phi2,	// Flag: Timer B is counting Phi 2
	     tb_cnt_ta;		// Flag: Timer B is counting underflows of Timer A

#ifdef FRODO_SC
	uint8_t timer_on_pb(uint8_t prb);

	bool ta_irq_next_cycle,		// Flag: Trigger TA IRQ in next cycle
		 tb_irq_next_cycle,		// Flag: Trigger TB IRQ in next cycle
		 has_new_cra,			// Flag: New value for CRA pending
		 has_new_crb,			// Flag: New value for CRB pending
		 ta_toggle,				// TA output to PB6 toggle state
		 tb_toggle;				// TB output to PB7 toggle state
	char ta_state, tb_state;	// Timer A/B states
	uint8_t new_cra, new_crb;	// New values for CRA/CRB
#endif
};


class MOS6526_1 : public MOS6526 {
public:
	MOS6526_1(MOS6510 *CPU, MOS6569 *VIC);

	void Reset();
	uint8_t ReadRegister(uint16_t adr);
	void WriteRegister(uint16_t adr, uint8_t byte);
	virtual void TriggerInterrupt(int bit);

	uint8_t KeyMatrix[8];	// C64 keyboard matrix, 1 bit/key (0: key down, 1: key up)
	uint8_t RevMatrix[8];	// Reversed keyboard matrix

	uint8_t Joystick1;		// Joystick 1 AND value
	uint8_t Joystick2;		// Joystick 2 AND value

private:
	void check_lp();

	MOS6569 *the_vic;

	uint8_t prev_lp;		// Previous state of LP line (bit 4)
};


class MOS6526_2 : public MOS6526{
public:
	MOS6526_2(MOS6510 *CPU, MOS6569 *VIC, MOS6502_1541 *CPU1541);

	void Reset();
	uint8_t ReadRegister(uint16_t adr);
	void WriteRegister(uint16_t adr, uint8_t byte);
	virtual void TriggerInterrupt(int bit);

	uint8_t IECLines;		// State of IEC lines (bit 7 - DATA, bit 6 - CLK, bit 4 - ATN)

private:
	MOS6569 *the_vic;
	MOS6502_1541 *the_cpu_1541;
};


// Timer states
enum {
	T_STOP,
	T_WAIT_THEN_COUNT,
	T_LOAD_THEN_STOP,
	T_LOAD_THEN_COUNT,
	T_LOAD_THEN_WAIT_THEN_COUNT,
	T_COUNT,
	T_COUNT_THEN_STOP
};

// CIA state
struct MOS6526State {
	uint8_t pra;
	uint8_t ddra;
	uint8_t prb;
	uint8_t ddrb;
	uint8_t ta_lo;
	uint8_t ta_hi;
	uint8_t tb_lo;
	uint8_t tb_hi;
	uint8_t tod_10ths;
	uint8_t tod_sec;
	uint8_t tod_min;
	uint8_t tod_hr;
	uint8_t sdr;
	uint8_t int_data;	// Pending interrupts
	uint8_t cra;
	uint8_t crb;
						// Additional registers
	uint16_t latcha;	// Timer latches
	uint16_t latchb;
	uint8_t alm_10ths;	// Alarm time
	uint8_t alm_sec;
	uint8_t alm_min;
	uint8_t alm_hr;
	uint8_t int_mask;	// Enabled interrupts

						// FrodoSC:
	bool ta_irq_next_cycle;
	bool tb_irq_next_cycle;
	bool has_new_cra;
	bool has_new_crb;
	bool ta_toggle;
	bool tb_toggle;
	char ta_state;
	char tb_state;
	uint8_t new_cra;
	uint8_t new_crb;
};


/*
 *  Emulate CIA for one cycle/raster line
 */

#ifdef FRODO_SC
inline void MOS6526::CheckIRQs()
{
	// Trigger pending interrupts
	if (ta_irq_next_cycle) {
		ta_irq_next_cycle = false;
		TriggerInterrupt(1);
	}
	if (tb_irq_next_cycle) {
		tb_irq_next_cycle = false;
		TriggerInterrupt(2);
	}
}
#else
inline void MOS6526::EmulateLine(int cycles)
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
			TriggerInterrupt(1);
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
			TriggerInterrupt(2);
		}
	}
}
#endif

#endif
