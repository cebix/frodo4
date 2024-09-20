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
#include "C64.h"

#include <fstream>
#include <regex>
namespace fs = std::filesystem;


// These are the active preferences
Prefs ThePrefs;


/*
 *  Constructor: Set up preferences with defaults
 */

Prefs::Prefs()
{
	NormalCycles = CYCLES_PER_LINE;
	BadLineCycles = CYCLES_PER_LINE - 40;
	CIACycles = CYCLES_PER_LINE;
	FloppyCycles = 64;
	ScalingNumerator = 4;
	ScalingDenominator = 1;

	SIDType = SIDTYPE_DIGITAL_6581;
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
	ShowLEDs = true;
}


/*
 *  Check preferences for validity and correct if necessary
 */

void Prefs::Check()
{
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

void Prefs::Load(fs::path prefs_path)
{
	std::ifstream file(prefs_path);
	if (! file) {
		return;
	}

	std::string line;
	while (std::getline(file, line)) {
		static const std::regex prefsLine(R"((\S*)\s*=\s*([\S ]*))");

		std::smatch m;
		if (std::regex_match(line, m, prefsLine)) {
			const std::string keyword = m[1].str();
			const std::string value = m[2].str();

			if (keyword == "NormalCycles") {
				NormalCycles = atoi(value.c_str());
			} else if (keyword == "BadLineCycles") {
				BadLineCycles = atoi(value.c_str());
			} else if (keyword == "CIACycles") {
				CIACycles = atoi(value.c_str());
			} else if (keyword == "FloppyCycles") {
				FloppyCycles = atoi(value.c_str());

			} else if (keyword == "DrivePath8") {
				DrivePath[0] = value;
			} else if (keyword == "DrivePath9") {
				DrivePath[1] = value;
			} else if (keyword == "DrivePath10") {
				DrivePath[2] = value;
			} else if (keyword == "DrivePath11") {
				DrivePath[3] = value;

			} else if (keyword == "SIDType") {
				if (value == "DIGITAL") {
					SIDType = SIDTYPE_DIGITAL_6581;
				} else if (value == "6581") {
					SIDType = SIDTYPE_DIGITAL_6581;
				} else if (value == "8580") {
					SIDType = SIDTYPE_DIGITAL_8580;
				} else if (value == "SIDCARD") {
					SIDType = SIDTYPE_SIDCARD;
				} else {
					SIDType = SIDTYPE_NONE;
				}
			} else if (keyword == "REUSize") {
				if (value == "128K") {
					REUSize = REU_128K;
				} else if (value == "256K") {
					REUSize = REU_256K;
				} else if (value == "512K") {
					REUSize = REU_512K;
				} else {
					REUSize = REU_NONE;
				}
			} else if (keyword == "DisplayType") {
				DisplayType = (value == "SCREEN") ? DISPTYPE_SCREEN : DISPTYPE_WINDOW;
			} else if (keyword == "Palette") {
				Palette = (value == "COLODORE") ? PALETTE_COLODORE : PALETTE_PEPTO;
			} else if (keyword == "Joystick1Port") {
				Joystick1Port = atoi(value.c_str());
			} else if (keyword == "Joystick2Port") {
				Joystick2Port = atoi(value.c_str());
			} else if (keyword == "ScalingNumerator") {
				ScalingNumerator = atoi(value.c_str());
			} else if (keyword == "ScalingDenominator") {
				ScalingDenominator = atoi(value.c_str());

			} else if (keyword == "SpritesOn") {
				SpritesOn = (value == "true");
			} else if (keyword == "SpriteCollisions") {
				SpriteCollisions = (value == "true");
			} else if (keyword == "JoystickSwap") {
				JoystickSwap = (value == "true");
			} else if (keyword == "LimitSpeed") {
				LimitSpeed = (value == "true");
			} else if (keyword == "FastReset") {
				FastReset = (value == "true");
			} else if (keyword == "CIAIRQHack") {
				CIAIRQHack = (value == "true");
			} else if (keyword == "MapSlash") {
				MapSlash = (value == "true");
			} else if (keyword == "Emul1541Proc") {
				Emul1541Proc = (value == "true");
			} else if (keyword == "SIDFilters") {
				SIDFilters = (value == "true");
			} else if (keyword == "ShowLEDs") {
				ShowLEDs = (value == "true");
			}
		}
	}

	Check();
}


/*
 *  Save preferences to file
 *  true: success, false: error
 */

bool Prefs::Save(fs::path prefs_path)
{
	Check();

	std::ofstream file(prefs_path, std::ios::trunc);
	if (! file) {
		return false;
	}

	file << std::boolalpha;

	file << "NormalCycles = " << NormalCycles << std::endl;
	file << "BadLineCycles = " << BadLineCycles << std::endl;
	file << "CIACycles = " << CIACycles << std::endl;
	file << "FloppyCycles = " <<  FloppyCycles << std::endl;

	for (unsigned i = 0; i < 4; ++i) {
		file << "DrivePath" << (i+8) << " = " << DrivePath[i] << std::endl;
	}

	file << "SIDType = ";
	switch (SIDType) {
		case SIDTYPE_NONE:         file << "NONE\n";    break;
		case SIDTYPE_DIGITAL_6581: file << "6581\n"; break;
		case SIDTYPE_DIGITAL_8580: file << "8580\n"; break;
		case SIDTYPE_SIDCARD:      file << "SIDCARD\n"; break;
	}
	file << "REUSize = ";
	switch (REUSize) {
		case REU_NONE: file << "NONE\n"; break;
		case REU_128K: file << "128K\n"; break;
		case REU_256K: file << "256K\n"; break;
		case REU_512K: file << "512K\n"; break;
	};
	file << "DisplayType = " << (DisplayType == DISPTYPE_WINDOW ? "WINDOW\n" : "SCREEN\n");
	file << "Palette = " << (Palette == PALETTE_COLODORE ? "COLODORE\n" : "PEPTO\n");
	file << "Joystick1Port = " << Joystick1Port << std::endl;
	file << "Joystick2Port = " << Joystick2Port << std::endl;
	file << "ScalingNumerator = " << ScalingNumerator << std::endl;
	file << "ScalingDenominator = " << ScalingDenominator << std::endl;

	file << "SpritesOn = " << SpritesOn << std::endl;
	file << "SpriteCollisions = " << SpriteCollisions << std::endl;
	file << "JoystickSwap = " << JoystickSwap << std::endl;
	file << "LimitSpeed = " << LimitSpeed << std::endl;
	file << "FastReset = " << FastReset << std::endl;
	file << "CIAIRQHack = " << CIAIRQHack << std::endl;
	file << "MapSlash = " << MapSlash << std::endl;
	file << "Emul1541Proc = " << Emul1541Proc << std::endl;
	file << "SIDFilters = " << SIDFilters << std::endl;
	file << "ShowLEDs = " << ShowLEDs << std::endl;

	return true;
}


#if defined(HAVE_GLADE)
#include "Prefs_glade.h"

#else
#include "Prefs_none.h"
#endif
