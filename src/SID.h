/*
 *  SID.h - 6581 emulation
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

#ifndef _SID_H
#define _SID_H

#include "Prefs.h"

#include <stdlib.h>


// Define this if you want an emulation of an 8580
// (affects combined waveforms)
#undef EMUL_MOS8580


class C64;
class SIDRenderer;
struct MOS6581State;

// Class for administrative functions
class MOS6581 {
public:
	MOS6581(C64 *c64);
	~MOS6581();

	void Reset();
	uint8_t ReadRegister(uint16_t adr);
	void WriteRegister(uint16_t adr, uint8_t byte);
	void NewPrefs(const Prefs *prefs);
	void PauseSound();
	void ResumeSound();
	void GetState(MOS6581State *ss) const;
	void SetState(const MOS6581State *ss);
	void EmulateLine();

private:
	void open_close_renderer(int old_type, int new_type);
	uint8_t read_osc3() const;

	C64 *the_c64;				// Pointer to C64 object
	SIDRenderer *the_renderer;	// Pointer to current renderer

	uint8_t regs[32];			// Copies of the 25 write-only SID registers
	uint8_t last_sid_byte;		// Last value written to SID
	uint32_t fake_v3_count;		// Fake voice 3 phase accumulator for oscillator read-back
};


// Renderers do the actual audio data processing
class SIDRenderer {
public:
	virtual ~SIDRenderer() {}
	virtual void Reset()=0;
	virtual void EmulateLine()=0;
	virtual void WriteRegister(uint16_t adr, uint8_t byte)=0;
	virtual void NewPrefs(const Prefs *prefs)=0;
	virtual void Pause()=0;
	virtual void Resume()=0;
};


// SID state
struct MOS6581State {
	uint8_t freq_lo_1;
	uint8_t freq_hi_1;
	uint8_t pw_lo_1;
	uint8_t pw_hi_1;
	uint8_t ctrl_1;
	uint8_t AD_1;
	uint8_t SR_1;

	uint8_t freq_lo_2;
	uint8_t freq_hi_2;
	uint8_t pw_lo_2;
	uint8_t pw_hi_2;
	uint8_t ctrl_2;
	uint8_t AD_2;
	uint8_t SR_2;

	uint8_t freq_lo_3;
	uint8_t freq_hi_3;
	uint8_t pw_lo_3;
	uint8_t pw_hi_3;
	uint8_t ctrl_3;
	uint8_t AD_3;
	uint8_t SR_3;

	uint8_t fc_lo;
	uint8_t fc_hi;
	uint8_t res_filt;
	uint8_t mode_vol;

	uint8_t pot_x;
	uint8_t pot_y;
	uint8_t osc_3;
	uint8_t env_3;

	uint32_t v3_count;
};


/*
 * Fill buffer (for Unix sound routines), sample volume (for sampled voice)
 */

inline void MOS6581::EmulateLine()
{
	// Simulate voice 3 phase accumulator
	uint8_t v3_ctrl = regs[0x12];	// Voice 3 control register
	if (v3_ctrl & 0x08) {			// Test bit
		fake_v3_count = 0;
	} else {
		uint32_t add = (regs[0x0f] << 8) | regs[0x0e];
		fake_v3_count = (fake_v3_count + add * ThePrefs.NormalCycles) & 0xffffff;
	}

	if (the_renderer != NULL) {
		the_renderer->EmulateLine();
	}
}


/*
 *  Read from register
 */

inline uint8_t MOS6581::ReadRegister(uint16_t adr)
{
	// A/D converters are not implemented
	if (adr == 0x19 || adr == 0x1a) {
		last_sid_byte = 0;
		return 0xff;
	}

	// Voice 3 oscillator read-back
	if (adr == 0x1b) {
		last_sid_byte = 0;
		return read_osc3();
	}

	// TODO: Voice 3 EG read-back is not implemented
	if (adr == 0x1c) {
		last_sid_byte = 0;
		return rand();
	}

	// Write-only register: Return last value written to SID
	return last_sid_byte;
}


/*
 *  Write to register
 */

inline void MOS6581::WriteRegister(uint16_t adr, uint8_t byte)
{
	// Keep a local copy of the register values
	last_sid_byte = regs[adr] = byte;

	if (the_renderer != NULL) {
		the_renderer->WriteRegister(adr, byte);
	}
}

#endif
