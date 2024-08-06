/*
 *  DisplayEmbeddedSDL.h - C64 graphics display, emulator window handling
 *  for embedded Linux devices (Qtopia, Maemo)
 *
 *  Frodo (C) 1994-1997,2002-2009 Christian Bauer
 *  Changes for embedded Linux devices (Qtopia, Maemo) (C) 2006 Bernd Lachner
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
#include "SAM.h"
#include "main.h"
#include "Version.h"

#include "sdlgui.h"
#include "dlgMain.h"

#define QWS
#include <SDL.h>
#include <SDL_thread.h>

// Display surface
static SDL_Surface *screen = 0;
static SDL_Surface *backgroundSurf = 0;
static SDL_Surface *surf = 0;

static SDL_mutex *screenLock = 0;

// Mode of Joystick emulation. 0 = none, 1 = Joyport 1, 2 = Joyport 2
static short joy_emu = 0;

// Keyboard
static bool tab_pressed = false;

// For LED error blinking
static C64Display *c64_disp;
static struct sigaction pulse_sa;
static itimerval pulse_tv;

// SDL joysticks
static SDL_Joystick *joy[2] = {NULL, NULL};

// Colors for speedometer/drive LEDs
enum 
{
	black = 0,
	white = 1,
	fill_gray = 16,
	shine_gray = 17,
	shadow_gray = 18,
	red = 19,
	green = 20,
	PALETTE_SIZE = 21
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
  6     /   ^   =  SHR HOM  ;   *   
  7    R/S  Q   C= SPC  2  CTL  <-  1
*/

#define MATRIX(a,b) (((a) << 3) | (b))


/*
 *  Open window
 */

char *buffer;

int width = 320;
int height = 240;

bool isGuiAvailable = true; // TODO from main.cpp
bool GUIOpened = false;

static SDL_Thread *GUIthread = NULL;
static const int GUI_RETURN_INFO = (SDL_USEREVENT+1);
static bool doUpdate = true;

void update( int32 x, int32 y, int32 w, int32 h, bool forced )
{
	if ( !forced && !doUpdate ) // the HW surface is available
		return;

	//	SDL_UpdateRect(SDL_GetVideoSurface(), 0, 0, width, height);
	// SDL_UpdateRect(surf, x, y, w, h);
	SDL_UpdateRect(screen, x, y, w, h);
}

void update( bool forced )
{
	update( 0, 0, width, height, forced );
}

void update()
{
	update( 0, 0, width, height, false );
}

void restoreBackground()
{
	if (backgroundSurf != NULL) {
		SDL_BlitSurface(backgroundSurf, NULL, screen, NULL);
		update(true);
		fprintf(stderr, "video surface restored\n");
	}
}

void allocateBackgroundSurf()
{
	// allocate new background video surface
	backgroundSurf = SDL_ConvertSurface(screen, screen->format, screen->flags);
	fprintf(stderr, "Allocating background video surface\n");
}

void freeBackgroundSurf()
{
	// free background video surface
	if (backgroundSurf != NULL) 
	{
		fprintf(stderr, "Freeing background video surface\n");
		SDL_FreeSurface(backgroundSurf);
		backgroundSurf = NULL;
	}
}

void openGUI()
{
	fprintf(stderr, "open GUI\n");
	if (GUIOpened) 
	{
		fprintf(stderr, "GUI is already open!\n");
		return;
	}
	allocateBackgroundSurf();
	surf = backgroundSurf;
	GUIOpened = true;
}

void closeGUI()
{
	fprintf(stderr, "close GUI\n");
	// update the main surface and then redirect VDI to it
	restoreBackground();
	surf = screen;			// redirect VDI to main surface
	fprintf(stderr, "VDI redirected back to main video surface\n");
	freeBackgroundSurf();
	GUIOpened = false;
}

void saveBackground()
{
	if (backgroundSurf != NULL) {
		SDL_BlitSurface(screen, NULL, backgroundSurf, NULL);
		surf = backgroundSurf;	// redirect VDI to background surface
		fprintf(stderr, "video surface saved to background, VDI redirected\n");
	}
}

void blendBackgrounds()
{
	if (backgroundSurf != NULL) 
	{
		SDL_Rect *Rect;
		Rect = SDLGui_GetFirstBackgroundRect();
		while (Rect != NULL) 
		{
			SDL_BlitSurface(backgroundSurf, Rect, screen, Rect);
			Rect = SDLGui_GetNextBackgroundRect();
		}
		update(true);
	}
}

static Prefs DialogPrefs; 

// running in a different thread
static int open_gui(void * /*ptr*/)
{
	openGUI();
	DialogPrefs = ThePrefs;
	// Show main dialog
	int status = Dialog_Main(DialogPrefs);
	// The status is sent to event checking thread by the USEREVENT+1 message
	SDL_Event ev;
	ev.type = GUI_RETURN_INFO;
	ev.user.code = status;	// STATUS_SHUTDOWN or STATUS_REBOOT
	ev.user.data1 = NULL;
	SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_EVENTMASK(GUI_RETURN_INFO));
	closeGUI();
	return 0;
}

bool start_GUI_thread()
{
	if (isGuiAvailable) // TODO && !hostScreen.isGUIopen()) 
	{
		GUIthread = SDL_CreateThread(open_gui, NULL);
	}
	return (GUIthread != NULL);
}

void kill_GUI_thread()
{
	if (GUIthread != NULL) {
		SDL_KillThread(GUIthread);
		GUIthread = NULL;
	}
}

void screenlock() 
{
	while (SDL_mutexP(screenLock)==-1) 
	{
		SDL_Delay(20);
		fprintf(stderr, "Couldn't lock mutex\n");
	}
}

void screenunlock() 
{
	while (SDL_mutexV(screenLock)==-1) 
	{
		SDL_Delay(20);
		fprintf(stderr, "Couldn't unlock mutex\n");
	}
}

int init_graphics(void)
{
	// Init SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Couldn't initialize SDL (%s)\n", SDL_GetError());
		return 0;
	}

	screenLock = SDL_CreateMutex();

	buffer = new char[DISPLAY_X*DISPLAY_Y];
	// Open window
	SDL_WM_SetCaption(VERSION_STRING, "Frodo");
	screen = SDL_SetVideoMode(320, 240, 8, SDL_DOUBLEBUF);
	surf = screen;
	if (screen == NULL)
	{
		fprintf(stderr, "SDL Couldn't set video mode to %d x %d\n", DISPLAY_X, DISPLAY_Y+17);
	}
	else
	{
		fprintf(stderr, "SDL Set video mode to %d x %d\n", DISPLAY_X, DISPLAY_Y+17);
		SDLGui_Init(screen);
		start_GUI_thread();
	}
	return 1;
}



/*
 *  Display constructor
 */

C64Display::C64Display(C64 *the_c64) : TheC64(the_c64)
{
	quit_requested = false;
	speedometer_string[0] = 0;

	// LEDs off
	for (int i=0; i<4; i++)
		led_state[i] = old_led_state[i] = LED_OFF;

	// Start timer for LED error blinking
	c64_disp = this;
	pulse_sa.sa_handler = (void (*)(int))pulse_handler;
	pulse_sa.sa_flags = SA_RESTART;
	sigemptyset(&pulse_sa.sa_mask);
	sigaction(SIGALRM, &pulse_sa, NULL);
	pulse_tv.it_interval.tv_sec = 0;
	pulse_tv.it_interval.tv_usec = 400000;
	pulse_tv.it_value.tv_sec = 0;
	pulse_tv.it_value.tv_usec = 400000;
	setitimer(ITIMER_REAL, &pulse_tv, NULL);
}


/*
 *  Display destructor
 */

C64Display::~C64Display()
{
	SDL_Quit();
	delete[] buffer;
}


/*
 *  Prefs may have changed
 */

void C64Display::NewPrefs(Prefs *prefs)
{
}


/*
 *  Redraw bitmap
 */

void C64Display::Update(void)
{
	screenlock();
	if (surf == NULL)
		return;
	int iOffsetX = (DISPLAY_X - surf->w) / 2;
	int iOffsetY = (DISPLAY_Y - surf->h) / 2;
	for (int j=0; j < surf->h - 17; j++)
	{
		memcpy(static_cast<char*>(surf->pixels)+surf->w*j, buffer+iOffsetX+DISPLAY_X*(j+iOffsetY), surf->w);
	}
	// Draw speedometer/LEDs
	SDL_Rect r = {0, (surf->h - 17), DISPLAY_X, 15};
	SDL_FillRect(surf, &r, fill_gray);
	r.w = DISPLAY_X; r.h = 1;
	SDL_FillRect(surf, &r, shine_gray);
	r.y = (surf->h - 17) + 14;
	SDL_FillRect(surf, &r, shadow_gray);
	r.w = 16;
	for (int i=2; i<5; i++) 
	{
		r.x = DISPLAY_X * i/5 - 24; r.y = (surf->h - 17) + 4;
		SDL_FillRect(surf, &r, shadow_gray);
		r.y = (surf->h - 17) + 10;
		SDL_FillRect(surf, &r, shine_gray);
	}
	r.y = (surf->h - 17); r.w = 1; r.h = 15;
	for (int i=0; i<4; i++) 
	{
		r.x = DISPLAY_X * i / 5;
		SDL_FillRect(surf, &r, shine_gray);
		r.x = DISPLAY_X * (i+1) / 5 - 1;
		SDL_FillRect(surf, &r, shadow_gray);
	}
	r.y = (surf->h - 17) + 4; r.h = 7;
	for (int i=2; i<5; i++)
	{
		r.x = DISPLAY_X * i/5 - 24;
		SDL_FillRect(surf, &r, shadow_gray);
		r.x = DISPLAY_X * i/5 - 9;
		SDL_FillRect(surf, &r, shine_gray);
	}
	r.y = (surf->h - 17) + 5; r.w = 14; r.h = 5;
	for (int i=0; i<3; i++) 
	{
		r.x = DISPLAY_X * (i+2) / 5 - 23;
		int c;
		switch (led_state[i]) 
		{
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
		SDL_FillRect(surf, &r, c);
	}

	draw_string(surf, DISPLAY_X * 1/5 + 8, (surf->h - 17) + 4, "D\x12 8", black, fill_gray);
	draw_string(surf, DISPLAY_X * 2/5 + 8, (surf->h - 17) + 4, "D\x12 9", black, fill_gray);
	draw_string(surf, DISPLAY_X * 3/5 + 8, (surf->h - 17) + 4, "D\x12 10", black, fill_gray);
	if (joy_emu == 1)
		draw_string(surf, DISPLAY_X * 4/5 + 2, (surf->h - 17) + 4, "1", black, fill_gray);
	else if (joy_emu == 2)
		draw_string(surf, DISPLAY_X * 4/5 + 2, (surf->h - 17) + 4, "2", black, fill_gray);
	draw_string(surf, 24, (surf->h - 17) + 4, speedometer_string, black, fill_gray);

	// Update display
	SDL_Flip(surf);

	blendBackgrounds();
	screenunlock();
}


/*
 *  Draw string into surface using the C64 ROM font
 */

void C64Display::draw_string(SDL_Surface *s, int x, int y, const char *str, uint8 front_color, uint8 back_color)
{
	uint8 *pb = (uint8 *)s->pixels + s->pitch*y + x;
	char c;
	while ((c = *str++) != 0) 
	{
		uint8 *q = TheC64->Char + c*8 + 0x800;
		uint8 *p = pb;
		for (int y=0; y<8; y++) 
		{
			uint8 v = *q++;
			p[0] = (v & 0x80) ? front_color : back_color;
			p[1] = (v & 0x40) ? front_color : back_color;
			p[2] = (v & 0x20) ? front_color : back_color;
			p[3] = (v & 0x10) ? front_color : back_color;
			p[4] = (v & 0x08) ? front_color : back_color;
			p[5] = (v & 0x04) ? front_color : back_color;
			p[6] = (v & 0x02) ? front_color : back_color;
			p[7] = (v & 0x01) ? front_color : back_color;
			p += s->pitch;
		}
		pb += 8;
	}
}


/*
 *  LED error blink
 */

void C64Display::pulse_handler(...)
{
	for (int i=0; i<4; i++)
		switch (c64_disp->led_state[i]) 
		{
			case LED_ERROR_ON:
				c64_disp->led_state[i] = LED_ERROR_OFF;
				break;
			case LED_ERROR_OFF:
				c64_disp->led_state[i] = LED_ERROR_ON;
				break;
		}
}


/*
 *  Draw speedometer
 */

void C64Display::Speedometer(int speed)
{
	static int delay = 0;

	if (delay >= 20)
	{
		delay = 0;
		sprintf(speedometer_string, "%d%%", speed);
	} 
	else
		delay++;
}


/*
 *  Return pointer to bitmap data
 */

uint8 *C64Display::BitmapBase(void)
{
	//return (uint8 *)screen->pixels;
	return (uint8 *)buffer;
}


/*
 *  Return number of bytes per row
 */

int C64Display::BitmapXMod(void)
{
	//return screen->pitch;
	return DISPLAY_X;
}



/*
 *  Poll the keyboard
 */

static void translate_key(SDLKey key, bool key_up, uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
	int c64_key = -1;
	if (tab_pressed)
	{
		// Function and run/stop key emulation on Zaurus keyboard
		switch (key)
		{
			case SDLK_q: c64_key = MATRIX(0,4); break;
			case SDLK_w: c64_key = MATRIX(0,4) | 0x80; break;
			case SDLK_e: c64_key = MATRIX(0,5); break;
			case SDLK_r: c64_key = MATRIX(0,5) | 0x80; break;
			case SDLK_t: c64_key = MATRIX(0,6); break;
			case SDLK_z: c64_key = MATRIX(0,6) | 0x80; break;
			case SDLK_u: c64_key = MATRIX(0,3); break;
			case SDLK_i: c64_key = MATRIX(0,3) | 0x80; break;
			case SDLK_o: c64_key = MATRIX(7,7); break;
		}
	}
	else
	{
		switch (key)
		{
			case SDLK_a: c64_key = MATRIX(1,2); break;
			case SDLK_b: c64_key = MATRIX(3,4); break;
			case SDLK_c: c64_key = MATRIX(2,4); break;
			case SDLK_d: c64_key = MATRIX(2,2); break;
			case SDLK_e: c64_key = MATRIX(1,6); break;
			case SDLK_f: c64_key = MATRIX(2,5); break;
			case SDLK_g: c64_key = MATRIX(3,2); break;
			case SDLK_h: c64_key = MATRIX(3,5); break;
			case SDLK_i: c64_key = MATRIX(4,1); break;
			case SDLK_j: c64_key = MATRIX(4,2); break;
			case SDLK_k: c64_key = MATRIX(4,5); break;
			case SDLK_l: c64_key = MATRIX(5,2); break;
			case SDLK_m: c64_key = MATRIX(4,4); break;
			case SDLK_n: c64_key = MATRIX(4,7); break;
			case SDLK_o: c64_key = MATRIX(4,6); break;
			case SDLK_p: c64_key = MATRIX(5,1); break;
			case SDLK_q: c64_key = MATRIX(7,6); break;
			case SDLK_r: c64_key = MATRIX(2,1); break;
			case SDLK_s: c64_key = MATRIX(1,5); break;
			case SDLK_t: c64_key = MATRIX(2,6); break;
			case SDLK_u: c64_key = MATRIX(3,6); break;
			case SDLK_v: c64_key = MATRIX(3,7); break;
			case SDLK_w: c64_key = MATRIX(1,1); break;
			case SDLK_x: c64_key = MATRIX(2,7); break;
			case SDLK_y: c64_key = MATRIX(3,1); break;
			case SDLK_z: c64_key = MATRIX(1,4); break;

			case SDLK_0: c64_key = MATRIX(4,3); break;
			case SDLK_1: c64_key = MATRIX(7,0); break;
			case SDLK_2: c64_key = MATRIX(7,3); break;
			case SDLK_3: c64_key = MATRIX(1,0); break;
			case SDLK_4: c64_key = MATRIX(1,3); break;
			case SDLK_5: c64_key = MATRIX(2,0); break;
			case SDLK_6: c64_key = MATRIX(2,3); break;
			case SDLK_7: c64_key = MATRIX(3,0); break;
			case SDLK_8: c64_key = MATRIX(3,3); break;
			case SDLK_9: c64_key = MATRIX(4,0); break;

			case SDLK_SPACE: c64_key = MATRIX(7,4); break;
			case SDLK_BACKQUOTE: c64_key = MATRIX(7,1); break;
			case SDLK_BACKSLASH: c64_key = MATRIX(6,6); break;
			case SDLK_COMMA: c64_key = MATRIX(5,7); break;
			case SDLK_PERIOD: c64_key = MATRIX(5,4); break;
			case SDLK_MINUS: c64_key = MATRIX(5,0); break;
			case SDLK_EQUALS: c64_key = MATRIX(5,3); break;
			case SDLK_LEFTBRACKET: c64_key = MATRIX(5,6); break;
			case SDLK_RIGHTBRACKET: c64_key = MATRIX(6,1); break;
			case SDLK_SEMICOLON: c64_key = MATRIX(5,5); break;
			case SDLK_QUOTE: c64_key = MATRIX(6,2); break;
			case SDLK_SLASH: c64_key = MATRIX(6,7); break;

			case SDLK_ESCAPE: c64_key = MATRIX(7,7); break;
			case SDLK_RETURN: c64_key = MATRIX(0,1); break;
			case SDLK_BACKSPACE: case SDLK_DELETE: c64_key = MATRIX(0,0); break;
			case SDLK_INSERT: c64_key = MATRIX(6,3); break;
			case SDLK_HOME: c64_key = MATRIX(6,3); break;
			case SDLK_END: c64_key = MATRIX(6,0); break;
			case SDLK_PAGEUP: c64_key = MATRIX(6,0); break;
			case SDLK_PAGEDOWN: c64_key = MATRIX(6,5); break;

			case SDLK_LCTRL: c64_key = MATRIX(7,2); break;
			case SDLK_RCTRL: c64_key = MATRIX(7,5); break;
			case SDLK_LSHIFT: c64_key = MATRIX(1,7); break;
			case SDLK_RSHIFT: c64_key = MATRIX(6,4); break;
			case SDLK_LALT: case SDLK_LMETA: c64_key = MATRIX(7,5); break;
			case SDLK_RALT: case SDLK_RMETA: c64_key = MATRIX(7,5); break;

			case SDLK_UP: c64_key = MATRIX(0,7)| 0x80; break;
			case SDLK_DOWN: c64_key = MATRIX(0,7); break;
			case SDLK_LEFT: c64_key = MATRIX(0,2) | 0x80; break;
			case SDLK_RIGHT: c64_key = MATRIX(0,2); break;

			case SDLK_F1: c64_key = MATRIX(0,4); break;
			case SDLK_F2: c64_key = MATRIX(0,4) | 0x80; break;
			case SDLK_F3: c64_key = MATRIX(0,5); break;
			case SDLK_F4: c64_key = MATRIX(0,5) | 0x80; break;
			case SDLK_F5: c64_key = MATRIX(0,6); break;
			case SDLK_F6: c64_key = MATRIX(0,6) | 0x80; break;
			case SDLK_F7: c64_key = MATRIX(0,3); break;
			case SDLK_F8: c64_key = MATRIX(0,3) | 0x80; break;

			case SDLK_KP0: case SDLK_KP5: c64_key = 0x10 | 0x40; break;
			case SDLK_KP1: c64_key = 0x06 | 0x40; break;
			case SDLK_KP2: c64_key = 0x02 | 0x40; break;
			case SDLK_KP3: c64_key = 0x0a | 0x40; break;
			case SDLK_KP4: c64_key = 0x04 | 0x40; break;
			case SDLK_KP6: c64_key = 0x08 | 0x40; break;
			case SDLK_KP7: c64_key = 0x05 | 0x40; break;
			case SDLK_KP8: c64_key = 0x01 | 0x40; break;
			case SDLK_KP9: c64_key = 0x09 | 0x40; break;

			case SDLK_KP_DIVIDE: c64_key = MATRIX(6,7); break;
			case SDLK_KP_ENTER: c64_key = MATRIX(0,1); break;

			// Support for Zaurus/Qtopia
			case SDLK_QUOTEDBL: c64_key = MATRIX(7,3) | 0x80; break;
			case SDLK_ASTERISK: c64_key = MATRIX(6,1); break;
			case SDLK_DOLLAR: c64_key = MATRIX(1,3) | 0x80; break;
			case SDLK_COLON: c64_key = MATRIX(5,5); break;
			case SDLK_AT: c64_key = MATRIX(5,6); break;
		}
	}
	if (c64_key < 0)
		return;

	// Zaurus/Qtopia joystick emulation
	if (joy_emu != 0)
	{
		switch (key)
		{
			case SDLK_SPACE: c64_key = 0x10 | 0x40; break;
			case SDLK_UP: c64_key = 0x01 | 0x40; break;
			case SDLK_DOWN: c64_key = 0x02 | 0x40; break;
			case SDLK_LEFT: c64_key =  0x04 | 0x40; break;
			case SDLK_RIGHT: c64_key = 0x08 | 0x40; break;
		}
	}

	// Handle joystick emulation
	if (c64_key & 0x40) 
	{
		c64_key &= 0x1f;
		if (key_up)
			*joystick |= c64_key;
		else
			*joystick &= ~c64_key;
		return;
	}

	// Handle other keys
	bool shifted = c64_key & 0x80;
	int c64_byte = (c64_key >> 3) & 7;
	int c64_bit = c64_key & 7;
	if (key_up) 
	{
		if (shifted) 
		{
			key_matrix[6] |= 0x10;
			rev_matrix[4] |= 0x40;
		}
		key_matrix[c64_byte] |= (1 << c64_bit);
		rev_matrix[c64_bit] |= (1 << c64_byte);
	} 
	else 
	{
		if (shifted)
		{
			key_matrix[6] &= 0xef;
			rev_matrix[4] &= 0xbf;
		}
		key_matrix[c64_byte] &= ~(1 << c64_bit);
		rev_matrix[c64_bit] &= ~(1 << c64_byte);
	}
}

void C64Display::PollKeyboard(uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
	SDL_Event event;
	int eventmask = SDL_EVENTMASK(SDL_KEYDOWN)
					| SDL_EVENTMASK(SDL_KEYUP)
					| SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN)
					| SDL_EVENTMASK(SDL_MOUSEBUTTONUP)
					| SDL_EVENTMASK(SDL_MOUSEMOTION)
					| SDL_EVENTMASK(SDL_JOYAXISMOTION)
					| SDL_EVENTMASK(SDL_JOYHATMOTION)
					| SDL_EVENTMASK(SDL_JOYBUTTONDOWN)
					| SDL_EVENTMASK(SDL_JOYBUTTONUP)
					| SDL_EVENTMASK(SDL_ACTIVEEVENT)
					| SDL_EVENTMASK(GUI_RETURN_INFO)
					| SDL_EVENTMASK(SDL_QUIT);

	SDL_PumpEvents();
	while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, eventmask))
	{
		if (GUIOpened) 
		{
			if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) 
			{
				if (event.type == SDL_MOUSEBUTTONDOWN)
					fprintf(stderr, "Mouse down\n");
				if (event.type == SDL_MOUSEBUTTONUP)
					fprintf(stderr, "Mouse up\n");
				SDL_Event ev;
				ev.type = SDL_USEREVENT;	// map button down/up to user event
				ev.user.code = event.type;
				ev.user.data1 = (void *)(int)event.button.x;
				ev.user.data2 = (void *)(int)event.button.y;
				SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_EVENTMASK(SDL_USEREVENT));
			}
			else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
			{
				if (event.type == SDL_KEYDOWN)
					fprintf(stderr, "Key down\n");
				if (event.type == SDL_KEYUP)
					fprintf(stderr, "Key up\n");
				SDLKey sym = event.key.keysym.sym;
				int state = SDL_GetModState(); // keysym.mod does not deliver single mod key presses for 
				SDL_Event ev;
				ev.type = SDL_USEREVENT;	// map key down/up event to user event
				ev.user.code = event.type;
				ev.user.data1 = (void *)(int)sym;
				ev.user.data2 = (void *)(int)state;
				SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_EVENTMASK(SDL_USEREVENT));
			}
		}
		else
		{
		switch (event.type)
		{
			case GUI_RETURN_INFO:
			{
				fprintf(stderr, "Return code from gui: %d\n", event.user.code);
				switch (event.user.code)
				{
					case DO_RESET:
						TheC64->Reset();
						break;
					case DO_QUIT:
						quit_requested = true;
						break;
					case DO_USEPREFS:
						TheC64->NewPrefs(&DialogPrefs);
						ThePrefs = DialogPrefs;
						break;
					case DO_SAVEPREFS:
						TheC64->NewPrefs(&DialogPrefs);
						ThePrefs = DialogPrefs;
						ThePrefs.Save(Frodo::get_prefs_path());
						break;
				}
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				start_GUI_thread();
				break;
			// Key pressed
			case SDL_KEYDOWN:
//				fprintf(stderr, "SDL-Key: %d\n", event.key.keysym.sym);
				if (tab_pressed && event.key.keysym.sym == SDLK_j)
				{
					if (joy_emu < 2)
						joy_emu++;
					else
						joy_emu = 0;
				}
				if (tab_pressed && event.key.keysym.sym == SDLK_p)
				{
					//  NMI (Restore)
					TheC64->NMI();
				}
				else
				{
					switch (event.key.keysym.sym)
					{
						case SDLK_TAB:
							tab_pressed = true;
							break;

						case SDLK_F9:	// F9: Invoke SAM
							SAM(TheC64);
							break;

						case SDLK_F11:	// F10: Quit
							// Iconify not implemented in Qtopia SDL yet. Quit instead show gui.
							//SDL_WM_IconifyWindow();
							quit_requested = true;
							break;

						case SDLK_F12:	// F12: Reset
							TheC64->Reset();
							break;

						case SDLK_KP_PLUS:	// '+' on keypad: Increase SkipFrames
							ThePrefs.SkipFrames++;
							break;

						case SDLK_KP_MINUS:	// '-' on keypad: Decrease SkipFrames
							if (ThePrefs.SkipFrames > 1)
								ThePrefs.SkipFrames--;
							break;

						case SDLK_KP_MULTIPLY:	// '*' on keypad: Toggle speed limiter
							ThePrefs.LimitSpeed = !ThePrefs.LimitSpeed;
							break;

						default:
							translate_key(event.key.keysym.sym, false, key_matrix, rev_matrix, joystick);
							break;
					}
				}
				break;

			// Key released
			case SDL_KEYUP:
				if (event.key.keysym.sym == SDLK_TAB)
					tab_pressed = false;
				else
{
					translate_key(event.key.keysym.sym, true, key_matrix, rev_matrix, joystick);
}
				break;

			// Quit Frodo
			case SDL_QUIT:
				quit_requested = true;
				break;
		}
		}
	}
}


/*
 *  Check if NumLock is down (for switching the joystick keyboard emulation)
 */

bool C64Display::NumLock(void)
{
	if (joy_emu == 2)
		return true;
	return false;
}


/*
 *  Open/close joystick drivers given old and new state of
 *  joystick preferences
 */

void C64::open_close_joystick(int port, int oldjoy, int newjoy)
{
	if (oldjoy != newjoy) {
		joy_minx[port] = joy_miny[port] = 32767;	// Reset calibration
		joy_maxx[port] = joy_maxy[port] = -32768;
		if (newjoy) {
			joy[port] = SDL_JoystickOpen(newjoy - 1);
			if (joy[port] == NULL)
				fprintf(stderr, "Couldn't open joystick %d\n", port + 1);
		} else {
			if (joy[port]) {
				SDL_JoystickClose(joy[port]);
				joy[port] = NULL;
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

uint8 C64::poll_joystick(int port)
{
	uint8 j = 0xff;

	if (port == 0 && (joy[0] || joy[1]))
		SDL_JoystickUpdate();

	if (joy[port]) {
		int x = SDL_JoystickGetAxis(joy[port], 0), y = SDL_JoystickGetAxis(joy[port], 1);

		if (x > joy_maxx[port])
			joy_maxx[port] = x;
		if (x < joy_minx[port])
			joy_minx[port] = x;
		if (y > joy_maxy[port])
			joy_maxy[port] = y;
		if (y < joy_miny[port])
			joy_miny[port] = y;

		if (joy_maxx[port] - joy_minx[port] < 100 || joy_maxy[port] - joy_miny[port] < 100)
			return 0xff;

		if (x < (joy_minx[port] + (joy_maxx[port]-joy_minx[port])/3))
			j &= 0xfb;							// Left
		else if (x > (joy_minx[port] + 2*(joy_maxx[port]-joy_minx[port])/3))
			j &= 0xf7;							// Right

		if (y < (joy_miny[port] + (joy_maxy[port]-joy_miny[port])/3))
			j &= 0xfe;							// Up
		else if (y > (joy_miny[port] + 2*(joy_maxy[port]-joy_miny[port])/3))
			j &= 0xfd;							// Down

		if (SDL_JoystickGetButton(joy[port], 0))
			j &= 0xef;							// Button
	}

	return j;
}

/*
 *  Allocate C64 colors
 */

void C64Display::InitColors(uint8 *colors)
{
	SDL_Color palette[PALETTE_SIZE];
	for (int i=0; i<16; i++)
	{
		palette[i].r = palette_red[i];
		palette[i].g = palette_green[i];
		palette[i].b = palette_blue[i];
	}
	palette[fill_gray].r = palette[fill_gray].g = palette[fill_gray].b = 0xd0;
	palette[shine_gray].r = palette[shine_gray].g = palette[shine_gray].b = 0xf0;
	palette[shadow_gray].r = palette[shadow_gray].g = palette[shadow_gray].b = 0x80;
	palette[red].r = 0xf0;
	palette[red].g = palette[red].b = 0;
	palette[green].g = 0xf0;
	palette[green].r = palette[green].b = 0;
	SDL_SetColors(screen, palette, 0, PALETTE_SIZE);

	for (int i=0; i<256; i++)
		colors[i] = i & 0x0f;
}


/*
 *  Show a requester (error message)
 */

long int ShowRequester(const char *a, const char *b, const char *)
{
	printf("%s: %s\n", a, b);
	return 1;
}
