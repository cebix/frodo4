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

#ifdef __BEOS__
#include <InterfaceKit.h>
#endif

#ifdef HAVE_SDL
#include <SDL.h>
#endif

#ifdef WIN32
#include <ddraw.h>
#endif

#include <string>


// Display dimensions
#if defined(SMALL_DISPLAY)
constexpr int DISPLAY_X = 0x168;
constexpr int DISPLAY_Y = 0x110;
#else
constexpr int DISPLAY_X = 0x180;
constexpr int DISPLAY_Y = 0x110;
#endif


class C64Window;
class C64Screen;
class C64;
class Prefs;

// Class for C64 graphics display
class C64Display {
public:
	C64Display(C64 *the_c64);
	~C64Display();

	void Update(void);
	void UpdateLEDs(int l0, int l1, int l2, int l3);
	void Speedometer(int speed);
	uint8_t *BitmapBase(void);
	int BitmapXMod(void);
	void PollKeyboard(uint8_t *key_matrix, uint8_t *rev_matrix, uint8_t *joystick);
	bool NumLock(void);
	void InitColors(uint8_t *colors);
	void NewPrefs(Prefs *prefs);

	C64 *TheC64;

#ifdef __BEOS__
	void Pause(void);
	void Resume(void);
#endif

#ifdef HAVE_SDL
	bool quit_requested = false;
#endif

private:
	int led_state[4];
	int old_led_state[4];

#ifdef __BEOS__
	C64Window *the_window;	// One of these is NULL
	C64Screen *the_screen;
	bool using_screen;		// Flag: Using the_screen
	key_info old_key_info;
	int draw_bitmap;		// Number of bitmap for the VIC to draw into
#endif

#ifdef HAVE_SDL
	SDL_Window *the_window = nullptr;
	SDL_Renderer *the_renderer = nullptr;
	SDL_Texture *the_texture = nullptr;
	uint8_t * pixel_buffer = nullptr;	// Buffer for VIC to draw into
	uint32_t palette[256];				// Mapping of VIC color values to native ARGB

	char speedometer_string[16];		// Speedometer text
	SDL_TimerID pulse_timer = 0;		// Timer for LED error blinking

	bool num_locked = false;

	void error_and_quit(const std::string & msg) const;

	void fill_rect(const SDL_Rect & r, uint8_t color) const;
	void draw_string(unsigned x, unsigned y, const char *str, uint8_t front_color) const;

	static uint32_t pulse_handler_static(uint32_t interval, void * arg);
	void pulse_handler();
#endif

#ifdef WIN32
public:
	long ShowRequester(const char *str, const char *button1, const char *button2 = NULL);
	void WaitUntilActive();
	void NewPrefs();
	void Pause();
	void Resume();
	void Quit();

	struct DisplayMode {
		int x;
		int y;
		int depth;
		BOOL modex;
	};
	int GetNumDisplayModes() const;
	const DisplayMode *GetDisplayModes() const;

private:
	// Window members.
	void ResetKeyboardState();
	BOOL MakeWindow();
	static LRESULT CALLBACK StaticWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	long WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static int VirtKey2C64(int virtkey, DWORD keydata);
	BOOL CalcViewPort();
	BOOL SetupWindow();
	BOOL SetupWindowMode(BOOL full_screen);
	BOOL RestoreWindow();
	BOOL ResizeWindow(int side, RECT *pRect);
	void WindowTitle();
	void CreateObjects();
	void DeleteObjects();

	// DirectDraw management members.
	BOOL StartDirectDraw();
	BOOL ResumeDirectDraw();
	BOOL ResetDirectDraw();
	BOOL StopDirectDraw();
	static HRESULT CALLBACK EnumModesCallback(LPDDSURFACEDESC pDDSD, LPVOID lpContext);
	HRESULT EnumModesCallback(LPDDSURFACEDESC pDDSD);
	static int CompareModes(const void *e1, const void *e2);
	BOOL Fail(const char *message);

	// DirectDraw worker members.
	BOOL SetPalettes();
	BOOL BuildColorTable();
	BOOL CopySurface(RECT &rcWork);
	BOOL FlipSurfaces();
	BOOL EraseSurfaces();
	BOOL RestoreSurfaces();

	void draw_led_bar(void);		// Draw LED bar on the window
	void draw_leds(BOOL force = false);	// Draw LEDs if force or changed
	void led_rect(int n, RECT &rc, RECT &led); // Compute LED rectangle
	void InsertNextDisk();			// should be a common func
	BOOL FileNameDialog(char *prefs_path, BOOL save = false);
	void OfferSave();			// Offer chance to save changes

	UBYTE *chunky_buf;			// Chunky buffer for drawing
	BOOL active;				// is application active?
	BOOL paused;				// is application paused?
	BOOL waiting;				// is application waiting?
	DWORD windowed_style;			// style of windowed window
	DWORD fullscreen_style;			// style of fullscreen window
	char failure_message[128];		// what when wrong
	int speed_index;			// look ma, no hands
	BOOL show_leds;				// cached prefs option
	BOOL full_screen;			// cached prefs option
	BOOL in_constructor;			// if we are being contructed 
	BOOL in_destructor;			// if we are being destroyed

	LPDIRECTDRAW pDD;			// DirectDraw object
	LPDIRECTDRAWSURFACE pPrimary;		// DirectDraw primary surface
	LPDIRECTDRAWSURFACE pBack;		// DirectDraw back surface
	LPDIRECTDRAWSURFACE pWork;		// DirectDraw working surface
	LPDIRECTDRAWCLIPPER pClipper;		// DirectDraw clipper
	LPDIRECTDRAWPALETTE pPalette;		// DirectDraw palette

	DWORD colors[256];			// our palette colors
	int colors_depth;			// depth of the colors table
#endif
};


// Exported functions
extern long ShowRequester(const char *str, const char *button1, const char *button2 = NULL);


#endif
