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

#include <stdlib.h>


// Define this if you want an emulation of an 8580
// (affects combined waveforms)
#undef EMUL_MOS8580


class Prefs;
class C64;
class SIDRenderer;
struct MOS6581State;

// Class for administrative functions
class MOS6581 {
public:
	MOS6581(C64 *c64);
	~MOS6581();

	void Reset(void);
	uint8_t ReadRegister(uint16_t adr);
	void WriteRegister(uint16_t adr, uint8_t byte);
	void NewPrefs(const Prefs *prefs);
	void PauseSound(void);
	void ResumeSound(void);
	void GetState(MOS6581State *ss);
	void SetState(const MOS6581State *ss);
	void EmulateLine(void);

private:
	void open_close_renderer(int old_type, int new_type);

	C64 *the_c64;				// Pointer to C64 object
	SIDRenderer *the_renderer;	// Pointer to current renderer
	uint8_t regs[32];			// Copies of the 25 write-only SID registers
	uint8_t last_sid_byte;		// Last value written to SID
};


// Renderers do the actual audio data processing
class SIDRenderer {
public:
	virtual ~SIDRenderer() {}
	virtual void Reset(void)=0;
	virtual void EmulateLine(void)=0;
	virtual void WriteRegister(uint16_t adr, uint8_t byte)=0;
	virtual void NewPrefs(const Prefs *prefs)=0;
	virtual void Pause(void)=0;
	virtual void Resume(void)=0;
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
};


/*
 * Fill buffer (for Unix sound routines), sample volume (for sampled voice)
 */

inline void MOS6581::EmulateLine(void)
{
	if (the_renderer != NULL)
		the_renderer->EmulateLine();
}


/*
 *  Read from register
 */

inline uint8_t MOS6581::ReadRegister(uint16_t adr)
{
	// A/D converters
	if (adr == 0x19 || adr == 0x1a) {
		last_sid_byte = 0;
		return 0xff;
	}

	// Voice 3 oscillator/EG readout
	if (adr == 0x1b || adr == 0x1c) {
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

	if (the_renderer != NULL)
		the_renderer->WriteRegister(adr, byte);
}

#endif
