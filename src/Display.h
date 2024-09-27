/*
 *  Display.h - C64 graphics display, emulator window handling
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

#ifndef _DISPLAY_H
#define _DISPLAY_H

#include <SDL.h>

#include <chrono>
#include <string>


// Display dimensions
constexpr int DISPLAY_X = 0x180;
constexpr int DISPLAY_Y = 0x110;


class C64;
class Prefs;

// Class for C64 graphics display
class C64Display {
public:
	C64Display(C64 * c64);
	~C64Display();

	void Pause();
	void Resume();

	void NewPrefs(const Prefs *prefs);

	void Update();

	void SetLEDs(int l0, int l1, int l2, int l3);
	void SetSpeedometer(int speed);
	void ShowNotification(std::string s);

	uint8_t *BitmapBase();
	int BitmapXMod();

	void PollKeyboard(uint8_t *key_matrix, uint8_t *rev_matrix, uint8_t *joystick);
	bool NumLock();

private:
	void init_colors(int palette_prefs);

	void error_and_quit(const std::string & msg) const;

	void fill_rect(const SDL_Rect & r, uint8_t color) const;
	void draw_string(unsigned x, unsigned y, const char *str, uint8_t front_color) const;

	static uint32_t pulse_handler_static(uint32_t interval, void * arg);
	void pulse_handler();

	void toggle_fullscreen(bool full);

	C64 * the_c64;						// Pointer to C64 object

	int led_state[4];
	int old_led_state[4];
	SDL_TimerID pulse_timer = 0;		// Timer for LED error blinking

	SDL_Window * the_window = nullptr;
	SDL_Renderer * the_renderer = nullptr;
	SDL_Texture * the_texture = nullptr;

	uint8_t * pixel_buffer = nullptr;	// Buffer for VIC to draw into
	uint32_t palette[256];				// Mapping of VIC color values to native ARGB

	char speedometer_string[16];		// Speedometer text (screen code)
	char notification[46];				// Notification text (screen code)
	std::chrono::time_point<std::chrono::steady_clock> notification_time;	// Time of notification

	bool num_locked = false;			// For keyboard joystick swap
};

#endif
