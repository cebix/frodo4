/*
 *  C64.cpp - Put the pieces together
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

#include "sysdeps.h"

#include "C64.h"
#include "1541gcr.h"
#include "CIA.h"
#include "CPUC64.h"
#include "CPU1541.h"
#include "Display.h"
#include "IEC.h"
#include "main.h"
#include "Prefs.h"
#include "REU.h"
#include "SID.h"
#include "VIC.h"

#include <SDL.h>

#include <chrono>
#include <memory>
#include <thread>
#include <utility>
namespace chrono = std::chrono;


#ifdef FRODO_SC
bool IsFrodoSC = true;
#else
bool IsFrodoSC = false;
#endif


// Snapshot magic header
#define SNAPSHOT_HEADER "FrodoSnapshot4\0\0"

// Snapshot flags
#define SNAPSHOT_FLAG_1541_PROC 1

// Snapshot data structure
struct Snapshot {
	uint8_t magic[16];
	uint16_t flags;

	char drive8Path[256];

	uint8_t ram[C64_RAM_SIZE];
	uint8_t color[COLOR_RAM_SIZE];

	uint8_t driveRam[DRIVE_RAM_SIZE];

	uint32_t cycleCounter;

	MOS6510State cpu;
	MOS6569State vic;
	MOS6581State sid;
	MOS6526State cia1;
	MOS6526State cia2;

	MOS6502State driveCpu;
	Job1541State driveJob;

	// TODO: REU state is not saved
};


// Length of rewind buffer
constexpr size_t REWIND_LENGTH = SCREEN_FREQ * 30;  // 30 seconds


// For speed limiting to 50 fps
constexpr int FRAME_TIME_us = 20000;	// 20 ms for 50 fps
constexpr int FORWARD_SCALE = 4;		// Fast-forward is four times faster


// Joystick dead zone around center (+/-), and hysteresis to prevent jitter
constexpr int JOYSTICK_DEAD_ZONE = 12000;
constexpr int JOYSTICK_HYSTERESIS = 1000;


/*
 *  Constructor: Allocate objects and memory
 */

C64::C64() : quit_requested(false), prefs_editor_requested(false), load_snapshot_requested(false)
{
	// Allocate RAM/ROM memory
	RAM = new uint8_t[C64_RAM_SIZE];
	Basic = new uint8_t[BASIC_ROM_SIZE];
	Kernal = new uint8_t[KERNAL_ROM_SIZE];
	Char = new uint8_t[CHAR_ROM_SIZE];
	Color = new uint8_t[COLOR_RAM_SIZE];
	RAM1541 = new uint8_t[DRIVE_RAM_SIZE];
	ROM1541 = new uint8_t[DRIVE_ROM_SIZE];

	// Initialize memory
	init_memory();

	// Open display
	TheDisplay = new C64Display(this);

	// Create the chips
	TheCPU = new MOS6510(this, RAM, Basic, Kernal, Char, Color);

	TheJob1541 = new Job1541(RAM1541);
	TheCPU1541 = new MOS6502_1541(this, TheJob1541, TheDisplay, RAM1541, ROM1541);

	TheVIC = TheCPU->TheVIC = new MOS6569(this, TheDisplay, TheCPU, RAM, Char, Color);
	TheSID = TheCPU->TheSID = new MOS6581;
	TheCIA1 = TheCPU->TheCIA1 = new MOS6526_1(TheCPU, TheVIC);
	TheCIA2 = TheCPU->TheCIA2 = TheCPU1541->TheCIA2 = new MOS6526_2(TheCPU, TheVIC, TheCPU1541);
	TheIEC = TheCPU->TheIEC = new IEC(TheDisplay);
	TheREU = TheCPU->TheREU = new REU(TheCPU);

	// Initialize joystick variables
	joy_minx[0] = joy_miny[0] = -JOYSTICK_DEAD_ZONE;
	joy_maxx[0] = joy_maxy[0] = +JOYSTICK_DEAD_ZONE;
	joy_maxtrigl[0] = joy_maxtrigr[0] = +JOYSTICK_DEAD_ZONE;
	joy_trigl_on[0] = joy_trigr_on[0] = false;

	joy_minx[1] = joy_miny[1] = -JOYSTICK_DEAD_ZONE;
	joy_maxx[1] = joy_maxy[1] = +JOYSTICK_DEAD_ZONE;
	joy_maxtrigl[1] = joy_maxtrigr[1] = +JOYSTICK_DEAD_ZONE;
	joy_trigl_on[1] = joy_trigr_on[1] = false;

	// Open joystick drivers if required
	open_close_joysticks(0, 0, ThePrefs.Joystick1Port, ThePrefs.Joystick2Port);
	joykey = 0xff;

	// Allocate buffer for rewinding
	rewind_buffer = new Snapshot[REWIND_LENGTH];
}


/*
 *  Destructor: Delete all objects
 */

C64::~C64()
{
	open_close_joysticks(ThePrefs.Joystick1Port, ThePrefs.Joystick2Port, 0, 0);

	delete TheJob1541;
	delete TheREU;
	delete TheIEC;
	delete TheCIA2;
	delete TheCIA1;
	delete TheSID;
	delete TheVIC;
	delete TheCPU1541;
	delete TheCPU;
	delete TheDisplay;

	delete[] RAM;
	delete[] Basic;
	delete[] Kernal;
	delete[] Char;
	delete[] Color;
	delete[] RAM1541;
	delete[] ROM1541;

	delete[] rewind_buffer;
}


/*
 *  Initialize emulation memory
 */

void C64::init_memory()
{
	// Initialize RAM with powerup pattern
	uint8_t *p = RAM;
	for (unsigned i = 0; i < 512; ++i) {
		for (unsigned j = 0; j < 64; ++j) {
			*p++ = 0;
		}
		for (unsigned j = 0; j < 64; ++j) {
			*p++ = 0xff;
		}
	}

	// Initialize color RAM with random values
	p = Color;
	for (unsigned i = 0; i < COLOR_RAM_SIZE; ++i) {
		*p++ = rand() & 0x0f;
	}

	// Clear 1541 RAM
	memset(RAM1541, 0, DRIVE_RAM_SIZE);
}


/*
 *  Start main emulation loop
 */

void C64::Run()
{
	// Reset chips
	TheCPU->Reset();
	TheSID->Reset();
	TheCIA1->Reset();
	TheCIA2->Reset();
	TheCPU1541->Reset();

	// Patch kernal IEC routines
	orig_kernal_1d84 = Kernal[0x1d84];
	orig_kernal_1d85 = Kernal[0x1d85];
	patch_kernal(ThePrefs.FastReset, ThePrefs.Emul1541Proc);

	// Remember start time of first frame
	frame_start = chrono::steady_clock::now();
	cycle_counter = 0;

	// Enter main loop
	main_loop();
}


/*
 *  Request emulator to quit
 */

void C64::RequestQuit()
{
	quit_requested = true;
}


/*
 *  Request emulator to show prefs editor at next VBlank
 */

void C64::RequestPrefsEditor()
{
	prefs_editor_requested = true;
}


/*
 *  Request emulator to load snapshot at next VBlank
 */

void C64::RequestLoadSnapshot(const std::string & path)
{
	requested_snapshot = path;
	load_snapshot_requested = true;
}


/*
 *  Reset C64
 */

void C64::Reset(bool clear_memory)
{
	TheCPU->AsyncReset();
	TheCPU1541->AsyncReset();
	TheSID->Reset();
	TheCIA1->Reset();
	TheCIA2->Reset();
	TheIEC->Reset();

	if (clear_memory) {
		init_memory();
	}

	reset_play_mode();
}


/*
 *  NMI C64
 */

void C64::NMI()
{
	TheCPU->AsyncNMI();
}


/*
 *  The preferences have changed. prefs is a pointer to the new
 *  preferences, ThePrefs still holds the previous ones.
 */

void C64::NewPrefs(const Prefs *prefs)
{
	open_close_joysticks(ThePrefs.Joystick1Port, ThePrefs.Joystick2Port, prefs->Joystick1Port, prefs->Joystick2Port);
	patch_kernal(prefs->FastReset, prefs->Emul1541Proc);

	TheDisplay->NewPrefs(prefs);

	TheIEC->NewPrefs(prefs);
	TheJob1541->NewPrefs(prefs);

	TheREU->NewPrefs(prefs);
	TheSID->NewPrefs(prefs);

	// Reset 1541 processor if turned on or off (to bring IEC lines back to sane state)
	if (ThePrefs.Emul1541Proc != prefs->Emul1541Proc) {
		TheCPU1541->AsyncReset();
	}

	reset_play_mode();
}


/*
 *  Turn 1541 processor emulation on or off, and optionally set the drive path.
 */

void C64::SetEmul1541Proc(bool on, const char * path)
{
	auto prefs = std::make_unique<Prefs>(ThePrefs);
	if (path != nullptr) {
		prefs->DrivePath[0] = path;
	}
	prefs->Emul1541Proc = on;
	NewPrefs(prefs.get());
	ThePrefs = *prefs;
}


/*
 *  Patch kernal reset and IEC routines
 */

void C64::patch_kernal(bool fast_reset, bool emul_1541_proc)
{
	if (fast_reset) {
		Kernal[0x1d84] = 0xa0;
		Kernal[0x1d85] = 0x00;
	} else {
		Kernal[0x1d84] = orig_kernal_1d84;
		Kernal[0x1d85] = orig_kernal_1d85;
	}

	if (emul_1541_proc) {
		Kernal[0x0d40] = 0x78;
		Kernal[0x0d41] = 0x20;
		Kernal[0x0d23] = 0x78;
		Kernal[0x0d24] = 0x20;
		Kernal[0x0d36] = 0x78;
		Kernal[0x0d37] = 0x20;
		Kernal[0x0e13] = 0x78;
		Kernal[0x0e14] = 0xa9;
		Kernal[0x0def] = 0x78;
		Kernal[0x0df0] = 0x20;
		Kernal[0x0dbe] = 0xad;
		Kernal[0x0dbf] = 0x00;
		Kernal[0x0dcc] = 0x78;
		Kernal[0x0dcd] = 0x20;
		Kernal[0x0e03] = 0x20;
		Kernal[0x0e04] = 0xbe;
	} else {
		Kernal[0x0d40] = 0xf2;	// IECOut
		Kernal[0x0d41] = 0x00;
		Kernal[0x0d23] = 0xf2;	// IECOutATN
		Kernal[0x0d24] = 0x01;
		Kernal[0x0d36] = 0xf2;	// IECOutSec
		Kernal[0x0d37] = 0x02;
		Kernal[0x0e13] = 0xf2;	// IECIn
		Kernal[0x0e14] = 0x03;
		Kernal[0x0def] = 0xf2;	// IECSetATN
		Kernal[0x0df0] = 0x04;
		Kernal[0x0dbe] = 0xf2;	// IECRelATN
		Kernal[0x0dbf] = 0x05;
		Kernal[0x0dcc] = 0xf2;	// IECTurnaround
		Kernal[0x0dcd] = 0x06;
		Kernal[0x0e03] = 0xf2;	// IECRelease
		Kernal[0x0e04] = 0x07;
	}

	// 1541
	ROM1541[0x2ae4] = 0xea;		// Don't check ROM checksum
	ROM1541[0x2ae5] = 0xea;
	ROM1541[0x2ae8] = 0xea;
	ROM1541[0x2ae9] = 0xea;
	ROM1541[0x2c9b] = 0xf2;		// DOS idle loop
	ROM1541[0x2c9c] = 0x00;
	ROM1541[0x3594] = 0x20;		// Write sector
	ROM1541[0x3595] = 0xf2;
	ROM1541[0x3596] = 0xf5;
	ROM1541[0x3597] = 0xf2;
	ROM1541[0x3598] = 0x01;
	ROM1541[0x3b0c] = 0xf2;		// Format track
	ROM1541[0x3b0d] = 0x02;
}


#ifdef FRODO_SC

/*
 *  Emulate one cycle of the C64.
 *  Returns true if a new video frame has started.
 */

bool C64::emulate_c64_cycle()
{
	// The order of calls is important here
	unsigned flags = TheVIC->EmulateCycle();
	if (flags & VIC_HBLANK) {
		TheSID->EmulateLine();
	}
	TheCIA1->CheckIRQs();
	TheCIA2->CheckIRQs();
	TheCIA1->EmulateCycle();
	TheCIA2->EmulateCycle();
	TheCPU->EmulateCycle();
	++cycle_counter;

	return flags & VIC_VBLANK;
}


/*
 *  Emulate one cycle of the 1541.
 */

void C64::emulate_1541_cycle()
{
	TheCPU1541->CountVIATimers(1);
	if (!TheCPU1541->Idle) {
		TheCPU1541->EmulateCycle();
	}
}

#endif // def FRODO_SC


/*
 *  Pause emulator
 */

void C64::pause()
{
	TheSID->PauseSound();
	TheDisplay->Pause();
}


/*
 *  Resume emulator
 */

void C64::resume()
{
	TheDisplay->Resume();
	TheSID->ResumeSound();

	// Flush event queue
	SDL_PumpEvents();
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
}


/*
 *  Vertical blank: Poll input devices, update display
 */

void C64::vblank()
{
	// Poll keyboard and joysticks
	poll_input();

	// Handle request for prefs editor
	if (prefs_editor_requested) {
		pause();
		if (! TheApp->RunPrefsEditor()) {
			quit_requested = true;
			return;
		}
		resume();
		prefs_editor_requested = false;
	}

	// Handle request for snapshot loading
	if (load_snapshot_requested) {
		LoadSnapshot(requested_snapshot);
		load_snapshot_requested = false;
	}

	// Count TOD clocks
	TheCIA1->CountTOD();
	TheCIA2->CountTOD();

	// Update window if needed
	if (! TheVIC->FrameSkipped()) {
    	TheDisplay->Update();
	}

	// Handle rewind feature
	handle_rewind();

	// Calculate time between frames, display speedometer
	chrono::time_point<chrono::steady_clock> now = chrono::steady_clock::now();

	int elapsed_us = chrono::duration_cast<chrono::microseconds>(now - frame_start).count();
	int speed_index = FRAME_TIME_us / double(elapsed_us + 1) * 100;

	// Limit speed to 100% if desired
	if ((elapsed_us < FRAME_TIME_us) && ThePrefs.LimitSpeed) {
		std::this_thread::sleep_until(frame_start);
		if (play_mode == PLAY_MODE_FORWARD) {
			frame_start += chrono::microseconds(FRAME_TIME_us / FORWARD_SCALE);
		} else {
			frame_start += chrono::microseconds(FRAME_TIME_us);
		}
		speed_index = 100;	// Hide speed display even in fast-forwarding mode
	} else {
		frame_start = now;
	}

	TheDisplay->UpdateSpeedometer(speed_index);
}


/*
 *  The emulation's main loop
 */

void C64::main_loop()
{
	unsigned prev_raster_y = 0;

	while (!quit_requested) {
		bool new_frame;

#ifdef FRODO_SC

		new_frame = emulate_c64_cycle();
		if (ThePrefs.Emul1541Proc) {
			emulate_1541_cycle();
		}

#else

		// The order of calls is important here
		int cycles = 0;
		unsigned flags = TheVIC->EmulateLine(cycles);
		new_frame = (flags & VIC_VBLANK);

		TheSID->EmulateLine();
#if !PRECISE_CIA_CYCLES
		TheCIA1->EmulateLine(ThePrefs.CIACycles);
		TheCIA2->EmulateLine(ThePrefs.CIACycles);
#endif

		if (ThePrefs.Emul1541Proc) {
			int cycles_1541 = ThePrefs.FloppyCycles;
			TheCPU1541->CountVIATimers(cycles_1541);

			if (!TheCPU1541->Idle) {
				// 1541 processor active, alternately execute
				//  6502 and 6510 instructions until both have
				//  used up their cycles
				while (cycles >= 0 || cycles_1541 >= 0)
					if (cycles > cycles_1541) {
						cycles -= TheCPU->EmulateLine(1);
					} else {
						cycles_1541 -= TheCPU1541->EmulateLine(1);
					}
			} else {
				TheCPU->EmulateLine(cycles);
			}
		} else {
			// 1541 processor disabled, only emulate 6510
			TheCPU->EmulateLine(cycles);
		}

#endif  // def FRODO_SC

		// Poll keyboard and mouse, and delay execution at three points
		// within the frame to reduce input lag. This also helps with the
		// asynchronously running SID emulation.
		if (ThePrefs.LimitSpeed && play_mode == PLAY_MODE_PLAY) {
			unsigned raster_y = TheVIC->RasterY();
			if (raster_y != prev_raster_y) {
				if (raster_y == TOTAL_RASTERS * 1 / 4) {
					std::this_thread::sleep_until(frame_start - chrono::microseconds(FRAME_TIME_us * 3 / 4));
					poll_input();
				} else if (raster_y == TOTAL_RASTERS * 2 / 4) {
					std::this_thread::sleep_until(frame_start - chrono::microseconds(FRAME_TIME_us * 2 / 4));
					poll_input();
				} else if (raster_y == TOTAL_RASTERS * 3 / 4) {
					std::this_thread::sleep_until(frame_start - chrono::microseconds(FRAME_TIME_us * 1 / 4));
					poll_input();
				}

				prev_raster_y = raster_y;
			}
		}

		// Update display etc. if new frame has started
		if (new_frame) {
			vblank();
		}
	}
}


/*
 *  Poll keyboard and joysticks
 */

void C64::poll_input()
{
	// Poll joysticks
	TheCIA1->Joystick1 = poll_joystick(0);
	TheCIA1->Joystick2 = poll_joystick(1);

	if (ThePrefs.JoystickSwap) {
		std::swap(TheCIA1->Joystick1, TheCIA1->Joystick2);
	}

	// Poll keyboard
	TheDisplay->PollKeyboard(TheCIA1->KeyMatrix, TheCIA1->RevMatrix, &joykey);

	// Joystick keyboard emulation
	if (TheDisplay->NumLock()) {
		TheCIA1->Joystick1 &= joykey;
	} else {
		TheCIA1->Joystick2 &= joykey;
	}
}


/*
 *  Open/close joystick drivers given old and new state of
 *  joystick preferences
 */

void C64::open_close_joystick(int port, int oldjoy, int newjoy)
{
	if (oldjoy != newjoy) {
		if (newjoy > 0) {
			int index = newjoy - 1;
			joy[port] = SDL_JoystickOpen(index);
			if (joy[port] == nullptr) {
				fprintf(stderr, "Couldn't open joystick %d: %s\n", port + 1, SDL_GetError());
			} else if (SDL_IsGameController(index)) {
				controller[port] = SDL_GameControllerOpen(index);
			}
		} else {
			if (controller[port]) {
				SDL_GameControllerClose(controller[port]);
				controller[port] = nullptr;
			}
			if (joy[port]) {
				SDL_JoystickClose(joy[port]);
				joy[port] = nullptr;
			}
		}
	}
}

void C64::open_close_joysticks(int oldjoy1, int oldjoy2, int newjoy1, int newjoy2)
{
	open_close_joystick(0, oldjoy1, newjoy1);
	open_close_joystick(1, oldjoy2, newjoy2);
}


/*
 *  Game controller added
 */

void C64::JoystickAdded(int32_t index)
{
	// Assign to port 2 first, then to port 1
	if (joy[1] == nullptr && ThePrefs.Joystick1Port != index + 1) {

		ThePrefs.Joystick2Port = index + 1;
		open_close_joystick(1, 0, ThePrefs.Joystick2Port);

	} else if (joy[0] == nullptr && ThePrefs.Joystick2Port != index + 1) {

		ThePrefs.Joystick1Port = index + 1;
		open_close_joystick(0, 0, ThePrefs.Joystick1Port);
	}
}


/*
 *  Game controller removed
 */

void C64::JoystickRemoved(int32_t instance_id)
{
	if (joy[0] && SDL_JoystickInstanceID(joy[0]) == instance_id) {

		// Unassign joystick port 1
		open_close_joystick(0, ThePrefs.Joystick1Port, 0);
		ThePrefs.Joystick1Port = 0;

	} else if (joy[1] && SDL_JoystickInstanceID(joy[1]) == instance_id) {

		// Unassign joystick port 2
		open_close_joystick(1, ThePrefs.Joystick2Port, 0);
		ThePrefs.Joystick2Port = 0;
	}
}


/*
 *  Poll joystick port, return CIA mask
 */

uint8_t C64::poll_joystick(int port)
{
	uint8_t j = 0xff;

	int x = 0;
	int y = 0;

	if (controller[port]) {

		// Use Game Controller API
		if (SDL_GameControllerGetButton(controller[port], SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
			j &= 0xfb;							// Left
		}
		if (SDL_GameControllerGetButton(controller[port], SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
			j &= 0xf7;							// Right
		}
		if (SDL_GameControllerGetButton(controller[port], SDL_CONTROLLER_BUTTON_DPAD_UP)) {
			j &= 0xfe;							// Up
		}
		if (SDL_GameControllerGetButton(controller[port], SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
			j &= 0xfd;							// Down
		}
		if (SDL_GameControllerGetButton(controller[port], SDL_CONTROLLER_BUTTON_A) ||
		    SDL_GameControllerGetButton(controller[port], SDL_CONTROLLER_BUTTON_B)) {
			j &= 0xef;							// Button
		}

		// Left trigger controls rewind
		int trigger = SDL_GameControllerGetAxis(controller[port], SDL_CONTROLLER_AXIS_TRIGGERLEFT);
		if (trigger > joy_maxtrigl[port]) {
			if (! joy_trigl_on[port]) {
				if (GetPlayMode() == PLAY_MODE_PLAY) {
					SetPlayMode(PLAY_MODE_REWIND);
				}
				joy_trigl_on[port] = true;
			}
			joy_maxtrigl[port] = +(JOYSTICK_DEAD_ZONE - JOYSTICK_HYSTERESIS);
		} else {
			if (joy_trigl_on[port]) {
				if (GetPlayMode() == PLAY_MODE_REWIND) {
					SetPlayMode(PLAY_MODE_PLAY);
				}
				joy_trigl_on[port] = false;
			}
			joy_maxtrigl[port] = +JOYSTICK_DEAD_ZONE;
		}

		// Right trigger controls fast-forward
		trigger = SDL_GameControllerGetAxis(controller[port], SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
		if (trigger > joy_maxtrigr[port]) {
			if (! joy_trigr_on[port]) {
				if (GetPlayMode() == PLAY_MODE_PLAY) {
					SetPlayMode(PLAY_MODE_FORWARD);
				}
				joy_trigr_on[port] = true;
			}
			joy_maxtrigr[port] = +(JOYSTICK_DEAD_ZONE - JOYSTICK_HYSTERESIS);
		} else {
			if (joy_trigr_on[port]) {
				if (GetPlayMode() == PLAY_MODE_FORWARD) {
					SetPlayMode(PLAY_MODE_PLAY);
				}
				joy_trigr_on[port] = false;
			}
			joy_maxtrigr[port] = +JOYSTICK_DEAD_ZONE;
		}

		// Left stick is an alternative to D-pad
		x = SDL_GameControllerGetAxis(controller[port], SDL_CONTROLLER_AXIS_LEFTX);
		y = SDL_GameControllerGetAxis(controller[port], SDL_CONTROLLER_AXIS_LEFTY);

	} else if (joy[port]) {

		// Not a Game Controller, use joystick API
		if (SDL_JoystickGetButton(joy[port], 0)) {
			j &= 0xef;							// Button
		}

		x = SDL_JoystickGetAxis(joy[port], 0);
		y = SDL_JoystickGetAxis(joy[port], 1);
	}

	if (x < joy_minx[port]) {
		j &= 0xfb;							// Left
		joy_minx[port] = -(JOYSTICK_DEAD_ZONE - JOYSTICK_HYSTERESIS);
	} else {
		joy_minx[port] = -JOYSTICK_DEAD_ZONE;
	}

	if (x > joy_maxx[port]) {
		j &= 0xf7;							// Right
		joy_maxx[port] = +(JOYSTICK_DEAD_ZONE - JOYSTICK_HYSTERESIS);
	} else {
		joy_maxx[port] = +JOYSTICK_DEAD_ZONE;
	}

	if (y < joy_miny[port]) {
		j &= 0xfe;							// Up
		joy_miny[port] = -(JOYSTICK_DEAD_ZONE - JOYSTICK_HYSTERESIS);
	} else {
		joy_miny[port] = -JOYSTICK_DEAD_ZONE;
	}

	if (y > joy_maxy[port]) {
		j &= 0xfd;							// Down
		joy_maxy[port] = +(JOYSTICK_DEAD_ZONE - JOYSTICK_HYSTERESIS);
	} else {
		joy_maxy[port] = +JOYSTICK_DEAD_ZONE;
	}

	return j;
}


/*
 *  Save state to snapshot (emulation must be in VBlank)
 *
 *  To be able to use SC snapshots with SL, the state of the SC C64 and 1541
 *  CPUs are not saved in the middle of an instruction. Instead the state is
 *  advanced cycle by cycle until the current instruction has finished.
 */

void C64::MakeSnapshot(Snapshot * s)
{
	memset(s, 0, sizeof(*s));

	memcpy(s->magic, SNAPSHOT_HEADER, sizeof(s->magic));

	if (ThePrefs.DrivePath[0].length() < sizeof(s->drive8Path)) {
		strcpy(s->drive8Path, ThePrefs.DrivePath[0].c_str());
	}

#ifdef FRODO_SC
	while (true) {
		TheCPU->GetState(&(s->cpu));

		if (s->cpu.instruction_complete)
			break;

		// Advance C64 state by one cycle
		emulate_c64_cycle();
		if (ThePrefs.Emul1541Proc) {
			emulate_1541_cycle();
		}
	}
#else
	TheCPU->GetState(&(s->cpu));
#endif
	s->cycleCounter = cycle_counter;

	TheVIC->GetState(&(s->vic));
	TheSID->GetState(&(s->sid));
	TheCIA1->GetState(&(s->cia1));
	TheCIA2->GetState(&(s->cia2));

	memcpy(s->ram, RAM, C64_RAM_SIZE);
	memcpy(s->color, Color, COLOR_RAM_SIZE);

	if (ThePrefs.Emul1541Proc) {
		s->flags |= SNAPSHOT_FLAG_1541_PROC;

#ifdef FRODO_SC
		while (true) {
			TheCPU1541->GetState(&(s->driveCpu));

			if (s->driveCpu.idle || s->driveCpu.instruction_complete)
				break;

			// Advance 1541 state by one cycle
			emulate_1541_cycle();
		}
#else
		TheCPU1541->GetState(&(s->driveCpu));
#endif
	}

	TheJob1541->GetState(&(s->driveJob));

	memcpy(s->driveRam, RAM1541, DRIVE_RAM_SIZE);
}


/*
 *  Restore state from snapshot (emulation must be paused and in VBlank)
 *
 *  Note: The magic header is not checked by this function.
 */

void C64::RestoreSnapshot(const Snapshot * s)
{
	// SL CPU64::SetState() overwrites ram[0/1], so we need to restore that
	// first in case we're loading an SC snapshot
	memcpy(RAM, s->ram, C64_RAM_SIZE);
	memcpy(Color, s->color, COLOR_RAM_SIZE);

	cycle_counter = s->cycleCounter;
	TheCPU->SetState(&(s->cpu));
	TheVIC->SetState(&(s->vic));
	TheSID->SetState(&(s->sid));
	TheCIA1->SetState(&(s->cia1));
	TheCIA2->SetState(&(s->cia2));

	if (s->flags & SNAPSHOT_FLAG_1541_PROC) {

		memcpy(RAM1541, s->driveRam, DRIVE_RAM_SIZE);

		// Switch on 1541 processor emulation if it is off
		if (! ThePrefs.Emul1541Proc) {
			SetEmul1541Proc(true, s->drive8Path);
		}

		TheCPU1541->SetState(&(s->driveCpu));
		TheJob1541->SetState(&(s->driveJob));

	} else {

		// Switch off 1541 processor emulation if it is on
		if (ThePrefs.Emul1541Proc) {
			SetEmul1541Proc(false);
		}
	}
}


/*
 *  Save snapshot file (emulation must be paused and in VBlank)
 */

bool C64::SaveSnapshot(const std::string & filename)
{
	FILE * f = fopen(filename.c_str(), "wb");
	if (f == nullptr) {
		ShowRequester("Can't create snapshot file", "OK", nullptr);
		return false;
	}

	auto s = std::make_unique<Snapshot>();
	MakeSnapshot(s.get());

	// TODO: Endianess and alignment should be taken care of
	// to make snapshot files portable

	if (fwrite(s.get(), sizeof(Snapshot), 1, f) != 1) {
		ShowRequester("Error writing to snapshot file", "OK", nullptr);
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}


/*
 *  Load snapshot file (emulation must be paused and in VBlank)
 */

bool C64::LoadSnapshot(const std::string & filename)
{
	FILE * f = fopen(filename.c_str(), "rb");
	if (f == nullptr) {
		ShowRequester("Can't open snapshot file", "OK", nullptr);
		return false;
	}

	auto s = std::make_unique<Snapshot>();

	if (fread(s.get(), sizeof(Snapshot), 1, f) != 1) {
		ShowRequester("Error reading snapshot file", "OK", nullptr);
		fclose(f);
		return false;
	}

	if (memcmp(s->magic, SNAPSHOT_HEADER, sizeof(s->magic)) != 0) {
		ShowRequester("Not a Frodo snapshot file", "OK", nullptr);
		fclose(f);
		return false;
	}

	RestoreSnapshot(s.get());
	reset_play_mode();

	fclose(f);
	return true;
}


/*
 *  Stop rewind/forward mode and clear rewind buffer
 */

void C64::reset_play_mode()
{
	SetPlayMode(PLAY_MODE_PLAY);
	rewind_start = 0;
	rewind_fill = 0;
}


/*
 *  Set rewind/forward mode
 */

void C64::SetPlayMode(PlayMode mode)
{
	play_mode = mode;
}


/*
 *  Handle rewind recording and replay (to be called in VBlank)
 */

void C64::handle_rewind()
{
	if (rewind_buffer != nullptr) {
		if (play_mode == PLAY_MODE_REWIND) {

			// Pop snapshot from ring buffer
			if (rewind_fill > 0) {
				size_t read_index = (rewind_start + rewind_fill - 1) % REWIND_LENGTH;
				RestoreSnapshot(rewind_buffer + read_index);

				// Keep first snapshot in buffer so we can repeat it when
				// reaching the end of the buffer
				if (rewind_fill > 1) {
					--rewind_fill;
				}
			}

		} else {

			// Add snapshot to ring buffer
			size_t write_index = (rewind_start + rewind_fill) % REWIND_LENGTH;
			MakeSnapshot(rewind_buffer + write_index);

			if (rewind_fill < REWIND_LENGTH) {
				++rewind_fill;
			} else {
				rewind_start = (rewind_start + 1) % REWIND_LENGTH;
			}
		}
	}
}


/*
 *  Check whether file is a snapshot file
 */

bool IsSnapshotFile(const char * filename)
{
	FILE * f = fopen(filename, "rb");
	if (f == nullptr)
		return false;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size != sizeof(Snapshot))
		return false;

	uint8_t magic[sizeof(Snapshot::magic)];
	memset(magic, 0, sizeof(magic));
	fread(magic, sizeof(magic), 1, f);
	fclose(f);

	return memcmp(magic, SNAPSHOT_HEADER, sizeof(magic)) == 0;
}
