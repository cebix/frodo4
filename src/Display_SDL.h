/*
 *  Display_SDL.h - C64 graphics display, emulator window handling,
 *                  SDL specific stuff
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

#include "C64.h"
#include "IEC.h"
#include "SAM.h"
#include "Version.h"

#include <SDL.h>

#include <filesystem>
#include <format>
namespace fs = std::filesystem;

#include <stdlib.h>


// Period of LED error blinking
constexpr uint32_t PULSE_ms = 400;

// For requester
static SDL_Window * c64_window = nullptr;

// Colors for speedometer/drive LEDs
enum {
	black = 0,
	white = 1,
	fill_gray = 16,
	shine_gray = 17,
	shadow_gray = 18,
	red = 19,
	green = 20,
};


/*
  C64 keyboard matrix:

    Bit 7   6   5   4   3   2   1   0
  0    CUD  F5  F3  F1  F7 CLR RET DEL
  1    SHL  E   S   Z   4   A   W   3
  2     X   T   F   C   6   D   R   5
  3     V   U   H   B   8   G   Y   7
  4     N   O   K   M   0   J   I   9
  5     ,   @   :   .   -   L   P   +
  6     /   ^   =  SHR HOM  ;   *   �
  7    R/S  Q   C= SPC  2  CTL  <-  1
*/

#define MATRIX(a,b) (((a) << 3) | (b))


/*
 *  Display constructor
 */

C64Display::C64Display(C64 *the_c64) : TheC64(the_c64)
{
	speedometer_string[0] = 0;

	// Create window and renderer
	uint32_t flags;
	if (ThePrefs.DisplayType == DISPTYPE_SCREEN) {
	    flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else {
	    flags = SDL_WINDOW_RESIZABLE;
	}

	int result = SDL_CreateWindowAndRenderer(
		DISPLAY_X * ThePrefs.ScalingNumerator / ThePrefs.ScalingDenominator,
		DISPLAY_Y * ThePrefs.ScalingNumerator / ThePrefs.ScalingDenominator,
		flags, &the_window, &the_renderer
	);
	if (result < 0) {
		error_and_quit(std::format("Couldn't initialize video output ({})\n", SDL_GetError()));
	}

	SDL_SetWindowTitle(the_window, VERSION_STRING);
	SDL_SetWindowMinimumSize(the_window, DISPLAY_X, DISPLAY_Y);
	SDL_RenderSetLogicalSize(the_renderer, DISPLAY_X, DISPLAY_Y);

	c64_window = the_window;

	// Clear screen to black
	SDL_SetRenderDrawColor(the_renderer, 0, 0, 0, 255);
	SDL_RenderClear(the_renderer);
	SDL_RenderPresent(the_renderer);

	// Create 32-bit display texture
	the_texture = SDL_CreateTexture(the_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DISPLAY_X, DISPLAY_Y);
	if (! the_texture) {
		error_and_quit(std::format("Couldn't create SDL texture ({})\n", SDL_GetError()));
	}

	// Create 8-bit indexed pixel buffer for VIC to draw into
	pixel_buffer = new uint8_t[DISPLAY_X * DISPLAY_Y];

	// Init color palette for pixel buffer
	init_colors(ThePrefs.Palette);

	// Hide mouse pointer in fullscreen mode
	if (ThePrefs.DisplayType == DISPTYPE_SCREEN) {
		SDL_ShowCursor(SDL_DISABLE);
	}

	// LEDs off
	for (int i=0; i<4; i++) {
		led_state[i] = old_led_state[i] = LED_OFF;
	}

	// Start timer for LED error blinking
	pulse_timer = SDL_AddTimer(PULSE_ms, pulse_handler_static, this);
	if (pulse_timer == 0) {
		error_and_quit(std::format("Couldn't create SDL pulse timer ({})\n", SDL_GetError()));
	}
}


/*
 *  Display destructor
 */

C64Display::~C64Display()
{
	if (pulse_timer) {
		SDL_RemoveTimer(pulse_timer);
	}

	delete[] pixel_buffer;

	c64_window = nullptr;
}


/*
 *  Show an error message and quit
 */

void C64Display::error_and_quit(const std::string & msg) const
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, VERSION_STRING, msg.c_str(), the_window);
	SDL_Quit();
	exit(1);
}


/*
 *  Pause display: Exit fullscreen mode
 */

void C64Display::Pause()
{
	if (ThePrefs.DisplayType == DISPTYPE_SCREEN) {
		toggle_fullscreen(false);
	}
}


/*
 *  Resume display: Re-enter fullscreen mode
 */

void C64Display::Resume()
{
	if (ThePrefs.DisplayType == DISPTYPE_SCREEN) {
		toggle_fullscreen(true);
	}
}


/*
 *  Prefs may have changed, recalculate palette
 */

void C64Display::NewPrefs(const Prefs *prefs)
{
	if (prefs->Palette != ThePrefs.Palette) {
		init_colors(prefs->Palette);
	}
}


/*
 *  Redraw bitmap
 */

void C64Display::Update()
{
	if (ThePrefs.ShowLEDs) {

		// Draw speedometer/LEDs
		draw_string(8, DISPLAY_Y - 8, speedometer_string, black);
		draw_string(8, DISPLAY_Y - 9, speedometer_string, shine_gray);

		for (unsigned i = 0; i < 4; ++i) {
			if (led_state[i] != LED_OFF) {
				static const char * drive_str[4] = {
					"D\x12 8", "D\x12 9", "D\x12 10", "D\x12 11"	// \x12 = "r"
				};

				draw_string(DISPLAY_X * (i+1) / 6 + 8, DISPLAY_Y - 8, drive_str[i], black);
				draw_string(DISPLAY_X * (i+1) / 6 + 7, DISPLAY_Y - 9, drive_str[i], shine_gray);

				SDL_Rect r = {(int)(DISPLAY_X * (i+2) / 6 - 16), DISPLAY_Y - 8, 14, 6};
				fill_rect(r, shadow_gray);
				r.x += 1; r.y += 1; r.w -=1; r.h -= 1;
				fill_rect(r, shine_gray);

				uint8_t c;
				switch (led_state[i]) {
					case LED_ON:
						c = green;
						break;
					case LED_ERROR_ON:
						c = red;
						break;
					default:
						c = black;
						break;
				}

				r.w -= 1; r.h -= 1;
				fill_rect(r, c);
			}
		}

		// Draw rewind/forward marker
		PlayMode mode = TheC64->GetPlayMode();
		if (mode != PLAY_MODE_PLAY) {
			const char * str = (mode == PLAY_MODE_REWIND) ? "<<" : ">>";
			draw_string(DISPLAY_X - 24, DISPLAY_Y - 8, str, black);
			draw_string(DISPLAY_X - 24, DISPLAY_Y - 9, str, shine_gray);
		}
	}

	// Convert 8-bit pixel buffer to 32-bit texture
	uint32_t * texture_buffer;
	int texture_pitch;

	SDL_LockTexture(the_texture, nullptr, (void **) &texture_buffer, &texture_pitch);

	uint8_t  * inPixel  = pixel_buffer;
	uint32_t * outPixel = texture_buffer;

	for (unsigned y = 0; y < DISPLAY_Y; ++y) {
		for (unsigned x = 0; x < DISPLAY_X; ++x) {
			outPixel[x] = palette[inPixel[x]];
		}
		inPixel  += DISPLAY_X;
		outPixel += texture_pitch / sizeof(uint32_t);
	}

	SDL_UnlockTexture(the_texture);

	// Update display
	SDL_RenderClear(the_renderer);
	SDL_RenderCopy(the_renderer, the_texture, nullptr, nullptr);
	SDL_RenderPresent(the_renderer);
}


/*
 *  Fill rectangle in pixel buffer
 */

void C64Display::fill_rect(const SDL_Rect & r, uint8_t color) const
{
	uint8_t * p = pixel_buffer + DISPLAY_X*r.y + r.x;
	for (int y = 0; y < r.h; ++y) {
		for (int x = 0; x < r.w; ++x) {
			p[x] = color;
		}
		p += DISPLAY_X;
	}
}

/*
 *  Draw string into pixel buffer using the C64 ROM font
 */

void C64Display::draw_string(unsigned x, unsigned y, const char *str, uint8_t front_color) const
{
	uint8_t *pb = pixel_buffer + DISPLAY_X*y + x;
	char c;
	while ((c = *str++) != 0) {
		uint8_t *q = TheC64->Char + c*8 + 0x800;
		uint8_t *p = pb;
		for (unsigned y = 0; y < 8; y++) {
			uint8_t v = *q++;
			for (unsigned x = 0; x < 8; ++x) {
				if (v & 0x80) { p[x] = front_color; }
				v <<= 1;
			}
			p += DISPLAY_X;
		}
		pb += 8;
	}
}


/*
 *  LED error blink
 */

uint32_t C64Display::pulse_handler_static(uint32_t interval, void * arg)
{
	C64Display * disp = static_cast<C64Display *>(arg);
	disp->pulse_handler();
	return interval;
}

void C64Display::pulse_handler()
{
	for (int i = 0; i < 4; ++i) {
		switch (led_state[i]) {
			case LED_ERROR_ON:
				led_state[i] = LED_ERROR_OFF;
				break;
			case LED_ERROR_OFF:
				led_state[i] = LED_ERROR_ON;
				break;
		}
	}
}


/*
 *  Draw speedometer
 */

void C64Display::Speedometer(int speed)
{
	static int delay = 0;

	if (delay >= 20) {
		delay = 0;
		if (speed == 100) {
			speedometer_string[0] = '\0';  // hide if speed = 100%
		} else {
			sprintf(speedometer_string, "%d%%", speed);
		}
	} else {
		delay++;
	}
}


/*
 *  Return pointer to VIC bitmap data
 */

uint8_t *C64Display::BitmapBase()
{
	return pixel_buffer;
}


/*
 *  Return number of VIC bitmap bytes per row
 */

int C64Display::BitmapXMod()
{
	return DISPLAY_X;
}


/*
 *  Toggle fullscreen mode
 */

void C64Display::toggle_fullscreen(bool full)
{
	if (full) {
		SDL_SetWindowFullscreen(the_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_SetWindowFullscreen(the_window, 0);
		SDL_ShowCursor(SDL_ENABLE);
	}
}


/*
 *  Poll the keyboard
 */

static void translate_key(SDL_Scancode key, bool key_up, uint8_t *key_matrix, uint8_t *rev_matrix, uint8_t *joystick)
{
	int c64_key = -1;
	switch (key) {
		case SDL_SCANCODE_A: c64_key = MATRIX(1,2); break;
		case SDL_SCANCODE_B: c64_key = MATRIX(3,4); break;
		case SDL_SCANCODE_C: c64_key = MATRIX(2,4); break;
		case SDL_SCANCODE_D: c64_key = MATRIX(2,2); break;
		case SDL_SCANCODE_E: c64_key = MATRIX(1,6); break;
		case SDL_SCANCODE_F: c64_key = MATRIX(2,5); break;
		case SDL_SCANCODE_G: c64_key = MATRIX(3,2); break;
		case SDL_SCANCODE_H: c64_key = MATRIX(3,5); break;
		case SDL_SCANCODE_I: c64_key = MATRIX(4,1); break;
		case SDL_SCANCODE_J: c64_key = MATRIX(4,2); break;
		case SDL_SCANCODE_K: c64_key = MATRIX(4,5); break;
		case SDL_SCANCODE_L: c64_key = MATRIX(5,2); break;
		case SDL_SCANCODE_M: c64_key = MATRIX(4,4); break;
		case SDL_SCANCODE_N: c64_key = MATRIX(4,7); break;
		case SDL_SCANCODE_O: c64_key = MATRIX(4,6); break;
		case SDL_SCANCODE_P: c64_key = MATRIX(5,1); break;
		case SDL_SCANCODE_Q: c64_key = MATRIX(7,6); break;
		case SDL_SCANCODE_R: c64_key = MATRIX(2,1); break;
		case SDL_SCANCODE_S: c64_key = MATRIX(1,5); break;
		case SDL_SCANCODE_T: c64_key = MATRIX(2,6); break;
		case SDL_SCANCODE_U: c64_key = MATRIX(3,6); break;
		case SDL_SCANCODE_V: c64_key = MATRIX(3,7); break;
		case SDL_SCANCODE_W: c64_key = MATRIX(1,1); break;
		case SDL_SCANCODE_X: c64_key = MATRIX(2,7); break;
		case SDL_SCANCODE_Y: c64_key = MATRIX(3,1); break;
		case SDL_SCANCODE_Z: c64_key = MATRIX(1,4); break;

		case SDL_SCANCODE_0: c64_key = MATRIX(4,3); break;
		case SDL_SCANCODE_1: c64_key = MATRIX(7,0); break;
		case SDL_SCANCODE_2: c64_key = MATRIX(7,3); break;
		case SDL_SCANCODE_3: c64_key = MATRIX(1,0); break;
		case SDL_SCANCODE_4: c64_key = MATRIX(1,3); break;
		case SDL_SCANCODE_5: c64_key = MATRIX(2,0); break;
		case SDL_SCANCODE_6: c64_key = MATRIX(2,3); break;
		case SDL_SCANCODE_7: c64_key = MATRIX(3,0); break;
		case SDL_SCANCODE_8: c64_key = MATRIX(3,3); break;
		case SDL_SCANCODE_9: c64_key = MATRIX(4,0); break;

		case SDL_SCANCODE_SPACE: c64_key = MATRIX(7,4); break;
		case SDL_SCANCODE_GRAVE: c64_key = MATRIX(7,1); break;			// ←
		case SDL_SCANCODE_BACKSLASH: c64_key = MATRIX(6,6); break;		// ↑
		case SDL_SCANCODE_COMMA: c64_key = MATRIX(5,7); break;
		case SDL_SCANCODE_PERIOD: c64_key = MATRIX(5,4); break;
		case SDL_SCANCODE_MINUS: c64_key = MATRIX(5,0); break;			// +
		case SDL_SCANCODE_EQUALS: c64_key = MATRIX(5,3); break;			// -
		case SDL_SCANCODE_LEFTBRACKET: c64_key = MATRIX(5,6); break;	// @
		case SDL_SCANCODE_RIGHTBRACKET: c64_key = MATRIX(6,1); break;	// *
		case SDL_SCANCODE_SEMICOLON: c64_key = MATRIX(5,5); break;		// :
		case SDL_SCANCODE_APOSTROPHE: c64_key = MATRIX(6,2); break;		// ;
		case SDL_SCANCODE_SLASH: c64_key = MATRIX(6,7); break;

		case SDL_SCANCODE_ESCAPE: c64_key = MATRIX(7,7); break;			// RUN/STOP
		case SDL_SCANCODE_RETURN: c64_key = MATRIX(0,1); break;
		case SDL_SCANCODE_BACKSPACE:
		case SDL_SCANCODE_DELETE: c64_key = MATRIX(0,0); break;			// INS/DEL
		case SDL_SCANCODE_HOME: c64_key = MATRIX(6,3); break;			// CLR/HOME
		case SDL_SCANCODE_END: c64_key = MATRIX(6,0); break;			// £
		case SDL_SCANCODE_PAGEUP: c64_key = MATRIX(6,6); break;			// ↑
		case SDL_SCANCODE_PAGEDOWN: c64_key = MATRIX(6,5); break;		// =

		case SDL_SCANCODE_LCTRL:
		case SDL_SCANCODE_TAB:
		case SDL_SCANCODE_RCTRL: c64_key = MATRIX(7,2); break;
		case SDL_SCANCODE_LSHIFT: c64_key = MATRIX(1,7); break;
		case SDL_SCANCODE_RSHIFT: c64_key = MATRIX(6,4); break;
		case SDL_SCANCODE_LALT: c64_key = MATRIX(7,5); break;			// C=
		case SDL_SCANCODE_RALT: c64_key = MATRIX(7,5); break;			// C=

		case SDL_SCANCODE_UP: c64_key = MATRIX(0,7)| 0x80; break;
		case SDL_SCANCODE_DOWN: c64_key = MATRIX(0,7); break;
		case SDL_SCANCODE_LEFT: c64_key = MATRIX(0,2) | 0x80; break;
		case SDL_SCANCODE_RIGHT: c64_key = MATRIX(0,2); break;

		case SDL_SCANCODE_F1: c64_key = MATRIX(0,4); break;
		case SDL_SCANCODE_F2: c64_key = MATRIX(0,4) | 0x80; break;
		case SDL_SCANCODE_F3: c64_key = MATRIX(0,5); break;
		case SDL_SCANCODE_F4: c64_key = MATRIX(0,5) | 0x80; break;
		case SDL_SCANCODE_F5: c64_key = MATRIX(0,6); break;
		case SDL_SCANCODE_F6: c64_key = MATRIX(0,6) | 0x80; break;
		case SDL_SCANCODE_F7: c64_key = MATRIX(0,3); break;
		case SDL_SCANCODE_F8: c64_key = MATRIX(0,3) | 0x80; break;

		case SDL_SCANCODE_KP_0:
		case SDL_SCANCODE_KP_5: c64_key = 0x10 | 0x40; break;
		case SDL_SCANCODE_KP_1: c64_key = 0x06 | 0x40; break;
		case SDL_SCANCODE_KP_2: c64_key = 0x02 | 0x40; break;
		case SDL_SCANCODE_KP_3: c64_key = 0x0a | 0x40; break;
		case SDL_SCANCODE_KP_4: c64_key = 0x04 | 0x40; break;
		case SDL_SCANCODE_KP_6: c64_key = 0x08 | 0x40; break;
		case SDL_SCANCODE_KP_7: c64_key = 0x05 | 0x40; break;
		case SDL_SCANCODE_KP_8: c64_key = 0x01 | 0x40; break;
		case SDL_SCANCODE_KP_9: c64_key = 0x09 | 0x40; break;

		default: break;
	}

	if (c64_key < 0)
		return;

	// Handle joystick emulation
	if (c64_key & 0x40) {
		c64_key &= 0x1f;
		if (key_up) {
			*joystick |= c64_key;
		} else {
			*joystick &= ~c64_key;
		}
		return;
	}

	// Handle other keys
	bool shifted = c64_key & 0x80;
	int c64_byte = (c64_key >> 3) & 7;
	int c64_bit = c64_key & 7;
	if (key_up) {
		if (shifted) {
			key_matrix[6] |= 0x10;
			rev_matrix[4] |= 0x40;
		}
		key_matrix[c64_byte] |= (1 << c64_bit);
		rev_matrix[c64_bit] |= (1 << c64_byte);
	} else {
		if (shifted) {
			key_matrix[6] &= 0xef;
			rev_matrix[4] &= 0xbf;
		}
		key_matrix[c64_byte] &= ~(1 << c64_bit);
		rev_matrix[c64_bit] &= ~(1 << c64_byte);
	}
}

void C64Display::PollKeyboard(uint8_t *key_matrix, uint8_t *rev_matrix, uint8_t *joystick)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {

			// Key pressed
			case SDL_KEYDOWN:
				switch (event.key.keysym.scancode) {

					case SDL_SCANCODE_F10:	// F10: Prefs/Quit
						TheC64->RequestPrefsEditor();
						break;

					case SDL_SCANCODE_F11:	// F11: NMI (Restore)
						TheC64->NMI();
						break;

					case SDL_SCANCODE_F12:	// F12: Reset (hold Shift to clear memory)
						TheC64->Reset(SDL_GetModState() & KMOD_SHIFT);
						break;

					case SDL_SCANCODE_NUMLOCKCLEAR:
						num_locked = !num_locked;
						break;

					case SDL_SCANCODE_KP_ENTER:	// Enter on keypad: Toggle fullscreen
						if (ThePrefs.DisplayType == DISPTYPE_WINDOW) {
							ThePrefs.DisplayType = DISPTYPE_SCREEN;
							toggle_fullscreen(true);
						} else {
							ThePrefs.DisplayType = DISPTYPE_WINDOW;
							toggle_fullscreen(false);
						}
						break;

					case SDL_SCANCODE_KP_PLUS:	// Plus on keypad: Fast-forward while pressed
						if (TheC64->GetPlayMode() == PLAY_MODE_PLAY) {
							TheC64->SetPlayMode(PLAY_MODE_FORWARD);
						}
						break;

					case SDL_SCANCODE_KP_MINUS:	// Minus on keypad: Rewind while pressed
						if (TheC64->GetPlayMode() == PLAY_MODE_PLAY) {
							TheC64->SetPlayMode(PLAY_MODE_REWIND);
						}
						break;

					default:
						translate_key(event.key.keysym.scancode, false, key_matrix, rev_matrix, joystick);
						break;
				}
				break;

			// Key released
			case SDL_KEYUP:
				if (event.key.keysym.scancode == SDL_SCANCODE_KP_PLUS) {
					if (TheC64->GetPlayMode() == PLAY_MODE_FORWARD) {
						TheC64->SetPlayMode(PLAY_MODE_PLAY);
					}
				} else if (event.key.keysym.scancode == SDL_SCANCODE_KP_MINUS) {
					if (TheC64->GetPlayMode() == PLAY_MODE_REWIND) {
						TheC64->SetPlayMode(PLAY_MODE_PLAY);
					}
				} else {
					translate_key(event.key.keysym.scancode, true, key_matrix, rev_matrix, joystick);
				}
				break;

			// File dropped
			case SDL_DROPFILE: {
				char * filename = event.drop.file;
				int type;

				if (fs::is_directory(filename)) {

					// Turn off 1541 processor emulation and mount directory
					TheC64->SetEmul1541Proc(false, filename);

				} else if (IsMountableFile(filename, type)) {

					// Mount disk image file
					TheC64->SetEmul1541Proc(ThePrefs.Emul1541Proc, filename);

				} else if (IsSnapshotFile(filename)) {

					// Load snapshot
					TheC64->RequestLoadSnapshot(filename);
				}

				SDL_free(filename);
				break;
			}

			// Controller attached/removed
			case SDL_CONTROLLERDEVICEADDED:
				TheC64->JoystickAdded(event.cdevice.which);
				break;

			case SDL_CONTROLLERDEVICEREMOVED:
				TheC64->JoystickRemoved(event.cdevice.which);
				break;

			// Quit Frodo
			case SDL_QUIT:
				TheC64->RequestQuit();
				break;
		}
	}
}


/*
 *  Check if NumLock is down (for switching the joystick keyboard emulation)
 */

bool C64Display::NumLock()
{
	return num_locked;
}


/*
 *  Set VIC color palette
 */

void C64Display::init_colors(int palette_prefs)
{
	// Create palette for indexed-to-ARGB conversion
	memset(palette, 0, sizeof(palette));

	const uint8_t * r, * g, * b;
	if (palette_prefs == PALETTE_COLODORE) {
		r = palette_colodore_red;
		g = palette_colodore_green;
		b = palette_colodore_blue;
	} else {
		r = palette_pepto_red;
		g = palette_pepto_green;
		b = palette_pepto_blue;
	}

	// Colors for VIC
	for (unsigned i = 0; i < 16; ++i) {
		palette[i] = (r[i] << 16) | (g[i] << 8) | (b[i] << 0);
	}

	// Extra colors for UI elements
	palette[fill_gray]   = (0xd0 << 16) | (0xd0 << 8) | (0xd0 << 0);
	palette[shine_gray]  = (0xf0 << 16) | (0xf0 << 8) | (0xf0 << 0);
	palette[shadow_gray] = (0x80 << 16) | (0x80 << 8) | (0x80 << 0);
	palette[red]         = (0xf0 << 16) | (0x00 << 8) | (0x00 << 0);
	palette[green]       = (0x00 << 16) | (0xf0 << 8) | (0x00 << 0);
}


/*
 *  Show a requester (error message)
 */

long int ShowRequester(const char *a, const char *b, const char *)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, VERSION_STRING, a, c64_window);
	return 1;
}
