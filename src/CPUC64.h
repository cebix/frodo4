/*
 *  CPUC64.h - 6510 (C64) emulation (line based)
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

#ifndef _CPU_C64_H
#define _CPU_C64_H

#include "C64.h"


// Set this to 1 if the 6502 PC should be represented by a real pointer
#ifndef FRODO_SC
#ifndef PC_IS_POINTER
#define PC_IS_POINTER 1
#endif
#endif

// Set this to 1 for more precise CPU cycle calculation
#ifndef PRECISE_CPU_CYCLES
#define PRECISE_CPU_CYCLES 0
#endif

// Set this to 1 for instruction-aligned CIA emulation
#ifndef PRECISE_CIA_CYCLES
#define PRECISE_CIA_CYCLES 0
#endif


// Interrupt types
enum {
	INT_VICIRQ,
	INT_CIAIRQ,
	INT_NMI
	// INT_RESET (private)
};


class MOS6569;
class MOS6581;
class MOS6526_1;
class MOS6526_2;
class REU;
class IEC;
struct MOS6510State;


// 6510 emulation (C64)
class MOS6510 {
public:
	MOS6510(C64 *c64, uint8_t *Ram, uint8_t *Basic, uint8_t *Kernal, uint8_t *Char, uint8_t *Color);

#ifdef FRODO_SC
	void EmulateCycle();				// Emulate one clock cycle
#else
	int EmulateLine(int cycles_left);	// Emulate until cycles_left underflows
#endif
	void Reset();
	void AsyncReset();					// Reset the CPU asynchronously
	void AsyncNMI();					// Raise NMI asynchronously (NMI pulse)
	void GetState(MOS6510State *s) const;
	void SetState(const MOS6510State *s);
	uint8_t ExtReadByte(uint16_t adr);
	void ExtWriteByte(uint16_t adr, uint8_t byte);
	uint8_t REUReadByte(uint16_t adr);
	void REUWriteByte(uint16_t adr, uint8_t byte);

	void TriggerVICIRQ();
	void ClearVICIRQ();
	void TriggerCIAIRQ();
	void ClearCIAIRQ();
	void TriggerNMI();
	void ClearNMI();

	int ExtConfig;		// Memory configuration for ExtRead/WriteByte (0..7)

	MOS6569 *TheVIC;	// Pointer to VIC
	MOS6581 *TheSID;	// Pointer to SID
	MOS6526_1 *TheCIA1;	// Pointer to CIA 1
	MOS6526_2 *TheCIA2;	// Pointer to CIA 2
	REU *TheREU;		// Pointer to REU
	IEC *TheIEC;		// Pointer to drive array

#ifdef FRODO_SC
	bool BALow;			// BA line for Frodo SC
#endif

private:
	uint8_t read_byte(uint16_t adr);
	uint8_t read_byte_io(uint16_t adr);
	uint16_t read_word(uint16_t adr);
	void write_byte(uint16_t adr, uint8_t byte);
	void write_byte_io(uint16_t adr, uint8_t byte);

	uint8_t read_zp(uint16_t adr);
	uint16_t read_zp_word(uint16_t adr);
	void write_zp(uint16_t adr, uint8_t byte);

	void new_config();
	void illegal_op(uint8_t op, uint16_t at);
	void illegal_jump(uint16_t at, uint16_t to);

	void do_adc(uint8_t byte);
	void do_sbc(uint8_t byte);

	uint8_t read_emulator_id(uint16_t adr);

	C64 *the_c64;			// Pointer to C64 object

	uint8_t *ram;			// Pointer to main RAM
	uint8_t *basic_rom, *kernal_rom, *char_rom, *color_ram; // Pointers to ROMs and color RAM

	union {					// Pending interrupts
		uint8_t intr[4];	// Index: See definitions above
		unsigned long intr_any;
	} interrupt;
	bool nmi_state;			// State of NMI line

	uint8_t n_flag, z_flag;
	bool v_flag, d_flag, i_flag, c_flag;
	uint8_t a, x, y, sp;

#if PC_IS_POINTER
	uint8_t *pc, *pc_base;
#else
	uint16_t pc;
#endif

#ifdef FRODO_SC
	uint32_t first_irq_cycle, first_nmi_cycle;

	enum {
		OPFLAG_IRQ_DISABLED = 0x01,
		OPFLAG_IRQ_ENABLED = 0x02,
	};
	uint8_t opflags;			// Interrupt operation flags

	uint8_t state, op;			// Current state and opcode
	uint16_t ar, ar2;			// Address registers
	uint8_t rdbuf;				// Data buffer for RMW instructions
	uint8_t ddr, pr, pr_out;	// Processor port
#else
	int	borrowed_cycles;		// Borrowed cycles from next line
#endif

	bool basic_in, kernal_in, char_in, io_in;
	uint8_t dfff_byte;
};

// 6510 state
struct MOS6510State {
	uint8_t a, x, y;
	uint8_t p;					// Processor flags
	uint16_t pc, sp;
	uint8_t ddr, pr, pr_out;	// Port
	uint8_t intr[4];			// Interrupt state
	bool nmi_state;	
	uint8_t dfff_byte;
								// Frodo SC:
	bool instruction_complete;
	uint8_t opflags;
	uint32_t first_irq_cycle;
	uint32_t first_nmi_cycle;
};


// Interrupt functions
#ifdef FRODO_SC
inline void MOS6510::TriggerVICIRQ()
{
	if (!(interrupt.intr[INT_VICIRQ] || interrupt.intr[INT_CIAIRQ])) {
		first_irq_cycle = the_c64->CycleCounter();
	}
	interrupt.intr[INT_VICIRQ] = true;
}

inline void MOS6510::TriggerCIAIRQ()
{
	if (!(interrupt.intr[INT_VICIRQ] || interrupt.intr[INT_CIAIRQ])) {
		first_irq_cycle = the_c64->CycleCounter();
	}
	interrupt.intr[INT_CIAIRQ] = true;
}

inline void MOS6510::TriggerNMI()
{
	if (!nmi_state) {
		nmi_state = true;
		interrupt.intr[INT_NMI] = true;
		first_nmi_cycle = the_c64->CycleCounter();
	}
}
#else
inline void MOS6510::TriggerVICIRQ()
{
	interrupt.intr[INT_VICIRQ] = true;
}

inline void MOS6510::TriggerCIAIRQ()
{
	interrupt.intr[INT_CIAIRQ] = true;
}

inline void MOS6510::TriggerNMI()
{
	if (!nmi_state) {
		nmi_state = true;
		interrupt.intr[INT_NMI] = true;
	}
}
#endif
inline void MOS6510::ClearVICIRQ()
{
	interrupt.intr[INT_VICIRQ] = false;
}

inline void MOS6510::ClearCIAIRQ()
{
	interrupt.intr[INT_CIAIRQ] = false;
}

inline void MOS6510::ClearNMI()
{
	nmi_state = false;
}

#endif
