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
	MOS6526(MOS6510 * cpu) : the_cpu(cpu) { }
	virtual ~MOS6526() { }

	void Reset();

	void GetState(MOS6526State * s) const;
	virtual void SetState(const MOS6526State * s);

#ifdef FRODO_SC
	void EmulateCycle();
#else
	void EmulateLine(int cycles);
#endif
	void CountTOD();

	void SetPAIn(uint8_t byte) { pa_in = byte; }
	void SetPBIn(uint8_t byte) { pb_in = byte; }
	uint8_t PAOut() const { return pra | ~ddra; }
	uint8_t PBOut() const { return prb | ~ddrb; }

protected:
	uint8_t read_register(uint8_t reg);
	void write_register(uint8_t reg, uint8_t byte);

	void set_int_flag(uint8_t flag);
	virtual void trigger_irq() = 0;
	virtual void clear_irq() = 0;

	uint8_t timer_on_pb(uint8_t prb);

	MOS6510 * the_cpu;	// Pointer to CPU object

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

	// Input lines for ports
	uint8_t pa_in = 0;
	uint8_t pb_in = 0;

#ifdef FRODO_SC
	bool ta_int_next_cycle,		// Flag: Trigger Timer A interrupt in next cycle
		 tb_int_next_cycle,		// Flag: Trigger Timer B interrupt in next cycle
		 has_new_cra,			// Flag: New value for CRA pending
		 has_new_crb,			// Flag: New value for CRB pending
		 ta_toggle,				// Timer A output to PB6 toggle state
		 tb_toggle;				// Timer B output to PB7 toggle state
	char ta_state, tb_state;	// Timer A/B states
	uint8_t new_cra, new_crb;	// New values for CRA/CRB
	uint8_t ta_output;			// Shift register for previous TA output states
#endif
};


class MOS6526_1 : public MOS6526 {
public:
	MOS6526_1(MOS6510 * cpu, MOS6569 * vic) : MOS6526(cpu), the_vic(vic) { }

	void Reset();

	uint8_t ReadRegister(uint8_t reg);
	void WriteRegister(uint8_t reg, uint8_t byte);

	uint8_t KeyMatrix[8];	// C64 keyboard matrix, 1 bit/key (0: key down, 1: key up)
	uint8_t RevMatrix[8];	// Reversed keyboard matrix

	uint8_t Joystick1;		// Joystick 1 AND value
	uint8_t Joystick2;		// Joystick 2 AND value

private:
	void trigger_irq() override;
	void clear_irq() override;

	void check_lp();

	MOS6569 * the_vic;		// Pointer to VIC object

	uint8_t prev_lp;		// Previous state of LP line (bit 4)
};


class MOS6526_2 : public MOS6526{
public:
	MOS6526_2(MOS6510 * cpu, MOS6569 * vic, MOS6502_1541 * cpu_1541) : MOS6526(cpu), the_vic(vic), the_cpu_1541(cpu_1541) { }

	void Reset();

	void SetState(const MOS6526State * s) override;

	uint8_t ReadRegister(uint8_t reg);
	void WriteRegister(uint8_t reg, uint8_t byte);

	uint8_t IECLines;		// State of IEC lines from C64 side
							// (bit 5 - DATA, bit 4 - CLK, bit 3 - ATN)
							// Wire-AND with 1541 state to obtain physical line state

private:
	void trigger_irq() override;
	void clear_irq() override;

	void write_pa(uint8_t inv_out);

	MOS6569 * the_vic;				// Pointer to VIC object
	MOS6502_1541 * the_cpu_1541;	// Pointer to 1541 CPU object
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
	uint8_t int_flags;	// Pending interrupts
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
	bool ta_int_next_cycle;
	bool tb_int_next_cycle;
	bool has_new_cra;
	bool has_new_crb;
	bool ta_toggle;
	bool tb_toggle;
	char ta_state;
	char tb_state;
	uint8_t new_cra;
	uint8_t new_crb;
	uint8_t ta_output;
};


/*
 *  Set interrupt flag
 */

inline void MOS6526::set_int_flag(uint8_t flag)
{
	icr |= flag;
	if (int_mask & flag) {
		icr |= 0x80;
		trigger_irq();
	}
}


/*
 *  Read from CIA register
 */

inline uint8_t MOS6526::read_register(uint8_t reg)
{
	switch (reg) {
		case 0:
			return (pra & ddra) | (pa_in & ~ddra);
		case 1: {
			uint8_t ret = (prb & ddrb) | (pb_in & ~ddrb);

			if ((cra | crb) & 0x02) {	// TA/TB output to PB enabled?
				ret = timer_on_pb(ret);
			}

			return ret;
		}
		case 2:
			return ddra;
		case 3:
			return ddrb;

		case 4:
			return ta;
		case 5:
			return ta >> 8;
		case 6:
			return tb;
		case 7:
			return tb >> 8;

		case 8:
			tod_halt = false;
			return tod_10ths;
		case 9:
			return tod_sec;
		case 10:
			return tod_min;
		case 11:
			tod_halt = true;
			return tod_hr;

		case 12:
			return sdr;

		case 13: {
			uint8_t ret = icr; // Read and clear ICR
			icr = 0;
#ifdef FRODO_SC
			ta_int_next_cycle = false;
			tb_int_next_cycle = false;
#endif
			clear_irq();
			return ret;
		}

		case 14:
			return cra;
		case 15:
			return crb;

		default:	// Can't happen
			return 0;
	}
}


/*
 *  Write to CIA register
 */

inline void MOS6526::write_register(uint8_t reg, uint8_t byte)
{
	switch (reg) {
		case 0:
			pra = byte;
			break;
		case 1:
			prb = byte;
			break;
		case 2:
			ddra = byte;
			break;
		case 3:
			ddrb = byte;
			break;

		case 4:
			latcha = (latcha & 0xff00) | byte;
			break;
		case 5:
			latcha = (latcha & 0xff) | (byte << 8);
			if (!(cra & 1)) {	// Reload timer if stopped
				ta = latcha;
			}
			break;
		case 6:
			latchb = (latchb & 0xff00) | byte;
			break;
		case 7:
			latchb = (latchb & 0xff) | (byte << 8);
			if (!(crb & 1)) {	// Reload timer if stopped
				tb = latchb;
			}
			break;

		case 8:
			if (crb & 0x80) {
				alm_10ths = byte & 0x0f;
			} else {
				tod_10ths = byte & 0x0f;
			}
			break;
		case 9:
			if (crb & 0x80) {
				alm_sec = byte & 0x7f;
			} else {
				tod_sec = byte & 0x7f;
			}
			break;
		case 10:
			if (crb & 0x80) {
				alm_min = byte & 0x7f;
			} else {
				tod_min = byte & 0x7f;
			}
			break;
		case 11:
			if (crb & 0x80) {
				alm_hr = byte & 0x9f;
			} else {
				tod_hr = byte & 0x9f;
			}
			break;

		case 12:
			sdr = byte;
			set_int_flag(8);	// Fake SDR interrupt for programs that need it
			break;

		case 13:
#ifndef FRODO_SC
			if (ThePrefs.CIAIRQHack) {	// Hack for addressing modes that read from the address
				icr = 0;
			}
#endif
			if (byte & 0x80) {
				int_mask |= byte & 0x1f;
				if (icr & int_mask & 0x1f) { // Trigger IRQ if pending
					icr |= 0x80;
					trigger_irq();
				}
			} else {
				int_mask &= ~(byte & 0x1f);
			}
			break;

		case 14:
#ifdef FRODO_SC
			has_new_cra = true;		// Delay write by 1 cycle
			new_cra = byte;
#else
			cra = byte & 0xef;
			if (byte & 0x10) { // Force load
				ta = latcha;
			}
			ta_cnt_phi2 = ((byte & 0x21) == 0x01);
#endif
			break;

		case 15:
#ifdef FRODO_SC
			has_new_crb = true;		// Delay write by 1 cycle
			new_crb = byte;
#else
			crb = byte & 0xef;
			if (byte & 0x10) { // Force load
				tb = latchb;
			}
			tb_cnt_phi2 = ((byte & 0x61) == 0x01);
			tb_cnt_ta = ((byte & 0x41) == 0x41);	// Ignore CNT, which is pulled high
#endif
			break;
	}
}

#endif
