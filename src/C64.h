/*
 *  C64.h - Put the pieces together
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

#ifndef _C64_H
#define _C64_H

#ifdef __BEOS__
#include <KernelKit.h>
#endif

#ifdef HAVE_SDL
#include <SDL_joystick.h>
#include <SDL_gamecontroller.h>
#endif


// Sizes of memory areas
constexpr int C64_RAM_SIZE = 0x10000;
constexpr int COLOR_RAM_SIZE = 0x400;
constexpr int BASIC_ROM_SIZE = 0x2000;
constexpr int KERNAL_ROM_SIZE = 0x2000;
constexpr int CHAR_ROM_SIZE = 0x1000;
constexpr int DRIVE_RAM_SIZE = 0x800;
constexpr int DRIVE_ROM_SIZE = 0x4000;


// false: Frodo, true: FrodoSC
extern bool IsFrodoSC;


class Prefs;
class C64Display;
class MOS6510;
class MOS6569;
class MOS6581;
class MOS6526_1;
class MOS6526_2;
class IEC;
class REU;
class MOS6502_1541;
class Job1541;
struct Snapshot;


class C64 {
public:
	C64();
	~C64();

	void Run(void);
	void Quit(void);
	void Pause(void);
	void Resume(void);
	void Reset(void);
	void NMI(void);
	void VBlank(bool draw_frame);
	void NewPrefs(const Prefs *prefs);
	void PatchKernal(bool fast_reset, bool emul_1541_proc);
	void MakeSnapshot(Snapshot * s);
	void RestoreSnapshot(const Snapshot * s);
	bool SaveSnapshot(const char * filename);
	bool LoadSnapshot(const char * filename);

	uint8_t *RAM, *Basic, *Kernal,
	        *Char, *Color;		// C64
	uint8_t *RAM1541, *ROM1541;	// 1541

	C64Display *TheDisplay;

	MOS6510 *TheCPU;			// C64
	MOS6569 *TheVIC;
	MOS6581 *TheSID;
	MOS6526_1 *TheCIA1;
	MOS6526_2 *TheCIA2;
	IEC *TheIEC;
	REU *TheREU;

	MOS6502_1541 *TheCPU1541;	// 1541
	Job1541 *TheJob1541;

	uint32_t CycleCounter;		// Cycle counter for Frodo SC

private:
	void c64_ctor1(void);
	void c64_ctor2(void);
	void c64_dtor(void);
	void open_close_joysticks(int oldjoy1, int oldjoy2, int newjoy1, int newjoy2);
	uint8_t poll_joystick(int port);
	void thread_func(void);

	bool thread_running;		// Emulation thread is running
	bool quit_thyself;			// Emulation thread shall quit
	bool have_a_break;			// Emulation thread shall pause

	int joy_minx[2], joy_maxx[2], joy_miny[2], joy_maxy[2]; // For dynamic joystick calibration
	uint8_t joykey;				// Joystick keyboard emulation mask value

	uint8_t orig_kernal_1d84,	// Original contents of kernal locations $1d84 and $1d85
	        orig_kernal_1d85;	// (for undoing the Fast Reset patch)

#ifdef __BEOS__
public:
	void SoundSync(void);

private:
	static long thread_invoc(void *obj);
	void open_close_joystick(int port, int oldjoy, int newjoy);

	void *joy[2];				// Joystick objects (BJoystick or BDigitalPort)
	bool joy_geek_port[2];		// Flag: joystick on GeekPort?
	thread_id the_thread;
	sem_id pause_sem;
	sem_id sound_sync_sem;
	bigtime_t start_time;
#endif

#ifdef HAVE_SDL
	void open_close_joystick(int port, int oldjoy, int newjoy);

	SDL_Joystick * joy[2] = { nullptr, nullptr };
	SDL_GameController * controller[2] = { nullptr, nullptr };
#endif

#ifdef WIN32
private:
	void CheckTimerChange();
	void StartTimer();
	void StopTimer();
	static void CALLBACK StaticTimeProc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);
	void TimeProc(UINT id);
#ifdef FRODO_SC
	void EmulateCyclesWith1541();
	void EmulateCyclesWithout1541();
#endif

	DWORD ref_time;				// when frame count was reset
	int skipped_frames;			// number of skipped frames
	int timer_every;			// frequency of timer in frames
	HANDLE timer_semaphore;		// Timer semaphore for synch
	MMRESULT timer_id;			// Timer identifier
	int frame;					// current frame number
	uint8_t joy_state;			// Current state of joystick
	bool state_change;
#endif
};


#endif
