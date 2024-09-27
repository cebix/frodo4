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


// Base class for cartridge with ROM
ROMCartridge::ROMCartridge(unsigned rom_size)
{
	// Allocate ROM
	rom = new uint8_t[rom_size];
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
