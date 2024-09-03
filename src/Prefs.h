/*
 *  Prefs.h - Global preferences
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

#ifndef _PREFS_H
#define _PREFS_H

#include <string>


// SID types
enum {
	SIDTYPE_NONE,		// SID emulation off
	SIDTYPE_DIGITAL,	// Digital SID emulation
	SIDTYPE_SIDCARD		// SID card
};


// REU sizes
enum {
	REU_NONE,		// No REU
	REU_128K,		// 128K
	REU_256K,		// 256K
	REU_512K		// 512K
};


// Display types
enum {
	DISPTYPE_WINDOW,	// Window
	DISPTYPE_SCREEN		// Fullscreen
};


// Preferences data
class Prefs {
public:
	Prefs();
#ifdef HAVE_GLADE
	bool ShowEditor(bool startup, std::string prefs_path, std::string snapshot_path);
#else
	bool ShowEditor(bool startup, char *prefs_name);
#endif
	void Check();
	void Load(const char *filename);
	bool Save(const char *filename);

	bool operator==(const Prefs &rhs) const;
	bool operator!=(const Prefs &rhs) const;

	int NormalCycles;		// Available CPU cycles in normal raster lines
	int BadLineCycles;		// Available CPU cycles in Bad Lines
	int CIACycles;			// CIA timer ticks per raster line
	int FloppyCycles;		// Available 1541 CPU cycles per line
	int SkipFrames;			// Draw every n-th frame

	char DrivePath[4][256];	// Path for drive 8..11

	int SIDType;			// SID emulation type
	int REUSize;			// Size of REU
	int DisplayType;		// Display type (windowed or full-screen)
	int Joystick1Port;		// Port that joystick 1 is connected to (0 = no joystick, all other values are system dependant)
	int Joystick2Port;		// Port that joystick 2 is connected to
	int ScalingNumerator;	// Window scaling numerator
	int ScalingDenominator;	// Window scaling denominator

	bool SpritesOn;			// Sprite display is on
	bool SpriteCollisions;	// Sprite collision detection is on
	bool JoystickSwap;		// Swap joysticks 1<->2
	bool LimitSpeed;		// Limit speed to 100%
	bool FastReset;			// Skip RAM test on reset
	bool CIAIRQHack;		// Write to CIA ICR clears IRQ
	bool MapSlash;			// Map '/' in C64 filenames
	bool Emul1541Proc;		// Enable processor-level 1541 emulation
	bool SIDFilters;		// Emulate SID filters
	bool DoubleScan;		// Double scan lines (BeOS, if DisplayType == DISPTYPE_SCREEN)
	bool JoystickGeekPort;	// Enable GeekPort joystick adapter (BeOS)
	bool HideCursor;		// Hide mouse cursor when visible (Win32)
	bool AutoPause;			// Auto pause when not foreground app (Win32)
	bool PrefsAtStartup;	// Show prefs dialog at startup (Win32)
	bool ShowLEDs;			// Show status bar
};


// These are the active preferences
extern Prefs ThePrefs;

// Theses are the preferences on disk
extern Prefs ThePrefsOnDisk;

#endif
