/*
 *  CPU1541.h - 6502 (1541) emulation (line based)
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

#ifndef _CPU_1541_H
#define _CPU_1541_H

#include "CIA.h"
#include "C64.h"


// Set this to 1 for more precise CPU cycle calculation
#ifndef PRECISE_CPU_CYCLES
#define PRECISE_CPU_CYCLES 0
#endif


// Interrupt types
enum {
	INT_VIA1IRQ,
	INT_VIA2IRQ,
	INT_RESET1541
};


class C64;
class Job1541;
class C64Display;
class MOS6502_1541;
struct MOS6502State;
struct MOS6522State;


// 6522 emulation (VIA)
class MOS6522 {
public:
	MOS6522(MOS6502_1541 * cpu, unsigned irq) : the_cpu(cpu), irq_type(irq) { }
	~MOS6522() { }

	void Reset();

	void GetState(MOS6522State * s) const;
	void SetState(const MOS6522State * s);

#ifdef FRODO_SC
	void EmulateCycle();			// Emulate one clock cycle
#else
	void CountTimers(int cycles);	// Emulate timers
#endif

	uint8_t ReadRegister(uint16_t adr);
	void WriteRegister(uint16_t adr, uint8_t byte);

	void SetPAIn(uint8_t byte) { pa_in = byte; }
	void SetPBIn(uint8_t byte) { pb_in = byte; }
	uint8_t PAOut() const { return pra | ~ddra; }
	uint8_t PBOut() const { return prb | ~ddrb; }

	uint8_t PCR() const { return pcr; }

	void TriggerCA1Interrupt();

private:
	void trigger_irq();
	void clear_irq(uint8_t flag);

	MOS6502_1541 * the_cpu;	// Pointer to CPU object

	unsigned irq_type;		// Which interrupt type to trigger

	// Registers
	uint8_t pra;
	uint8_t ddra;
	uint8_t prb;
	uint8_t ddrb;
	uint16_t t1c;	// T1 counter
	uint16_t t1l;	// T1 latch
	uint16_t t2c;	// T2 counter
	uint16_t t2l;	// T2 latch
	uint8_t sr;
	uint8_t acr;
	uint8_t pcr;
	uint8_t ifr;
	uint8_t ier;

	// Input lines for ports
	uint8_t pa_in = 0;
	uint8_t pb_in = 0;

	// Flags for timer one-shot control
	bool t1_irq_blocked;
	bool t2_irq_blocked;

							// Frodo SC:
	uint8_t t1_load_delay;	// Delay line for T1 reload
	uint8_t t2_load_delay;	// Delay line for T2 reload
	uint8_t t2_input_delay;	// Delay line for T2 counter input
	uint8_t irq_delay;		// Delay line for IRQ assertion
};


// 6502 emulation (1541)
class MOS6502_1541 {
public:
	MOS6502_1541(C64 *c64, Job1541 *job, C64Display *disp, uint8_t *Ram, uint8_t *Rom);
	~MOS6502_1541();

#ifdef FRODO_SC
	void EmulateCPUCycle();				// Emulate one clock cycle
	void EmulateVIACycle();
#else
	int EmulateLine(int cycles_left);	// Emulate until cycles_left underflows
	void CountVIATimers(int cycles);
#endif
	void Reset();
	void AsyncReset();					// Reset the CPU asynchronously

	uint32_t CycleCounter() const { return cycle_counter; }

	void GetState(MOS6502State * s) const;
	void SetState(const MOS6502State * s);

	uint8_t ExtReadByte(uint16_t adr);
	void ExtWriteByte(uint16_t adr, uint8_t byte);

	void TriggerInterrupt(unsigned which);
	void ClearInterrupt(unsigned which) { interrupt.intr[which] = false; }

	void TriggerIECInterrupt();
	uint8_t CalcIECLines() const;

	MOS6526_2 *TheCIA2;		// Pointer to C64 CIA 2

	uint8_t IECLines;		// State of IEC lines from 1541 side
							// (bit 5 - DATA, bit 4 - CLK, bit 3 - ATN)
							// Wire-AND with C64 state to obtain physical line state

	bool Idle;				// true: 1541 is idle

private:
	uint8_t read_byte(uint16_t adr);
	uint8_t read_byte_via1(uint16_t adr);
	uint8_t read_byte_via2(uint16_t adr);
	uint16_t read_word(uint16_t adr);
	void write_byte(uint16_t adr, uint8_t byte);
	void write_byte_via1(uint16_t adr, uint8_t byte);
	void write_byte_via2(uint16_t adr, uint8_t byte);

	void set_iec_lines(uint8_t inv_out);
	bool set_overflow_enabled() const { return (via2->PCR() & 0x0e) == 0x0e; }	// CA2 high output

	uint8_t read_zp(uint16_t adr);
	uint16_t read_zp_word(uint16_t adr);
	void write_zp(uint16_t adr, uint8_t byte);

	void jump(uint16_t adr);
	void illegal_op(uint8_t op, uint16_t at);

	void do_adc(uint8_t byte);
	void do_sbc(uint8_t byte);

	uint8_t *ram;			// Pointer to main RAM
	uint8_t *rom;			// Pointer to ROM
	C64 *the_c64;			// Pointer to C64 object
	C64Display *the_display; // Pointer to C64 display object
	Job1541 *the_job;		// Pointer to 1541 job object

	uint32_t cycle_counter;

	union {					// Pending interrupts
		uint8_t intr[4];	// Index: See definitions above
		unsigned long intr_any;
	} interrupt;

	uint8_t n_flag, z_flag;
	bool v_flag, d_flag, i_flag, c_flag;
	uint8_t a, x, y, sp;
	uint16_t pc;

#ifdef FRODO_SC
	uint32_t first_irq_cycle;
	uint32_t first_nmi_cycle;	// Unused

	enum {
		OPFLAG_IRQ_DISABLED = 0x01,
		OPFLAG_IRQ_ENABLED = 0x02,
	};
	uint8_t opflags;		// Interrupt operation flags

	uint8_t state, op;		// Current state and opcode
	uint16_t ar, ar2;		// Address registers
	uint8_t rdbuf;			// Data buffer for RMW instructions
#else
	int borrowed_cycles;	// Borrowed cycles from next line
#endif

	uint8_t atn_ack;		// ATN acknowledge: 0x00 or 0x08 (XOR value for IECLines ATN)

	MOS6522 * via1 = nullptr;	// VIA 1 object
	MOS6522 * via2 = nullptr;	// VIA 2 object
};


// VIA state
struct MOS6522State {
	uint8_t pra;			// Registers
	uint8_t ddra;
	uint8_t prb;
	uint8_t ddrb;
	uint16_t t1c;
	uint16_t t1l;
	uint16_t t2c;
	uint16_t t2l;
	uint8_t sr;
	uint8_t acr;
	uint8_t pcr;
	uint8_t ifr;
	uint8_t ier;

	bool t1_irq_blocked;
	bool t2_irq_blocked;

							// Frodo SC:
	uint8_t t1_load_delay;	// Delay line for T1 reload
	uint8_t t2_load_delay;	// Delay line for T2 reload
	uint8_t t2_input_delay;	// Delay line for T2 counter input
	uint8_t irq_delay;		// Delay line for IRQ assertion
};


// 6502 and VIA state
struct MOS6502State {
	uint32_t cycle_counter;

	uint8_t a, x, y;
	uint8_t p;				// Processor flags
	uint16_t pc, sp;

	uint8_t intr[4];		// Interrupt state

	bool instruction_complete;
	bool idle;
	uint8_t opflags;

	MOS6522State via1;		// VIA 1
	MOS6522State via2;		// VIA 2
};


/*
 *  Clear VIA interrupt flag, deassert IRQ line if no interrupts are pending.
 */

inline void MOS6522::clear_irq(uint8_t flag)
{
	ifr &= ~flag;
	if ((ifr & ier & 0x7f) == 0) {
		ifr &= 0x7f;
#ifdef FRODO_SC
		irq_delay = 0;
#else
		the_cpu->ClearInterrupt(irq_type);
#endif
	}
}


/*
 *  Read from VIA register
 */

inline uint8_t MOS6522::ReadRegister(uint16_t adr)
{
	switch (adr & 0xf) {
		case 0:
			clear_irq(0x10);	// Clear CB1 interrupt
			return (prb & ddrb) | (pb_in & ~ddrb);
		case 1:
			clear_irq(0x02);	// Clear CA1 interrupt
			return (pra & ddra) | (pa_in & ~ddra);
		case 2:
			return ddrb;
		case 3:
			return ddra;
		case 4:
			clear_irq(0x40);	// Clear T1 interrupt
			return t1c;
		case 5:
			return t1c >> 8;
		case 6:
			return t1l;
		case 7:
			return t1l >> 8;
		case 8:
			clear_irq(0x20);	// Clear T2 interrupt
			return t2c;
		case 9:
			return t2c >> 8;
		case 10:
			return sr;
		case 11:
			return acr;
		case 12:
			return pcr;
		case 13:
			return ifr;
		case 14:
			return ier | 0x80;
		case 15:
			return (pra & ddra) | (pa_in & ~ddra);
		default:	// Can't happen
			return 0;
	}
}


/*
 *  Write to VIA register
 */

inline void MOS6522::WriteRegister(uint16_t adr, uint8_t byte)
{
	switch (adr & 0xf) {
		case 0:
			prb = byte;
			clear_irq(0x10);	// Clear CB1 interrupt
			break;
		case 1:
			pra = byte;
			clear_irq(0x02);	// Clear CA1 interrupt
			break;
		case 2:
			ddrb = byte;
			break;
		case 3:
			ddra = byte;
			break;
		case 4:
		case 6:
			t1l = (t1l & 0xff00) | byte;
			break;
		case 5:
			t1l = (t1l & 0xff) | (byte << 8);
#ifdef FRODO_SC
			t1_load_delay |= 1;	// Load in next cycle
#else
			t1c = t1l;			// Load immediately
#endif
			t1_irq_blocked = false;
			clear_irq(0x40);	// Clear T1 interrupt
			break;
		case 7:
			t1l = (t1l & 0xff) | (byte << 8);
			clear_irq(0x40);	// Clear T1 interrupt
			break;
		case 8:
			t2l = (t2l & 0xff00) | byte;
			break;
		case 9:
			t2l = (t2l & 0xff) | (byte << 8);
#ifdef FRODO_SC
			t2_load_delay |= 1;	// Load in next cycle
#else
			t2c = t2l;			// Load immediately
#endif
			t2_irq_blocked = false;
			clear_irq(0x20);	// Clear T2 interrupt
			break;
		case 10:
			sr = byte;
			break;
		case 11:
			acr = byte;
			break;
		case 12:
			pcr = byte;
			break;
		case 13:
			clear_irq(byte & 0x7f);
			break;
		case 14:
			if (byte & 0x80) {
				ier |= byte & 0x7f;
			} else {
				ier &= ~byte;
			}
			break;
		case 15:
			pra = byte;
			break;
	}
}

#endif
