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
constexpr int FRAME_TIME_us = 20000;	// 20 ms for 50 fps
constexpr int FORWARD_SCALE = 4;		// Fast-forward is four times faster

// Joystick dead zone around center (+/-), and hysteresis to prevent jitter
constexpr int JOYSTICK_DEAD_ZONE = 12000;
constexpr int JOYSTICK_HYSTERESIS = 1000;


/*
 *  Constructor, system-dependent things
 */

void C64::c64_ctor1()
{
	// Initialize joystick variables
	joy_minx[0] = joy_miny[0] = -JOYSTICK_DEAD_ZONE;
	joy_maxx[0] = joy_maxy[0] = +JOYSTICK_DEAD_ZONE;
	joy_maxtrigl[0] = joy_maxtrigr[0] = +JOYSTICK_DEAD_ZONE;
	joy_trigl_on[0] = joy_trigr_on[0] = false;

	joy_minx[1] = joy_miny[1] = -JOYSTICK_DEAD_ZONE;
	joy_maxx[1] = joy_maxy[1] = +JOYSTICK_DEAD_ZONE;
	joy_maxtrigl[1] = joy_maxtrigr[1] = +JOYSTICK_DEAD_ZONE;
	joy_trigl_on[1] = joy_trigr_on[1] = false;
}

void C64::c64_ctor2()
{
	frame_start = chrono::steady_clock::now();
}


/*
 *  Destructor, system-dependent things
 */

void C64::c64_dtor()
{
}


/*
 *  Start main emulation thread
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
	PatchKernal(ThePrefs.FastReset, ThePrefs.Emul1541Proc);

	quit_thyself = false;
	thread_func();
}


/*
 *  Vertical blank: Poll keyboard and joysticks, update display
 */

void C64::vblank()
{
	// Poll joysticks
	TheCIA1->Joystick1 = poll_joystick(0);
	TheCIA1->Joystick2 = poll_joystick(1);

	if (ThePrefs.JoystickSwap) {
		uint8_t tmp = TheCIA1->Joystick1;
		TheCIA1->Joystick1 = TheCIA1->Joystick2;
		TheCIA1->Joystick2 = tmp;
	}

	// Poll keyboard
	TheDisplay->PollKeyboard(TheCIA1->KeyMatrix, TheCIA1->RevMatrix, &joykey);
	if (TheDisplay->quit_requested) {
		quit_thyself = true;
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

	TheDisplay->Speedometer(speed_index);
}


/*
 *  The emulation's main loop
 */

void C64::thread_func()
{
	while (!quit_thyself) {
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

		// Update display etc. if new frame has started
		if (new_frame) {
			vblank();
		}
	}
}


/*
 *  Pause main emulation thread
 */

void C64::Pause()
{
	TheSID->PauseSound();
}


/*
 *  Resume main emulation thread
 */

void C64::Resume()
{
	TheSID->ResumeSound();

	// Flush event queue
	SDL_PumpEvents();
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
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
