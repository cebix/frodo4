/*
 *  SID.cpp - 6581 emulation
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
 * Incompatibilities:
 * ------------------
 *
 *  - Lots of empirically determined constants in the filter calculations
 */

#include "sysdeps.h"

#include "SID.h"
#include "C64.h"
#include "main.h"
#include "VIC.h"
#include "Prefs.h"

#include <SDL_audio.h>

#include <math.h>


// Define to use fixed-point arithmetic for filter calculations
#undef USE_FIXPOINT_MATHS

#ifdef USE_FIXPOINT_MATHS

#define FIXPOINT_PREC 16	// Number of fractional bits used in fixpoint representation
#define ldSINTAB 9			// Size of sine table (0 to 90 degrees)
#include "FixPoint.h"

using filter_t = FixPoint;

#else

using filter_t = float;

#endif


/*
 *  Resonance frequency polynomials (6581)
 */

static inline float resonance_freq_lp(float f)
{
	return 227.755 - 1.7635 * f - 0.0176385 * f * f + 0.00333484 * f * f * f;
}

static inline float resonance_freq_hp(float f)
{
	return 366.374 - 14.0052 * f + 0.603212 * f * f - 0.000880196 * f * f * f;
}


/*
 *  Random number generator for noise waveform
 */

static uint8_t sid_random()
{
	static uint32_t seed = 1;
	seed = seed * 1103515245 + 12345;
	return seed >> 16;
}


/*
 *  Constructor
 */

MOS6581::MOS6581()
{
	the_renderer = nullptr;
	for (unsigned i = 0; i < 32; ++i) {
		regs[i] = 0;
	}

	// Open the renderer
	open_close_renderer(SIDTYPE_NONE, ThePrefs.SIDType);
}


/*
 *  Destructor
 */

MOS6581::~MOS6581()
{
	// Close the renderer
	open_close_renderer(ThePrefs.SIDType, SIDTYPE_NONE);
}


/*
 *  Reset the SID
 */

void MOS6581::Reset()
{
	for (unsigned i = 0; i < 32; ++i) {
		regs[i] = 0;
	}

	last_sid_byte = 0;
	last_sid_seq = 0;

	fake_v3_update_cycle = 0;
	fake_v3_count = 0x555555;
	fake_v3_eg_level = 0;
	fake_v3_eg_state = EG_RELEASE;

	// Reset the renderer
	if (the_renderer != nullptr) {
		the_renderer->Reset();
	}
}


/*
 *  Preferences may have changed
 */

void MOS6581::NewPrefs(const Prefs *prefs)
{
	open_close_renderer(ThePrefs.SIDType, prefs->SIDType);
	if (the_renderer != nullptr) {
		the_renderer->NewPrefs(prefs);
	}
}


/*
 *  Pause sound output
 */

void MOS6581::PauseSound()
{
	if (the_renderer != nullptr) {
		the_renderer->Pause();
	}
}


/*
 *  Resume sound output
 */

void MOS6581::ResumeSound()
{
	if (the_renderer != nullptr) {
		the_renderer->Resume();
	}
}


/*
 *  Simulate oscillator 3 for read-back
 */

void MOS6581::update_osc3()
{
	uint32_t now = TheC64->CycleCounter();

	uint8_t v3_ctrl = regs[0x12];	// Voice 3 control register
	if (v3_ctrl & 0x08) {			// Test bit
		fake_v3_count = 0;
	} else {
		uint32_t elapsed = now - fake_v3_update_cycle;
		uint32_t add = (regs[0x0f] << 8) | regs[0x0e];
		fake_v3_count = (fake_v3_count + add * elapsed) & 0xffffff;
	}

	fake_v3_update_cycle = now;
}


/*
 *  Oscillator 3 read-back
 */

uint8_t MOS6581::read_osc3()
{
	update_osc3();

	uint8_t v3_ctrl = regs[0x12];   // Voice 3 control register
	if (v3_ctrl & 0x10) {			// Triangle wave
		// TODO: Ring modulation from voice 2
		if (fake_v3_count & 0x800000) {
			return (fake_v3_count >> 15) ^ 0xff;
		} else {
			return fake_v3_count >> 15;
		}
	} else if (v3_ctrl & 0x20) {	// Sawtooth wave
		return fake_v3_count >> 16;
	} else if (v3_ctrl & 0x40) {	// Rectangle wave
		uint32_t pw = ((regs[0x11] & 0x0f) << 8) | regs[0x10];
		if (fake_v3_count > (pw << 12)) {
			return 0xff;
		} else {
			return 0x00;
		}
	} else if (v3_ctrl & 0x80) {	// Noise wave
		return sid_random();
	} else {
		// TODO: Combined waveforms
		return 0;
	}
}


/*
 *  EG 3 read-back
 */

uint8_t MOS6581::read_env3() const
{
	return (uint8_t)(fake_v3_eg_level >> 16);
}


/*
 *  Tables (sampled from a 6581R4AR)
 */

// Number of cycles for each SID data bus leakage step
uint16_t MOS6581::sid_leakage_cycles[9] = {
	0, 0xa300, 0x3b00, 0x2280, 0x0400, 0x1280, 0x1a80, 0x3a00, 0x0080
};

// Bit mask for each SID data bus leakage step
uint8_t MOS6581::sid_leakage_mask[9] = {
	0, 0x7f, 0xfb, 0xf7, 0xfd, 0xbf, 0xdf, 0xef, 0xfe
};


/*
 *  Get SID state
 */

void MOS6581::GetState(MOS6581State * s) const
{
	s->freq_lo_1 = regs[0];
	s->freq_hi_1 = regs[1];
	s->pw_lo_1 = regs[2];
	s->pw_hi_1 = regs[3];
	s->ctrl_1 = regs[4];
	s->AD_1 = regs[5];
	s->SR_1 = regs[6];

	s->freq_lo_2 = regs[7];
	s->freq_hi_2 = regs[8];
	s->pw_lo_2 = regs[9];
	s->pw_hi_2 = regs[10];
	s->ctrl_2 = regs[11];
	s->AD_2 = regs[12];
	s->SR_2 = regs[13];

	s->freq_lo_3 = regs[14];
	s->freq_hi_3 = regs[15];
	s->pw_lo_3 = regs[16];
	s->pw_hi_3 = regs[17];
	s->ctrl_3 = regs[18];
	s->AD_3 = regs[19];
	s->SR_3 = regs[20];

	s->fc_lo = regs[21];
	s->fc_hi = regs[22];
	s->res_filt = regs[23];
	s->mode_vol = regs[24];

	s->pot_x = 0xff;
	s->pot_y = 0xff;

	s->v3_update_cycle = fake_v3_update_cycle;
	s->v3_count = fake_v3_count;
	s->v3_eg_level = fake_v3_eg_level;
	s->v3_eg_state = fake_v3_eg_state;

	s->last_sid_cycles = last_sid_cycles;
	s->last_sid_seq = last_sid_seq;
	s->last_sid_byte = last_sid_byte;
}


/*
 *  Restore SID state
 */

void MOS6581::SetState(const MOS6581State * s)
{
	regs[0] = s->freq_lo_1;
	regs[1] = s->freq_hi_1;
	regs[2] = s->pw_lo_1;
	regs[3] = s->pw_hi_1;
	regs[4] = s->ctrl_1;
	regs[5] = s->AD_1;
	regs[6] = s->SR_1;

	regs[7] = s->freq_lo_2;
	regs[8] = s->freq_hi_2;
	regs[9] = s->pw_lo_2;
	regs[10] = s->pw_hi_2;
	regs[11] = s->ctrl_2;
	regs[12] = s->AD_2;
	regs[13] = s->SR_2;

	regs[14] = s->freq_lo_3;
	regs[15] = s->freq_hi_3;
	regs[16] = s->pw_lo_3;
	regs[17] = s->pw_hi_3;
	regs[18] = s->ctrl_3;
	regs[19] = s->AD_3;
	regs[20] = s->SR_3;

	regs[21] = s->fc_lo;
	regs[22] = s->fc_hi;
	regs[23] = s->res_filt;
	regs[24] = s->mode_vol;

	fake_v3_update_cycle = s->v3_update_cycle;
	fake_v3_count = s->v3_count;
	fake_v3_eg_level = s->v3_eg_level;
	fake_v3_eg_state = s->v3_eg_state;

	last_sid_cycles = s->last_sid_cycles;
	last_sid_seq = s->last_sid_seq;
	last_sid_byte = s->last_sid_byte;

	// Stuff the new register values into the renderer
	if (the_renderer != nullptr) {
		for (unsigned i = 0; i < 25; ++i) {
			the_renderer->WriteRegister(i, regs[i]);
		}
	}
}


/**
 **  Renderer for digital SID emulation (SIDTYPE_DIGITAL_*)
 **/

constexpr int SAMPLE_FREQ = 48000;			// Desired default sample frequency (note: obtained freq may be different!)
constexpr uint32_t SID_FREQ = 985248;		// SID frequency in Hz
constexpr uint32_t CALC_FREQ = 50;			// Frequency at which calc_buffer is called in Hz (should be 50Hz)
constexpr size_t SAMPLE_BUF_SIZE = TOTAL_RASTERS * 2;	// Size of buffer for sampled voice (double buffered)


// SID waveforms (some of them :-)
enum {
	WAVE_NONE,
	WAVE_TRI,
	WAVE_SAW,
	WAVE_TRISAW,
	WAVE_RECT,
	WAVE_TRIRECT,
	WAVE_SAWRECT,
	WAVE_TRISAWRECT,
	WAVE_NOISE
};

// Filter types
enum {
	FILT_NONE,
	FILT_LP,
	FILT_BP,
	FILT_LPBP,
	FILT_HP,
	FILT_NOTCH,
	FILT_HPBP,
	FILT_ALL
};


// Structure for one voice
struct DRVoice {
	int wave;			// Selected waveform
	int eg_state;		// Current state of EG
	DRVoice *mod_by;	// Voice that modulates this one
	DRVoice *mod_to;	// Voice that is modulated by this one

	uint32_t count;		// Phase accumulator for waveform generator, 8.16 fixed
	uint32_t add;		// Added to accumulator in every sample frame

	uint16_t freq;		// SID frequency value
	uint16_t pw;		// SID pulse-width value

	int32_t a_add;		// EG parameters
	int32_t d_sub;
	int32_t s_level;
	int32_t r_sub;
	int32_t eg_level;	// Current EG level, 8.16 fixed

	uint32_t noise;		// Last noise generator output value

	bool gate;			// EG gate bit
	bool ring;			// Ring modulation bit
	bool test;			// Test bit

						// The following bit is set for the modulating
						// voice, not for the modulated one (as the SID bits)
	bool sync;			// Sync modulation bit
	bool mute;			// Voice muted (voice 3 only)
};


// Renderer class
class DigitalRenderer : public SIDRenderer {
public:
	DigitalRenderer();
	virtual ~DigitalRenderer();

	void Reset() override;
	void EmulateLine() override;
	void WriteRegister(uint16_t adr, uint8_t byte) override;
	void NewPrefs(const Prefs *prefs) override;
	void Pause() override;
	void Resume() override;

private:
	void set_wave_table(int sid_type);
	void set_ffreq_table(int sid_type);

	void calc_filter();
	void calc_buffer(int16_t *buf, long count);

	bool ready;						// Flag: Renderer has initialized and is ready

	uint8_t volume;					// Master volume
	uint8_t res_filt;				// RES/FILT register

	uint32_t sid_cycles_frac;		// Number of SID cycles per output sample frame (16.16)
#ifdef USE_FIXPOINT_MATHS
	filter_t sidquot;
#endif

	const uint16_t * TriSawTable = nullptr;
	const uint16_t * TriRectTable = nullptr;
	const uint16_t * SawRectTable = nullptr;
	const uint16_t * TriSawRectTable = nullptr;

	static const uint16_t TriSawTable_6581[0x100];
	static const uint16_t TriRectTable_6581[0x100];
	static const uint16_t SawRectTable_6581[0x100];
	static const uint16_t TriSawRectTable_6581[0x100];

	static const uint16_t TriSawTable_8580[0x100];
	static const uint16_t TriRectTable_8580[0x100];
	static const uint16_t SawRectTable_8580[0x100];
	static const uint16_t TriSawRectTable_8580[0x100];

	DRVoice voice[3];				// Data for 3 voices

	uint8_t f_type;					// Filter type
	uint8_t f_freq;					// SID filter frequency (upper 8 bits)
	uint8_t f_res;					// Filter resonance (0..15)
	filter_t f_ampl;				// IIR filter input attenuation
	filter_t d1, d2, g1, g2;		// IIR filter coefficients
	filter_t f_ampl_eff;			// Smoothed filter parameters
	filter_t d1_eff, d2_eff;
	filter_t g1_eff, g2_eff;
	filter_t xn1, xn2, yn1, yn2;	// IIR filter previous input/output signal
	filter_t ffreq_LP[256];			// Precomputed filter resonance frequencies
	filter_t ffreq_HP[256];

	filter_t out_lp_g;				// IIR filter coefficients for external output
	filter_t out_hp_d, out_hp_g;
	filter_t audio_out_lp;			// IIR filter previous output signals
	filter_t audio_out_lp1;
	filter_t audio_out_hp;

	uint8_t sample_vol[SAMPLE_BUF_SIZE]; 		// Sampled master volume setting (Impossible Mission, Ghostbusters, Arkanoid, ...)
	uint8_t sample_res_filt[SAMPLE_BUF_SIZE];	// Sampled RES/FILT register (Space Taxi)
	unsigned sample_in_ptr;			// Index in sample buffers for writing

	static void buffer_proc(void * userdata, uint8_t * buffer, int size);
	SDL_AudioDeviceID device_id;	// SDL audio device ID
	SDL_AudioSpec obtained;			// Obtained output format
};


// Combined waveforms sampled from a 6581R4
const uint16_t DigitalRenderer::TriSawTable_6581[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0808,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1010, 0x3C3C,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0808,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1010, 0x3C3C
};

const uint16_t DigitalRenderer::TriRectTable_6581[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080, 0xC0C0,
	0x0000, 0x8080, 0x8080, 0xE0E0, 0x8080, 0xE0E0, 0xF0F0, 0xFCFC,
	0xFFFF, 0xFCFC, 0xFAFA, 0xF0F0, 0xF6F6, 0xE0E0, 0xE0E0, 0x8080,
	0xEEEE, 0xE0E0, 0xE0E0, 0x8080, 0xC0C0, 0x0000, 0x0000, 0x0000,
	0xDEDE, 0xC0C0, 0xC0C0, 0x0000, 0x8080, 0x0000, 0x0000, 0x0000,
	0x8080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0xBEBE, 0x8080, 0x8080, 0x0000, 0x8080, 0x0000, 0x0000, 0x0000,
	0x8080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x7E7E, 0x4040, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

const uint16_t DigitalRenderer::SawRectTable_6581[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7878,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7878
};

const uint16_t DigitalRenderer::TriSawRectTable_6581[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};


// Combined waveforms sampled from an 8580R5
const uint16_t DigitalRenderer::TriSawTable_8580[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0808,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1818, 0x3C3C,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1C1C,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x8080, 0x0000, 0x8080, 0x8080,
	0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xE0E0,
	0xF0F0, 0xF0F0, 0xF0F0, 0xF0F0, 0xF8F8, 0xF8F8, 0xFCFC, 0xFEFE
};

const uint16_t DigitalRenderer::TriRectTable_8580[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0xFFFF, 0xFCFC, 0xF8F8, 0xF0F0, 0xF4F4, 0xF0F0, 0xF0F0, 0xE0E0,
	0xECEC, 0xE0E0, 0xE0E0, 0xC0C0, 0xE0E0, 0xC0C0, 0xC0C0, 0xC0C0,
	0xDCDC, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0x8080, 0x8080,
	0xC0C0, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x0000, 0x0000,
	0xBEBE, 0xA0A0, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x0000,
	0x8080, 0x8080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x8080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x7E7E, 0x7070, 0x6060, 0x0000, 0x4040, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

const uint16_t DigitalRenderer::SawRectTable_8580[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080, 0x8080,
	0x0000, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xB0B0, 0xBEBE,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080,
	0x0000, 0x0000, 0x0000, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0,
	0x0000, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0,
	0x8080, 0x8080, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xDCDC,
	0x8080, 0x8080, 0x8080, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0,
	0xC0C0, 0xC0C0, 0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xE0E0, 0xECEC,
	0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xE0E0, 0xF0F0, 0xF0F0, 0xF4F4,
	0xF0F0, 0xF0F0, 0xF8F8, 0xF8F8, 0xF8F8, 0xFCFC, 0xFEFE, 0xFFFF
};

const uint16_t DigitalRenderer::TriSawRectTable_8580[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080, 0x8080,
	0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0, 0xC0C0,
	0xC0C0, 0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xF0F0, 0xF8F8, 0xFCFC
};


/*
 *  Set waveform tables according to SID tyupe
 */

void DigitalRenderer::set_wave_table(int sid_type)
{
	if (sid_type == SIDTYPE_DIGITAL_6581) {
		TriSawTable     = TriSawTable_6581;
		TriRectTable    = TriRectTable_6581;
		SawRectTable    = SawRectTable_6581;
		TriSawRectTable = TriSawRectTable_6581;
	} else {
		TriSawTable     = TriSawTable_8580;
		TriRectTable    = TriRectTable_8580;
		SawRectTable    = SawRectTable_8580;
		TriSawRectTable = TriSawRectTable_8580;
	}
}


/*
 *  Compute filter resonance frequencies according to SID type
 */

void DigitalRenderer::set_ffreq_table(int sid_type)
{
	// Not actually a function of SID type per se, but the 8580 is usually
	// used with 2200 pF filter caps (vs. 470 pF on the 6581), so the
	// resonance frequencies of the 8580 are lower
	float c;
	if (sid_type == SIDTYPE_DIGITAL_6581) {
		c = 1.0;
	} else {
		c = 470.0 / 2200.0;
	}

	for (unsigned f = 0; f < 256; ++f) {
		ffreq_LP[f] = filter_t(resonance_freq_lp(f) * c);
		ffreq_HP[f] = filter_t(resonance_freq_hp(f) * c);
	}
}


const int16_t MOS6581::EGDivTable[16] = {
	9, 32,
	63, 95,
	149, 220,
	267, 313,
	392, 977,
	1954, 3126,
	3906, 11720,
	19531, 31251
};

const uint8_t MOS6581::EGDRShift[256] = {
	5,5,5,5,5,5,5,5,4,4,4,4,4,4,4,4,
	3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};


/*
 *  Constructor
 */

DigitalRenderer::DigitalRenderer()
{
#ifdef USE_FIXPOINT_MATHS
	InitFixSinTab();
#endif

	// Link voices together
	voice[0].mod_by = &voice[2];
	voice[1].mod_by = &voice[0];
	voice[2].mod_by = &voice[1];
	voice[0].mod_to = &voice[1];
	voice[1].mod_to = &voice[2];
	voice[2].mod_to = &voice[0];

	// Set waveform tables
	set_wave_table(ThePrefs.SIDType);

	// Precompute filter resonance frequencies
	set_ffreq_table(ThePrefs.SIDType);

	// Reset SID
	Reset();

	SDL_AudioSpec desired;
	SDL_zero(desired);
	SDL_zero(obtained);

	// Set up desired output format
	desired.freq = SAMPLE_FREQ;
	desired.format = AUDIO_S16SYS;
	desired.channels = 1;
	desired.samples = 256;
	desired.callback = buffer_proc;
	desired.userdata = this;

	// Open output device
	device_id = SDL_OpenAudioDevice(NULL, false, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
	if (device_id == 0) {
		fprintf(stderr, "WARNING: Cannot open audio: %s\n", SDL_GetError());
		return;
	}

	// Calculate number of SID cycles per sample frame
	sid_cycles_frac = uint32_t(float(SID_FREQ) / obtained.freq * 65536.0);

	// AUDIO OUT of SID is connected to 16 kHz (R = 10 kΩ, C = 1 nF)
	// low-pass and 16 Hz (R = 1 kΩ, C = 10 µF) high-pass RC filters
	float wc_lp = 1.0 / (obtained.freq * 10000.0 * 1.0E-9);
	float wc_hp = 1.0 / (obtained.freq * 1000.0 * 10.0E-6);

	out_lp_g = filter_t(1 / (1 + wc_lp));
	out_hp_g = filter_t(1 - wc_hp);				// Approximation of (1-sin(wc))/cos(wc) for small wc
	out_hp_d = filter_t((1 + out_hp_g) / 2);

	// Start sound output
	Resume();

	// Ready for action
	ready = true;
}


/*
 *  Destructor
 */

DigitalRenderer::~DigitalRenderer()
{
	if (device_id) {
		SDL_CloseAudioDevice(device_id);
	}
}


/*
 *  Reset emulation
 */

void DigitalRenderer::Reset()
{
	volume = 0;
	res_filt = 0;

	for (unsigned v = 0; v < 3; ++v) {
		voice[v].wave = WAVE_NONE;
		voice[v].eg_state = EG_RELEASE;
		voice[v].count = 0x555555;
		voice[v].add = 0;
		voice[v].freq = voice[v].pw = 0;
		voice[v].eg_level = voice[v].s_level = 0;
		voice[v].a_add = voice[v].d_sub = voice[v].r_sub = sid_cycles_frac / MOS6581::EGDivTable[0];
		voice[v].gate = voice[v].ring = voice[v].test = false;
		voice[v].sync = voice[v].mute = false;
	}

	f_type = FILT_NONE;
	f_freq = f_res = 0;
	f_ampl = f_ampl_eff = filter_t(0.5);
	d1 = d2 = g1 = g2 = filter_t(0.0);
	d1_eff = d2_eff = g1_eff = g2_eff = filter_t(0.0);
	xn1 = xn2 = yn1 = yn2 = filter_t(0.0);

	audio_out_lp = audio_out_lp1 = audio_out_hp = filter_t(0.0);

	sample_in_ptr = 0;
	memset(sample_vol, 0, SAMPLE_BUF_SIZE);
	memset(sample_res_filt, 0, SAMPLE_BUF_SIZE);
}


/*
 *  Pause sound output
 */

void DigitalRenderer::Pause()
{
	if (device_id) {
		SDL_PauseAudioDevice(device_id, true);
	}
}


/*
 *  Resume sound output
 */

void DigitalRenderer::Resume()
{
	if (device_id) {
		SDL_PauseAudioDevice(device_id, false);
	}
}


/*
 *  Sample current volume setting once per raster line (for sampled voice)
 */

void DigitalRenderer::EmulateLine()
{
	// Record registers for sample playback
	sample_vol[sample_in_ptr] = volume;
	sample_res_filt[sample_in_ptr] = res_filt;
	sample_in_ptr = (sample_in_ptr + 1) % SAMPLE_BUF_SIZE;
}


/*
 *  Write to register
 */

void DigitalRenderer::WriteRegister(uint16_t adr, uint8_t byte)
{
	if (!ready)
		return;

	unsigned v = adr / 7;	// Voice number

	switch (adr) {
		case 0:
		case 7:
		case 14:
			voice[v].freq = (voice[v].freq & 0xff00) | byte;
#ifdef USE_FIXPOINT_MATHS
			voice[v].add = intmult(sid_cycles_frac, voice[v].freq);
#else
			voice[v].add = (uint32_t)(float(voice[v].freq) / obtained.freq * SID_FREQ);
#endif
			break;

		case 1:
		case 8:
		case 15:
			voice[v].freq = (voice[v].freq & 0xff) | (byte << 8);
#ifdef USE_FIXPOINT_MATHS
			voice[v].add = intmult(sid_cycles_frac, voice[v].freq);
#else
			voice[v].add = (uint32_t)(float(voice[v].freq) / obtained.freq * SID_FREQ);
#endif
			break;

		case 2:
		case 9:
		case 16:
			voice[v].pw = (voice[v].pw & 0x0f00) | byte;
			break;

		case 3:
		case 10:
		case 17:
			voice[v].pw = (voice[v].pw & 0xff) | ((byte & 0xf) << 8);
			break;

		case 4:
		case 11:
		case 18:
			voice[v].wave = (byte >> 4) & 0xf;
			if ((byte & 1) != voice[v].gate) {
				if (byte & 1) {		// Gate turned on
					voice[v].eg_state = EG_ATTACK;
				} else {			// Gate turned off
					voice[v].eg_state = EG_RELEASE;
				}
			}
			voice[v].gate = byte & 1;
			voice[v].mod_by->sync = byte & 2;
			voice[v].ring = byte & 4;
			if ((voice[v].test = byte & 8) != 0) {
				voice[v].count = 0;
			}
			break;

		case 5:
		case 12:
		case 19:
			voice[v].a_add = sid_cycles_frac / MOS6581::EGDivTable[byte >> 4];
			voice[v].d_sub = sid_cycles_frac / MOS6581::EGDivTable[byte & 0xf];
			break;

		case 6:
		case 13:
		case 20:
			voice[v].s_level = (byte >> 4) * 0x111111;
			voice[v].r_sub = sid_cycles_frac / MOS6581::EGDivTable[byte & 0xf];
			break;

		case 22:
			f_freq = byte;
			break;

		case 23:
			res_filt = byte;
			f_res = byte >> 4;
			break;

		case 24:
			volume = byte & 0xf;
			voice[2].mute = byte & 0x80;

			f_type = (byte >> 4) & 7;
			break;
	}
}


/*
 *  Preferences may have changed
 */

void DigitalRenderer::NewPrefs(const Prefs *prefs)
{
	set_wave_table(prefs->SIDType);
	set_ffreq_table(prefs->SIDType);
	calc_filter();
}


/*
 *  Calculate IIR filter coefficients
 */

void DigitalRenderer::calc_filter()
{
	if (! ThePrefs.SIDFilters)
		return;

	// Filter off? Then reset all coefficients
	if (f_type == FILT_NONE) {
		d1 = filter_t(0.0); d2 = filter_t(0.0);
		g1 = filter_t(0.0); g2 = filter_t(0.0);
		f_ampl = filter_t(0.0);
		return;
	}

	filter_t fr, arg;

	// Calculate resonance frequency
	if (f_type == FILT_LP || f_type == FILT_LPBP) {
		fr = ffreq_LP[f_freq];
	} else {
		fr = ffreq_HP[f_freq];
	}

	// Limit to <1/2 sample frequency, avoid div by 0 in case FILT_BP/FILT_NOTCH below
#ifdef USE_FIXPOINT_MATHS
	arg = fr / (obtained.freq >> 1);
#else
	arg = fr / (float(obtained.freq) * 0.5);
#endif

	if (arg > filter_t(0.99)) {
		arg = filter_t(0.99);
	}
	if (arg < filter_t(0.01)) {
		arg = filter_t(0.01);
	}

	// Calculate poles (resonance frequency and resonance)
	//
	// The (complex) poles are at
	//   zp_1/2 = (-g1 +/- sqrt(g1^2 - 4*g2)) / 2
	g2 = filter_t(0.55) + filter_t(1.2) * arg * (arg - 1) + filter_t(f_res) * filter_t(0.0133333333);
#ifdef USE_FIXPOINT_MATHS
	g1 = filter_t(-2) * fixsqrt(g2) * fixcos(arg);
#else
	g1 = filter_t(-2.0) * sqrt(g2) * cos(M_PI * arg);
#endif

	// Increase resonance if LP/HP combined with BP
	if (f_type == FILT_LPBP || f_type == FILT_HPBP) {
		g2 += filter_t(0.1);
	}

	// Stabilize filter
#ifdef USE_FIXPOINT_MATHS
	if (g1.abs() >= g2 + filter_t(1.0)) {
#else
	if (fabs(g1) >= g2 + filter_t(1.0)) {
#endif
		if (g1 > 0) {
			g1 = g2 + filter_t(0.99);
		} else {
			g1 = -(g2 + filter_t(0.99));
		}
	}

	// Calculate roots (filter characteristic) and input attenuation
	//
	// The (complex) roots are at
	//   z0_1/2 = (-d1 +/- sqrt(d1^2 - 4*d2)) / 2
	switch (f_type) {

		case FILT_LPBP:
		case FILT_LP:		// Both roots at -1, H(1)=1
			d1 = filter_t(2.0); d2 = filter_t(1.0);
			f_ampl = filter_t(0.25) * (filter_t(1.0) + g1 + g2);
			break;

		case FILT_HPBP:
		case FILT_HP:		// Both roots at 1, H(-1)=1
			d1 = filter_t(-2.0); d2 = filter_t(1.0);
			f_ampl = filter_t(0.25) * (filter_t(1.0) - g1 + g2);
			break;

		case FILT_BP: {		// Roots at +1 and -1, H_max=1
			d1 = filter_t(0.0); d2 = filter_t(-1.0);
#ifdef USE_FIXPOINT_MATHS
			filter_t c = fixsqrt(g2*g2 + filter_t(2.0)*g2 - g1*g1 + filter_t(1.0));
#else
			filter_t c = sqrt(g2*g2 + filter_t(2.0)*g2 - g1*g1 + filter_t(1.0));
#endif
			f_ampl = filter_t(0.25) * (filter_t(-2.0)*g2*g2 - (filter_t(4.0)+filter_t(2.0)*c)*g2 - filter_t(2.0)*c + (c+filter_t(2.0))*g1*g1 - filter_t(2.0)) / (-g2*g2 - (c+filter_t(2.0))*g2 - c + g1*g1 - filter_t(1.0));
			break;
		}

		case FILT_NOTCH: {	// Roots at exp(i*pi*arg) and exp(-i*pi*arg), H(1)=1 (arg>=0.5) or H(-1)=1 (arg<0.5)
#ifdef USE_FIXPOINT_MATHS
			filter_t ca = fixcos(arg);
#else
			filter_t ca = cos(M_PI * arg);
#endif
			d1 = filter_t(-2.0) * ca; d2 = filter_t(1.0);
			if (arg >= filter_t(0.5)) {
				f_ampl = filter_t(0.5) * (filter_t(1.0) + g1 + g2) / (filter_t(1.0) - ca);
			} else {
				f_ampl = filter_t(0.5) * (filter_t(1.0) - g1 + g2) / (filter_t(1.0) + ca);
			}
			break;
		}

		// TODO: This is pure guesswork...
		case FILT_ALL: {	// Roots at 2*exp(i*pi*arg) and 2*exp(-i*pi*arg), H(-1)=1 (arg>=0.5) or H(1)=1 (arg<0.5)
#ifdef USE_FIXPOINT_MATHS
			filter_t ca = fixcos(arg);
#else
			filter_t ca = cos(M_PI * arg);
#endif
			d1 = filter_t(-4.0) * ca; d2 = filter_t(4.0);
			if (arg >= filter_t(0.5)) {
				f_ampl = (filter_t(1.0) - g1 + g2) / (filter_t(5.0) + filter_t(4.0) * ca);
			} else {
				f_ampl = (filter_t(1.0) + g1 + g2) / (filter_t(5.0) - filter_t(4.0) * ca);
			}
			break;
		}

		default:
			break;
	}
}


/*
 *  Fill one audio buffer with calculated SID sound
 */

void DigitalRenderer::calc_buffer(int16_t *buf, long count)
{
	// Index in sample buffer for reading, 16.16 fixed
	uint32_t sample_count = (sample_in_ptr + SAMPLE_BUF_SIZE/2) << 16;

	// Output DC offset
	int32_t dc_offset = (ThePrefs.SIDType == SIDTYPE_DIGITAL_6581) ? 0x800000 : 0x100000;

	count >>= 1;	// 16 bit mono output, count is in bytes
	while (count--) {

		// Get current master volume and RES/FILT setting from sample buffers
		uint8_t master_volume = sample_vol[(sample_count >> 16) % SAMPLE_BUF_SIZE];
		uint8_t res_filt = sample_res_filt[(sample_count >> 16) % SAMPLE_BUF_SIZE];
		sample_count += ((TOTAL_RASTERS * SCREEN_FREQ) << 16) / obtained.freq;

		int32_t sum_output = 0;
		int32_t sum_output_filter = 0;

		// Loop for all three voices
		for (unsigned j = 0; j < 3; ++j) {
			DRVoice *v = &voice[j];

			// Envelope generator
			uint16_t envelope;

			switch (v->eg_state) {
				case EG_ATTACK:
					v->eg_level += v->a_add;
					if (v->eg_level > 0xffffff) {
						v->eg_level = 0xffffff;
						v->eg_state = EG_DECAY_SUSTAIN;
					}
					break;
				case EG_DECAY_SUSTAIN:
					v->eg_level -= v->d_sub >> MOS6581::EGDRShift[v->eg_level >> 16];
					if (v->eg_level < v->s_level) {
						v->eg_level = v->s_level;
					}
					break;
				case EG_RELEASE:
					v->eg_level -= v->r_sub >> MOS6581::EGDRShift[v->eg_level >> 16];
					if (v->eg_level < 0) {
						v->eg_level = 0;
					}
					break;
			}
			envelope = v->eg_level >> 16;

			// Waveform generator
			uint16_t output;

			if (!v->test) {
				v->count += v->add;
			}

			if (v->sync && (v->count > 0x1000000)) {
				v->mod_to->count = 0;
			}

			v->count &= 0xffffff;

			switch (v->wave) {
				case WAVE_TRI: {
					uint32_t ctrl = v->count;
					if (v->ring) {
						ctrl ^= v->mod_by->count;
					}
					if (ctrl & 0x800000) {
						output = (v->count >> 7) ^ 0xffff;
					} else {
						output = v->count >> 7;
					}
					break;
				}
				case WAVE_SAW:
					output = v->count >> 8;
					break;
				case WAVE_RECT:
					if (v->test) {
						output = 0xffff;
					} else if (v->count >= (uint32_t)(v->pw << 12)) {
						output = 0xffff;
					} else {
						output = 0;
					}
					break;
				case WAVE_TRISAW:
					output = TriSawTable[v->count >> 16];
					break;
				case WAVE_TRIRECT:
					if (v->count >= (uint32_t)(v->pw << 12)) {
						output = TriRectTable[v->count >> 16];
					} else {
						output = 0;
					}
					break;
				case WAVE_SAWRECT:
					if (v->count >= (uint32_t)(v->pw << 12)) {
						output = SawRectTable[v->count >> 16];
					} else {
						output = 0;
					}
					break;
				case WAVE_TRISAWRECT:
					if (v->count >= (uint32_t)(v->pw << 12)) {
						output = TriSawRectTable[v->count >> 16];
					} else {
						output = 0;
					}
					break;
				case WAVE_NOISE:
					if (v->count > 0x100000) {
						output = v->noise = sid_random() << 8;
						v->count &= 0xfffff;
					} else {
						output = v->noise;
					}
					break;
				default:
					output = 0x8000;
					break;
			}

			// Route voice through filter if selected
			if (res_filt & (1 << j)) {
				sum_output_filter += (int16_t)(output ^ 0x8000) * envelope;
			} else if (!v->mute) {
				sum_output += (int16_t)(output ^ 0x8000) * envelope;
			}
		}

		// SID-internal filters
		if (ThePrefs.SIDFilters) {
			f_ampl_eff = f_ampl_eff * filter_t(0.8) + f_ampl * filter_t(0.2);	// Smooth out filter parameter transitions
			d1_eff     = d1_eff     * filter_t(0.8) + d1     * filter_t(0.2);
			d2_eff     = d2_eff     * filter_t(0.8) + d2     * filter_t(0.2);
			g1_eff     = g1_eff     * filter_t(0.8) + g1     * filter_t(0.2);
			g2_eff     = g2_eff     * filter_t(0.8) + g2     * filter_t(0.2);

#ifdef USE_FIXPOINT_MATHS
			int32_t xn = f_ampl_eff.imul(sum_output_filter);
			int32_t yn = xn + d1_eff.imul(xn1) + d2_eff.imul(xn2) - g1_eff.imul(yn1) - g2_eff.imul(yn2);
			yn2 = yn1; yn1 = yn; xn2 = xn1; xn1 = xn;
			sum_output_filter = yn;
#else
			filter_t xn = filter_t(sum_output_filter) * f_ampl_eff;
			filter_t yn = xn + d1_eff * xn1 + d2_eff * xn2 - g1_eff * yn1 - g2_eff * yn2;
			yn2 = yn1; yn1 = yn; xn2 = xn1; xn1 = xn;
			sum_output_filter = (int32_t) yn;
#endif
		}

		// External filter on AUDIO OUT
		int32_t ext_output = (sum_output - sum_output_filter + dc_offset) * master_volume;

#ifdef USE_FIXPOINT_MATHS
		ext_output >>= 14;
#else
		if (ThePrefs.SIDFilters) {
			filter_t audio_out = filter_t(ext_output) / (1 << 14);
			audio_out_lp = out_lp_g * audio_out_lp + (1 - out_lp_g) * audio_out;
			audio_out_hp = out_hp_g * audio_out_hp + out_hp_d * (audio_out_lp - audio_out_lp1);
			audio_out_lp1 = audio_out_lp;
			ext_output = (int32_t) audio_out_hp;
		} else {
			ext_output >>= 14;
		}
#endif

		// Write to buffer
		if (ext_output > 0x7fff) {	// Using filters can cause minor clipping
			ext_output = 0x7fff;
		} else if (ext_output < -0x8000) {
			ext_output = -0x8000;
		}
		*buf++ = ext_output;
	}
}


/*
 *  Audio callback function 
 */

void DigitalRenderer::buffer_proc(void * userdata, uint8_t * buffer, int size)
{
	DigitalRenderer * renderer = (DigitalRenderer *) userdata;

	renderer->calc_filter();
	renderer->calc_buffer((int16_t *) buffer, size);
}


#ifdef __linux__
#include "SID_catweasel.h"
#endif


/*
 *  Open/close the renderer, according to old and new prefs
 */

static bool is_digital(int sid_type)
{
	return sid_type == SIDTYPE_DIGITAL_6581 || sid_type == SIDTYPE_DIGITAL_8580;
}

void MOS6581::open_close_renderer(int old_type, int new_type)
{
	if (old_type == new_type || is_digital(old_type) == is_digital(new_type))
		return;

	// Delete the old renderer
	delete the_renderer;

	// Create new renderer
	if (new_type == SIDTYPE_DIGITAL_6581 || new_type == SIDTYPE_DIGITAL_8580) {
		the_renderer = new DigitalRenderer;
#ifdef __linux__
	} else if (new_type == SIDTYPE_SIDCARD) {
		the_renderer = new CatweaselRenderer;
#endif
	} else {
		the_renderer = nullptr;
	}

	// Stuff the current register values into the new renderer
	if (the_renderer != nullptr) {
		for (unsigned i = 0; i < 25; ++i) {
			the_renderer->WriteRegister(i, regs[i]);
		}
	}
}
