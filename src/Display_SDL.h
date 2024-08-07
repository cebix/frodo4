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
#include "SAM.h"
#include "Version.h"

#include <SDL.h>

#ifdef ENABLE_OPENGL
#include <GL/glew.h>
#endif


// Display dimensions including drive LEDs etc.
static const int FRAME_WIDTH = DISPLAY_X;
static const int FRAME_HEIGHT = DISPLAY_Y + 16;

// Display surface
static SDL_Surface *screen = NULL;

// Keyboard
static bool num_locked = false;

// For LED error blinking
static C64Display *c64_disp;
static struct sigaction pulse_sa;
static itimerval pulse_tv;

// SDL joysticks
static SDL_Joystick *joy[2] = {NULL, NULL};

// Colors for speedometer/drive LEDs
enum {
	black = 0,
	white = 1,
	fill_gray = 16,
	shine_gray = 17,
	shadow_gray = 18,
	red = 19,
	green = 20,
	PALETTE_SIZE = 21
};

#ifdef ENABLE_OPENGL

// Display texture dimensions
static const int TEXTURE_SIZE = 512;  // smallest power-of-two that fits DISPLAY_X/Y

// Texture object for VIC palette
static GLuint palette_tex;

// Texture object for VIC display
static GLuint vic_tex;

#endif

/*
  C64 keyboard matrix:

    Bit 7   6   5   4   3   2   1   0
  0    CUD  F5  F3  F1  F7 CLR RET DEL
  1    SHL  E   S   Z   4   A   W   3
  2     X   T   F   C   6   D   R   5
  3     V   U   H   B   8   G   Y   7
  4     N   O   K   M   0   J   I   9
  5     ,   @   :   .   -   L   P   +
  6     /   ^   =  SHR HOM  ;   *   £
  7    R/S  Q   C= SPC  2  CTL  <-  1
*/

#define MATRIX(a,b) (((a) << 3) | (b))


/*
 *  Open window
 */

int init_graphics(void)
{
	// Init SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Couldn't initialize SDL (%s)\n", SDL_GetError());
		return 0;
	}

	return 1;
}


#ifdef ENABLE_OPENGL
/*
 *  Set direct projection (GL coordinates = window pixel coordinates)
 */

static void set_projection()
{
	int width = SDL_GetVideoSurface()->w;
	int height = SDL_GetVideoSurface()->h;

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float aspect = float(width) / float(height);
	const float want_aspect = float(FRAME_WIDTH) / float(FRAME_HEIGHT);
	int left, right, top, bottom;
	if (aspect > want_aspect) {
		// Window too wide, center horizontally
		top = 0; bottom = FRAME_HEIGHT;
		int diff = (int(FRAME_WIDTH * aspect / want_aspect) - FRAME_WIDTH) / 2;
		left = -diff;
		right = FRAME_WIDTH + diff;
	} else {
		// Window too high, center vertically
		left = 0; right = FRAME_WIDTH;
		int diff = (int(FRAME_HEIGHT * want_aspect / aspect) - FRAME_HEIGHT) / 2;
		top = -diff;
		bottom = FRAME_HEIGHT + diff;
	}
	glOrtho(left, right, bottom, top, -1, 1);

	glClear(GL_COLOR_BUFFER_BIT);
}


/*
 *  User resized video display (only possible with OpenGL)
 */

static void video_resized(int width, int height)
{
	uint32 flags = (ThePrefs.DisplayType == DISPTYPE_SCREEN ? SDL_FULLSCREEN : 0);
	flags |= (SDL_ANYFORMAT | SDL_OPENGL | SDL_RESIZABLE);
	SDL_SetVideoMode(width, height, 16, flags);
	set_projection();
}
#endif


/*
 *  Display constructor
 */

C64Display::C64Display(C64 *the_c64) : TheC64(the_c64)
{
	quit_requested = false;
	speedometer_string[0] = 0;

	// Open window
	SDL_WM_SetCaption(VERSION_STRING, "Frodo");
	uint32 flags = (ThePrefs.DisplayType == DISPTYPE_SCREEN ? SDL_FULLSCREEN : 0);

#ifdef ENABLE_OPENGL

	flags |= (SDL_ANYFORMAT | SDL_OPENGL | SDL_RESIZABLE);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Surface *real_screen = SDL_SetVideoMode(FRAME_WIDTH * 2, FRAME_HEIGHT * 2, 16, flags);
	if (!real_screen) {
		fprintf(stderr, "Couldn't initialize OpenGL video output (%s)\n", SDL_GetError());
		exit(1);
	}

	// VIC display and UI elements are rendered into an off-screen surface
	screen = SDL_CreateRGBSurface(SDL_SWSURFACE, FRAME_WIDTH, FRAME_HEIGHT, 8, 0xff, 0xff, 0xff, 0xff);

	// We need OpenGL 2.0 or higher
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		fprintf(stderr, "Couldn't initialize GLEW (%s)\n", glewGetErrorString(err));
		exit(1);
	}

	if (!glewIsSupported("GL_VERSION_2_0")) {
		fprintf(stderr, "Frodo requires OpenGL 2.0 or higher\n");
		exit(1);
	}

	// Set direct projection
	set_projection();

	// Set GL state
	glShadeModel(GL_FLAT);
	glDisable(GL_DITHER);
	glColor4f(1.0, 1.0, 1.0, 1.0);

	// Create fragment shader for emulating a paletted texture
	GLuint shader = glCreateShader(GL_FRAGMENT_SHADER_ARB);
	const char * src =
		"uniform sampler2D screen;"
		"uniform sampler1D palette;"
		"const float texSize = 512.0;"  // texture size
		"const float texel = 1.0 / texSize;"
		"void main()"
		"{"
#if 0
		// Nearest neighbour
		"  vec4 idx = texture2D(screen, gl_TexCoord[0].st);"
		"  gl_FragColor = texture1D(palette, idx.r);"
#else
		// Linear interpolation
		// (setting the GL_TEXTURE_MAG_FILTER to GL_LINEAR would interpolate
		// the color indices which is not what we want; we need to manually
		// interpolate the palette values instead)
		"  vec2 st = gl_TexCoord[0].st - vec2(texel * 0.5, texel * 0.5);"
		"  vec4 idx00 = texture2D(screen, st);"
		"  vec4 idx01 = texture2D(screen, st + vec2(0, texel));"
		"  vec4 idx10 = texture2D(screen, st + vec2(texel, 0));"
		"  vec4 idx11 = texture2D(screen, st + vec2(texel, texel));"
		"  vec2 f = fract(st * texSize);"
		"  vec4 color0 = mix(texture1D(palette, idx00.r), texture1D(palette, idx01.r), f.y);"
		"  vec4 color1 = mix(texture1D(palette, idx10.r), texture1D(palette, idx11.r), f.y);"
		"  gl_FragColor = mix(color0, color1, f.x);"
#endif
		"}";
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			GLchar *log = (GLchar *)malloc(logLength);
			GLint actual;
			glGetShaderInfoLog(shader, logLength, &actual, log);
			fprintf(stderr, "%s\n", log);
			exit(1);
		}
	}
   
	GLuint program = glCreateProgram();
	glAttachShader(program, shader);
	glLinkProgram(program);
	glUseProgram(program);

	// Create VIC display texture (8-bit color index in the red channel)
	uint8 *tmp = (uint8 *)malloc(TEXTURE_SIZE * TEXTURE_SIZE);
	memset(tmp, 0, TEXTURE_SIZE * TEXTURE_SIZE);

	glGenTextures(1, &vic_tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, vic_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  // don't interpolate color index values
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, tmp);
	glUniform1i(glGetUniformLocation(program, "screen"), 0);

	free(tmp);

	// Create VIC palette texture
	tmp = (uint8 *)malloc(256 * 3);
	memset(tmp, 0xff, 256 * 3);
	for (int i=0; i<16; ++i) {
		tmp[i*3+0] = palette_red[i & 0x0f];
		tmp[i*3+1] = palette_green[i & 0x0f];
		tmp[i*3+2] = palette_blue[i & 0x0f];
	}
	tmp[fill_gray*3+0] = tmp[fill_gray*3+1] = tmp[fill_gray*3+2] = 0xd0;
	tmp[shine_gray*3+0] = tmp[shine_gray*3+1] = tmp[shine_gray*3+2] = 0xf0;
	tmp[shadow_gray*3+0] = tmp[shadow_gray*3+1] = tmp[shadow_gray*3+2] = 0x80;
	tmp[red*3+0] = 0xf0;
	tmp[red*3+1] = tmp[red*3+2] = 0;
	tmp[green*3+1] = 0xf0;
	tmp[green*3+0] = tmp[green*3+2] = 0;

	glGenTextures(1, &palette_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, palette_tex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  // don't interpolate palette entries
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, tmp);
	glUniform1i(glGetUniformLocation(program, "palette"), 1);

	free(tmp);

#else

	flags |= (SDL_HWSURFACE | SDL_DOUBLEBUF);
	screen = SDL_SetVideoMode(FRAME_WIDTH, FRAME_HEIGHT, 8, flags);

#endif

	// Hide mouse pointer in fullscreen mode
	if (ThePrefs.DisplayType == DISPTYPE_SCREEN)
		SDL_ShowCursor(0);

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
	pulse_tv.it_interval.tv_sec = 0;
	pulse_tv.it_interval.tv_usec = 0;
	pulse_tv.it_value.tv_sec = 0;
	pulse_tv.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &pulse_tv, NULL);

	SDL_Quit();

	c64_disp = NULL;
}


/*
 *  Prefs may have changed
 */

void C64Display::NewPrefs(Prefs *prefs)
{
	// Unused, we handle fullscreen/window mode switches in PollKeyboard()
}


/*
 *  Redraw bitmap
 */

void C64Display::Update(void)
{
	// Draw speedometer/LEDs
	SDL_Rect r = {0, DISPLAY_Y, DISPLAY_X, 15};
	SDL_FillRect(screen, &r, fill_gray);
	r.w = DISPLAY_X; r.h = 1;
	SDL_FillRect(screen, &r, shine_gray);
	r.y = DISPLAY_Y + 14;
	SDL_FillRect(screen, &r, shadow_gray);
	r.w = 16;
	for (int i=2; i<6; i++) {
		r.x = DISPLAY_X * i/5 - 24; r.y = DISPLAY_Y + 4;
		SDL_FillRect(screen, &r, shadow_gray);
		r.y = DISPLAY_Y + 10;
		SDL_FillRect(screen, &r, shine_gray);
	}
	r.y = DISPLAY_Y; r.w = 1; r.h = 15;
	for (int i=0; i<5; i++) {
		r.x = DISPLAY_X * i / 5;
		SDL_FillRect(screen, &r, shine_gray);
		r.x = DISPLAY_X * (i+1) / 5 - 1;
		SDL_FillRect(screen, &r, shadow_gray);
	}
	r.y = DISPLAY_Y + 4; r.h = 7;
	for (int i=2; i<6; i++) {
		r.x = DISPLAY_X * i/5 - 24;
		SDL_FillRect(screen, &r, shadow_gray);
		r.x = DISPLAY_X * i/5 - 9;
		SDL_FillRect(screen, &r, shine_gray);
	}
	r.y = DISPLAY_Y + 5; r.w = 14; r.h = 5;
	for (int i=0; i<4; i++) {
		r.x = DISPLAY_X * (i+2) / 5 - 23;
		int c;
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
		SDL_FillRect(screen, &r, c);
	}

	draw_string(screen, DISPLAY_X * 1/5 + 8, DISPLAY_Y + 4, "D\x12 8", black, fill_gray);
	draw_string(screen, DISPLAY_X * 2/5 + 8, DISPLAY_Y + 4, "D\x12 9", black, fill_gray);
	draw_string(screen, DISPLAY_X * 3/5 + 8, DISPLAY_Y + 4, "D\x12 10", black, fill_gray);
	draw_string(screen, DISPLAY_X * 4/5 + 8, DISPLAY_Y + 4, "D\x12 11", black, fill_gray);
	draw_string(screen, 24, DISPLAY_Y + 4, speedometer_string, black, fill_gray);

#ifdef ENABLE_OPENGL
	// Load screen to texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, vic_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FRAME_WIDTH, FRAME_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, screen->pixels);

	// Draw textured rectangle
	glBegin(GL_QUADS);
		glTexCoord2f(0.0, 0.0);
		glVertex2f(0.0, 0.0);
		glTexCoord2f(float(FRAME_WIDTH) / TEXTURE_SIZE, 0.0);
		glVertex2f(float(FRAME_WIDTH), 0.0);
		glTexCoord2f(float(FRAME_WIDTH) / TEXTURE_SIZE, float(FRAME_HEIGHT) / TEXTURE_SIZE);
		glVertex2f(float(FRAME_WIDTH), float(FRAME_HEIGHT));
		glTexCoord2f(0.0, float(FRAME_HEIGHT) / TEXTURE_SIZE);
		glVertex2f(0.0, float(FRAME_HEIGHT));
	glEnd();

	// Update display
	SDL_GL_SwapBuffers();
#else
	// Update display
	SDL_Flip(screen);
#endif
}


/*
 *  Draw string into surface using the C64 ROM font
 */

void C64Display::draw_string(SDL_Surface *s, int x, int y, const char *str, uint8 front_color, uint8 back_color)
{
	uint8 *pb = (uint8 *)s->pixels + s->pitch*y + x;
	char c;
	while ((c = *str++) != 0) {
		uint8 *q = TheC64->Char + c*8 + 0x800;
		uint8 *p = pb;
		for (int y=0; y<8; y++) {
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
		switch (c64_disp->led_state[i]) {
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

	if (delay >= 20) {
		delay = 0;
		sprintf(speedometer_string, "%d%%", speed);
	} else
		delay++;
}


/*
 *  Return pointer to bitmap data
 */

uint8 *C64Display::BitmapBase(void)
{
	return (uint8 *)screen->pixels;
}


/*
 *  Return number of bytes per row
 */

int C64Display::BitmapXMod(void)
{
	return screen->pitch;
}


/*
 *  Poll the keyboard
 */

static void translate_key(SDLKey key, bool key_up, uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
	int c64_key = -1;
	switch (key) {
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

		case SDLK_LCTRL: case SDLK_TAB: c64_key = MATRIX(7,2); break;
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
		default: break;
	}

	if (c64_key < 0)
		return;

	// Handle joystick emulation
	if (c64_key & 0x40) {
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

void C64Display::PollKeyboard(uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {

			// Key pressed
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {

					case SDLK_F9:	// F9: Invoke SAM
						if (ThePrefs.DisplayType == DISPTYPE_WINDOW)  // don't invoke in fullscreen mode
							SAM(TheC64);
						break;

					case SDLK_F10:	// F10: Prefs/Quit
						TheC64->Pause();
						if (ThePrefs.DisplayType == DISPTYPE_SCREEN) {  // exit fullscreen mode
							SDL_WM_ToggleFullScreen(SDL_GetVideoSurface());
							SDL_ShowCursor(1);
						}

						if (!TheApp->RunPrefsEditor()) {
							quit_requested = true;
						} else {
							if (ThePrefs.DisplayType == DISPTYPE_SCREEN) {  // enter fullscreen mode
								SDL_ShowCursor(0);
								SDL_WM_ToggleFullScreen(SDL_GetVideoSurface());
							}
						}

						TheC64->Resume();
						break;

					case SDLK_F11:	// F11: NMI (Restore)
						TheC64->NMI();
						break;

					case SDLK_F12:	// F12: Reset
						TheC64->Reset();
						break;

					case SDLK_NUMLOCK:
						num_locked = true;
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
				break;

			// Key released
			case SDL_KEYUP:
				if (event.key.keysym.sym == SDLK_NUMLOCK)
					num_locked = false;
				else
					translate_key(event.key.keysym.sym, true, key_matrix, rev_matrix, joystick);
				break;

#ifdef ENABLE_OPENGL
			// Window resized
			case SDL_VIDEORESIZE:
				video_resized(event.resize.w, event.resize.h);
				break;
#endif

			// Quit Frodo
			case SDL_QUIT:
				quit_requested = true;
				break;
		}
	}
}


/*
 *  Check if NumLock is down (for switching the joystick keyboard emulation)
 */

bool C64Display::NumLock(void)
{
	return num_locked;
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
	for (int i=0; i<16; i++) {
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
