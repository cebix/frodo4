/*
 *  Cartridge.cpp - Cartridge emulation
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

#include "Cartridge.h"

#include <filesystem>
namespace fs = std::filesystem;


// Base class for cartridge with ROM
ROMCartridge::ROMCartridge(unsigned rom_size)
{
	// Allocate ROM
	rom = new uint8_t[rom_size];
	memset(rom, 0xff, rom_size);
}

ROMCartridge::~ROMCartridge()
{
	// Free ROM
	delete[] rom;
}


// 8K ROM cartridge (EXROM = 0, GAME = 1)
Cartridge8K::Cartridge8K() : ROMCartridge(0x2000)
{
	notEXROM = false;
}

uint8_t Cartridge8K::ReadROML(uint16_t adr, uint8_t ram_byte, bool notLoram)
{
	return notLoram ? rom[adr] : ram_byte;
}


// 16K ROM cartridge (EXROM = 0, GAME = 0)
Cartridge16K::Cartridge16K() : ROMCartridge(0x4000)
{
	notEXROM = false;
	notGAME = false;
}

uint8_t Cartridge16K::ReadROML(uint16_t adr, uint8_t ram_byte, bool notLoram)
{
	return notLoram ? rom[adr] : ram_byte;
}

uint8_t Cartridge16K::ReadROMH(uint16_t adr, uint8_t ram_byte, uint8_t basic_byte, bool notLoram, bool notHiram)
{
	return notHiram ? rom[adr + 0x2000] : ram_byte;
}


/*
 *  Check whether file is a cartridge image file
 */

bool IsCartridgeFile(const std::string & path)
{
	// Reject directories
	if (fs::is_directory(path))
		return false;

	// Read file header
	FILE * f = fopen(path.c_str(), "rb");
	if (f == nullptr)
		return false;

	uint8_t header[64];
	if (fread(header, sizeof(header), 1, f) != 1) {
		fclose(f);
		return false;
	}

	fclose(f);

	// Check for signature and version
	uint16_t version = (header[0x14] << 8) | header[0x15];
	return memcmp(header, "C64 CARTRIDGE   ", 16) == 0 && version == 0x0100;
}


/*
 *  Construct cartridge object from image file path, return nullptr on error
 */

Cartridge * Cartridge::FromFile(const std::string & path, std::string & ret_error_msg)
{
	// Empty path = no cartridge
	if (path.empty())
		return nullptr;

	Cartridge * cart = nullptr;
	FILE * f = nullptr;
	{
		// Read file header
		f = fopen(path.c_str(), "rb");
		if (f == nullptr) {
			ret_error_msg = "Can't open cartridge file";
			return nullptr;
		}

		uint8_t header[64];
		if (fread(header, sizeof(header), 1, f) != 1)
			goto error_read;

		// Check for signature and version
		uint16_t version = (header[0x14] << 8) | header[0x15];
		if (memcmp(header, "C64 CARTRIDGE   ", 16) != 0 || version != 0x0100)
			goto error_unsupp;

		// Check cartridge type
		uint16_t type = (header[0x16] << 8) | header[0x17];
		if (type != 0)	// Normal cartridge
			goto error_unsupp;

		// Check EXROM/GAME lines
		uint8_t exrom = header[0x18];
		uint8_t game = header[0x19];

		if (exrom != 0)					// Ultimax or not a ROM cartridge
			goto error_unsupp;

		uint16_t rom_size = (game == 0) ? 0x4000 : 0x2000;	// 16K or 8K

		// Load first CHIP header
		if (fread(header, 16, 1, f) != 1)
			goto error_read;

		// Check for signature and size
		uint16_t chip_type = (header[0x08] << 8) | header[0x09];
		uint16_t chip_size = (header[0x0e] << 8) | header[0x0f];
		if (memcmp(header, "CHIP", 4) != 0 || chip_type != 0 || chip_size > rom_size)
			goto error_unsupp;

		// Create cartridge object and load ROM
		if (game == 0) {
			cart = new Cartridge16K;
		} else {
			cart = new Cartridge8K;
		}

		if (fread(static_cast<ROMCartridge *>(cart)->ROM(), chip_size, 1, f) != 1)
			goto error_read;

		fclose(f);
	}
	return cart;

error_read:
	delete cart;
	fclose(f);

	ret_error_msg = "Error reading cartridge file";
	return nullptr;

error_unsupp:
	delete cart;
	fclose(f);

	ret_error_msg = "Unsupported cartridge type";
	return nullptr;
}
