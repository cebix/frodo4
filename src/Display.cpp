/*
 *  Display.cpp - C64 graphics display, emulator window handling
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

#include "Display.h"
#include "main.h"
#include "Prefs.h"


// LED states
enum {
	LED_OFF,		// LED off
	LED_ON,			// LED on (green)
	LED_ERROR_ON,	// LED blinking (red), currently on
	LED_ERROR_OFF	// LED blinking, currently off
};


#define USE_PEPTO_COLORS 1

#ifdef USE_PEPTO_COLORS

// C64 color palette
// Values based on measurements by Philip "Pepto" Timmermann <pepto@pepto.de>
// (see http://www.pepto.de/projects/colorvic/)
const uint8_t palette_red[16] = {
	0x00, 0xff, 0x86, 0x4c, 0x88, 0x35, 0x20, 0xcf, 0x88, 0x40, 0xcb, 0x34, 0x68, 0x8b, 0x68, 0xa1
};

const uint8_t palette_green[16] = {
	0x00, 0xff, 0x19, 0xc1, 0x17, 0xac, 0x07, 0xf2, 0x3e, 0x2a, 0x55, 0x34, 0x68, 0xff, 0x4a, 0xa1
};

const uint8_t palette_blue[16] = {
	0x00, 0xff, 0x01, 0xe3, 0xbd, 0x0a, 0xc0, 0x2d, 0x00, 0x00, 0x37, 0x34, 0x68, 0x59, 0xff, 0xa1
};

#else

// C64 color palette (traditional Frodo colors)
const uint8_t palette_red[16] = {
	0x00, 0xff, 0x99, 0x00, 0xcc, 0x44, 0x11, 0xff, 0xaa, 0x66, 0xff, 0x40, 0x80, 0x66, 0x77, 0xc0
};

const uint8_t palette_green[16] = {
	0x00, 0xff, 0x00, 0xff, 0x00, 0xcc, 0x00, 0xdd, 0x55, 0x33, 0x66, 0x40, 0x80, 0xff, 0x77, 0xc0
};

const uint8_t palette_blue[16] = {
	0x00, 0xff, 0x00, 0xcc, 0xcc, 0x44, 0x99, 0x00, 0x00, 0x00, 0x66, 0x40, 0x80, 0x66, 0xff, 0xc0
};

#endif


/*
 *  Update drive LED display (deferred until Update())
 */

void C64Display::UpdateLEDs(int l0, int l1, int l2, int l3)
{
	led_state[0] = l0;
	led_state[1] = l1;
	led_state[2] = l2;
	led_state[3] = l3;
}


#if defined(__BEOS__)
#include "Display_Be.h"
#elif defined(HAVE_SDL)
# if defined(QTOPIA) or defined(MAEMO)
#  include "Display_EmbeddedSDL.h"
# else
#  include "Display_SDL.h"
# endif
#elif defined(__unix)
# include "Display_x.h"
#elif defined(WIN32)
#include "Display_WIN32.h"
#endif
