/*
 *  C64_SDL.h - Put the pieces together, SDL specific stuff
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

#include "main.h"

#include <SDL.h>

#include <chrono>
#include <thread>
namespace chrono = std::chrono;


// For speed limiting to 50 fps
static chrono::time_point<chrono::steady_clock> frame_start;
static constexpr int FRAME_TIME_us = 20000;  // 20 ms for 50 fps

// Joystick dead zone around center (+/-), and hysteresis to prevent jitter
static constexpr int JOYSTICK_DEAD_ZONE = 12000;
static constexpr int JOYSTICK_HYSTERESIS = 1000;


/*
 *  Constructor, system-dependent things
 */

void C64::c64_ctor1(void)
{
	// Initialize joystick variables
	joy_minx[0] = joy_miny[0] = -JOYSTICK_DEAD_ZONE;
	joy_maxx[0] = joy_maxy[0] = +JOYSTICK_DEAD_ZONE;
	joy_minx[1] = joy_miny[1] = -JOYSTICK_DEAD_ZONE;
	joy_maxx[1] = joy_maxy[1] = +JOYSTICK_DEAD_ZONE;
}

void C64::c64_ctor2(void)
{
   	printf("Use F9 to enter the SAM machine language monitor,\n"
   	       "F10 to edit preferences or quit,\n"
   	       "F11 to cause an NMI (RESTORE key) and\n"
   	       "F12 to reset the C64.\n\n");
  
	frame_start = chrono::steady_clock::now();
}


/*
 *  Destructor, system-dependent things
 */

void C64::c64_dtor(void)
{
}


/*
 *  Start main emulation thread
 */

void C64::Run(void)
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
	PatchKernal(ThePrefs.FastReset, ThePrefs.Emul1541Proc);

	quit_thyself = false;
	thread_func();
}


/*
 *  Vertical blank: Poll keyboard and joysticks, update window
 */

void C64::VBlank(bool draw_frame)
{
	// Poll keyboard
	TheDisplay->PollKeyboard(TheCIA1->KeyMatrix, TheCIA1->RevMatrix, &joykey);
	if (TheDisplay->quit_requested) {
		quit_thyself = true;
	}

	// Poll joysticks
	TheCIA1->Joystick1 = poll_joystick(0);
	TheCIA1->Joystick2 = poll_joystick(1);

	if (ThePrefs.JoystickSwap) {
		uint8_t tmp = TheCIA1->Joystick1;
		TheCIA1->Joystick1 = TheCIA1->Joystick2;
		TheCIA1->Joystick2 = tmp;
	}

	// Joystick keyboard emulation
	if (TheDisplay->NumLock()) {
		TheCIA1->Joystick1 &= joykey;
	} else {
		TheCIA1->Joystick2 &= joykey;
	}
       
	// Count TOD clocks
	TheCIA1->CountTOD();
	TheCIA2->CountTOD();

	// Update window if needed
	if (draw_frame) {
    	TheDisplay->Update();
	}

	// Calculate time between frames, display speedometer
	chrono::time_point<chrono::steady_clock> now = chrono::steady_clock::now();

	int elapsed_us = chrono::duration_cast<chrono::microseconds>(now - frame_start).count();
	int speed_index = FRAME_TIME_us / double(elapsed_us + 1) * 100;

	// Limit speed to 100% if desired
	if ((elapsed_us < FRAME_TIME_us) && ThePrefs.LimitSpeed) {
		std::this_thread::sleep_until(frame_start);
		frame_start += chrono::microseconds(FRAME_TIME_us);
		speed_index = 100;
	} else {
		frame_start = now;
	}

	TheDisplay->Speedometer(speed_index);
}


/*
 * The emulation's main loop
 */

void C64::thread_func(void)
{
#ifdef FRODO_SC
	while (!quit_thyself) {

		// The order of calls is important here
		if (TheVIC->EmulateCycle())
			TheSID->EmulateLine();
		TheCIA1->CheckIRQs();
		TheCIA2->CheckIRQs();
		TheCIA1->EmulateCycle();
		TheCIA2->EmulateCycle();
		TheCPU->EmulateCycle();

		if (ThePrefs.Emul1541Proc) {
			TheCPU1541->CountVIATimers(1);
			if (!TheCPU1541->Idle) {
				TheCPU1541->EmulateCycle();
			}
		}
		CycleCounter++;
#else
	while (!quit_thyself) {

		// The order of calls is important here
		int cycles = TheVIC->EmulateLine();
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
#endif
	}
}


/*
 *  Pause main emulation thread
 */

void C64::Pause(void)
{
	TheSID->PauseSound();
}


/*
 *  Resume main emulation thread
 */

void C64::Resume(void)
{
	TheSID->ResumeSound();
}


/*
 *  Open/close joystick drivers given old and new state of
 *  joystick preferences
 */

void C64::open_close_joystick(int port, int oldjoy, int newjoy)
{
	if (oldjoy != newjoy) {
		if (newjoy) {
			joy[port] = SDL_JoystickOpen(newjoy - 1);
			if (joy[port] == nullptr) {
				fprintf(stderr, "Couldn't open joystick %d\n", port + 1);
			}
		} else {
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
 *  Poll joystick port, return CIA mask
 */

uint8_t C64::poll_joystick(int port)
{
	uint8_t j = 0xff;

	if (port == 0 && (joy[0] || joy[1])) {
		SDL_JoystickUpdate();
	}

	if (joy[port]) {
		int x = SDL_JoystickGetAxis(joy[port], 0), y = SDL_JoystickGetAxis(joy[port], 1);

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

		if (SDL_JoystickGetButton(joy[port], 0)) {
			j &= 0xef;							// Button
		}
	}

	return j;
}
