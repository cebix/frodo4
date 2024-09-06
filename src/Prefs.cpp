/*
 *  Prefs.cpp - Global preferences
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

#include "Prefs.h"
#include "Display.h"
#include "C64.h"
#include "main.h"


// These are the active preferences
Prefs ThePrefs;


/*
 *  Constructor: Set up preferences with defaults
 */

Prefs::Prefs()
{
	NormalCycles = 63;
	BadLineCycles = 23;
	CIACycles = 63;
	FloppyCycles = 64;
	SkipFrames = 1;
	ScalingNumerator = 4;
	ScalingDenominator = 1;

	strcpy(DrivePath[0], "");
	strcpy(DrivePath[1], "");
	strcpy(DrivePath[2], "");
	strcpy(DrivePath[3], "");

	SIDType = SIDTYPE_DIGITAL;
	REUSize = REU_NONE;
	DisplayType = DISPTYPE_WINDOW;
	Palette = PALETTE_PEPTO;
	Joystick1Port = 0;
	Joystick2Port = 0;

	SpritesOn = true;
	SpriteCollisions = true;
	JoystickSwap = false;
	LimitSpeed = true;
	FastReset = false;
	CIAIRQHack = false;
	MapSlash = true;
	Emul1541Proc = true;
	SIDFilters = true;
	DoubleScan = true;
	HideCursor = false;
	AutoPause = false;
	PrefsAtStartup = true;
	ShowLEDs = true;
}


/*
 *  Check if two Prefs structures are equal
 */

bool Prefs::operator==(const Prefs &rhs) const
{
	return (1
		&& NormalCycles == rhs.NormalCycles
		&& BadLineCycles == rhs.BadLineCycles
		&& CIACycles == rhs.CIACycles
		&& FloppyCycles == rhs.FloppyCycles
		&& SkipFrames == rhs.SkipFrames
		&& ScalingNumerator == rhs.ScalingNumerator
		&& ScalingDenominator == rhs.ScalingNumerator
		&& strcmp(DrivePath[0], rhs.DrivePath[0]) == 0
		&& strcmp(DrivePath[1], rhs.DrivePath[1]) == 0
		&& strcmp(DrivePath[2], rhs.DrivePath[2]) == 0
		&& strcmp(DrivePath[3], rhs.DrivePath[3]) == 0
		&& SIDType == rhs.SIDType
		&& REUSize == rhs.REUSize
		&& DisplayType == rhs.DisplayType
		&& Palette == rhs.Palette
		&& SpritesOn == rhs.SpritesOn
		&& SpriteCollisions == rhs.SpriteCollisions
		&& Joystick1Port == rhs.Joystick1Port
		&& Joystick2Port == rhs.Joystick2Port
		&& JoystickSwap == rhs.JoystickSwap
		&& LimitSpeed == rhs.LimitSpeed
		&& FastReset == rhs.FastReset
		&& CIAIRQHack == rhs.CIAIRQHack
		&& MapSlash == rhs.MapSlash
		&& Emul1541Proc == rhs.Emul1541Proc
		&& SIDFilters == rhs.SIDFilters
		&& DoubleScan == rhs.DoubleScan
		&& HideCursor == rhs.HideCursor
		&& AutoPause == rhs.AutoPause
		&& PrefsAtStartup == rhs.PrefsAtStartup
		&& ShowLEDs == rhs.ShowLEDs
	);
}

bool Prefs::operator!=(const Prefs &rhs) const
{
	return !operator==(rhs);
}


/*
 *  Check preferences for validity and correct if necessary
 */

void Prefs::Check()
{
	if (SkipFrames <= 0) {
		SkipFrames = 1;
	}

	if (ScalingNumerator <= 0) {
		ScalingNumerator = 1;
	}

	if (ScalingDenominator <= 0) {
		ScalingDenominator = 1;
	}

	if (SIDType < SIDTYPE_NONE || SIDType > SIDTYPE_SIDCARD) {
		SIDType = SIDTYPE_NONE;
	}

	if (REUSize < REU_NONE || REUSize > REU_512K) {
		REUSize = REU_NONE;
	}

	if (DisplayType < DISPTYPE_WINDOW || DisplayType > DISPTYPE_SCREEN) {
		DisplayType = DISPTYPE_WINDOW;
	}

	if (Palette < PALETTE_PEPTO || Palette > PALETTE_COLODORE) {
		Palette = PALETTE_PEPTO;
	}
}


/*
 *  Load preferences from file
 */

void Prefs::Load(const char *filename)
{
	FILE *file;
	char line[256], keyword[256], value[256];

	if ((file = fopen(filename, "r")) != NULL) {
		while(fgets(line, 255, file)) {
			if (sscanf(line, "%s = %[^\n]\n", keyword, value) == 2) {
				if (!strcmp(keyword, "NormalCycles")) {
					NormalCycles = atoi(value);
				} else if (!strcmp(keyword, "BadLineCycles")) {
					BadLineCycles = atoi(value);
				} else if (!strcmp(keyword, "CIACycles")) {
					CIACycles = atoi(value);
				} else if (!strcmp(keyword, "FloppyCycles")) {
					FloppyCycles = atoi(value);
				} else if (!strcmp(keyword, "SkipFrames")) {
					SkipFrames = atoi(value);
				} else if (!strcmp(keyword, "ScalingNumerator")) {
					ScalingNumerator = atoi(value);
				} else if (!strcmp(keyword, "ScalingDenominator")) {
					ScalingDenominator = atoi(value);
				} else if (!strcmp(keyword, "DrivePath8")) {
					strcpy(DrivePath[0], value);
				} else if (!strcmp(keyword, "DrivePath9")) {
					strcpy(DrivePath[1], value);
				} else if (!strcmp(keyword, "DrivePath10")) {
					strcpy(DrivePath[2], value);
				} else if (!strcmp(keyword, "DrivePath11")) {
					strcpy(DrivePath[3], value);
				} else if (!strcmp(keyword, "SIDType")) {
					if (!strcmp(value, "DIGITAL")) {
						SIDType = SIDTYPE_DIGITAL;
					} else if (!strcmp(value, "SIDCARD")) {
						SIDType = SIDTYPE_SIDCARD;
					} else {
						SIDType = SIDTYPE_NONE;
					}
				} else if (!strcmp(keyword, "REUSize")) {
					if (!strcmp(value, "128K")) {
						REUSize = REU_128K;
					} else if (!strcmp(value, "256K")) {
						REUSize = REU_256K;
					} else if (!strcmp(value, "512K")) {
						REUSize = REU_512K;
					} else {
						REUSize = REU_NONE;
					}
				} else if (!strcmp(keyword, "DisplayType")) {
					DisplayType = strcmp(value, "SCREEN") ? DISPTYPE_WINDOW : DISPTYPE_SCREEN;
				} else if (!strcmp(keyword, "Palette")) {
					Palette = strcmp(value, "COLODORE") ? PALETTE_PEPTO : PALETTE_COLODORE;
				} else if (!strcmp(keyword, "Joystick1Port")) {
					Joystick1Port = atoi(value);
				} else if (!strcmp(keyword, "Joystick2Port")) {
					Joystick2Port = atoi(value);
				} else if (!strcmp(keyword, "SpritesOn")) {
					SpritesOn = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "SpriteCollisions")) {
					SpriteCollisions = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "JoystickSwap")) {
					JoystickSwap = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "LimitSpeed")) {
					LimitSpeed = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "FastReset")) {
					FastReset = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "CIAIRQHack")) {
					CIAIRQHack = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "MapSlash")) {
					MapSlash = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "Emul1541Proc")) {
					Emul1541Proc = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "SIDFilters")) {
					SIDFilters = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "DoubleScan")) {
					DoubleScan = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "HideCursor")) {
					HideCursor = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "AutoPause")) {
					AutoPause = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "PrefsAtStartup")) {
					PrefsAtStartup = !strcmp(value, "TRUE");
				} else if (!strcmp(keyword, "ShowLEDs")) {
					ShowLEDs = !strcmp(value, "TRUE");
				}
			}
		}
		fclose(file);
	}
	Check();
}


/*
 *  Save preferences to file
 *  true: success, false: error
 */

bool Prefs::Save(const char *filename)
{
	FILE *file;

	Check();
	if ((file = fopen(filename, "w")) != NULL) {
		fprintf(file, "NormalCycles = %d\n", NormalCycles);
		fprintf(file, "BadLineCycles = %d\n", BadLineCycles);
		fprintf(file, "CIACycles = %d\n", CIACycles);
		fprintf(file, "FloppyCycles = %d\n", FloppyCycles);
		fprintf(file, "SkipFrames = %d\n", SkipFrames);
		fprintf(file, "ScalingNumerator = %d\n", ScalingNumerator);
		fprintf(file, "ScalingDenominator = %d\n", ScalingDenominator);
		for (int i=0; i<4; i++) {
			fprintf(file, "DrivePath%d = %s\n", i+8, DrivePath[i]);
		}
		fprintf(file, "SIDType = ");
		switch (SIDType) {
			case SIDTYPE_NONE:
				fprintf(file, "NONE\n");
				break;
			case SIDTYPE_DIGITAL:
				fprintf(file, "DIGITAL\n");
				break;
			case SIDTYPE_SIDCARD:
				fprintf(file, "SIDCARD\n");
				break;
		}
		fprintf(file, "REUSize = ");
		switch (REUSize) {
			case REU_NONE:
				fprintf(file, "NONE\n");
				break;
			case REU_128K:
				fprintf(file, "128K\n");
				break;
			case REU_256K:
				fprintf(file, "256K\n");
				break;
			case REU_512K:
				fprintf(file, "512K\n");
				break;
		};
		fprintf(file, "DisplayType = %s\n", DisplayType == DISPTYPE_WINDOW ? "WINDOW" : "SCREEN");
		fprintf(file, "Palette = %s\n", Palette == PALETTE_COLODORE ? "COLODORE" : "PEPTO");
		fprintf(file, "Joystick1Port = %d\n", Joystick1Port);
		fprintf(file, "Joystick2Port = %d\n", Joystick2Port);
		fprintf(file, "SpritesOn = %s\n", SpritesOn ? "TRUE" : "FALSE");
		fprintf(file, "SpriteCollisions = %s\n", SpriteCollisions ? "TRUE" : "FALSE");
		fprintf(file, "JoystickSwap = %s\n", JoystickSwap ? "TRUE" : "FALSE");
		fprintf(file, "LimitSpeed = %s\n", LimitSpeed ? "TRUE" : "FALSE");
		fprintf(file, "FastReset = %s\n", FastReset ? "TRUE" : "FALSE");
		fprintf(file, "CIAIRQHack = %s\n", CIAIRQHack ? "TRUE" : "FALSE");
		fprintf(file, "MapSlash = %s\n", MapSlash ? "TRUE" : "FALSE");
		fprintf(file, "Emul1541Proc = %s\n", Emul1541Proc ? "TRUE" : "FALSE");
		fprintf(file, "SIDFilters = %s\n", SIDFilters ? "TRUE" : "FALSE");
		fprintf(file, "DoubleScan = %s\n", DoubleScan ? "TRUE" : "FALSE");
		fprintf(file, "HideCursor = %s\n", HideCursor ? "TRUE" : "FALSE");
		fprintf(file, "AutoPause = %s\n", AutoPause ? "TRUE" : "FALSE");
		fprintf(file, "PrefsAtStartup = %s\n", PrefsAtStartup ? "TRUE" : "FALSE");
		fprintf(file, "ShowLEDs = %s\n", ShowLEDs ? "TRUE" : "FALSE");
		fclose(file);
		return true;
	}
	return false;
}


#if defined(__BEOS__)
#include "Prefs_Be.h"

#elif defined(HAVE_GLADE)
#include "Prefs_glade.h"

#else
#include "Prefs_none.h"
#endif
