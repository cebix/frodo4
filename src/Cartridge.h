/*
 *  Cartridge.h - Cartridge emulation
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

#ifndef _CARTRIDGE_H
#define _CARTRIDGE_H

#include "sysdeps.h"


// Base class for cartridges
class Cartridge {
public:
	Cartridge() { }
	virtual ~Cartridge() { }

	virtual void Reset() { }

	// Default for $8000..$9fff is to read RAM
	virtual uint8_t ReadROML(uint16_t adr, uint8_t ram_byte, bool notLoram)
	{
		return ram_byte;
	}

	// Default for $a000..$bfff is to read RAM or BASIC ROM, depending on /LORAM
	virtual uint8_t ReadROMH(uint16_t adr, uint8_t ram_byte, uint8_t basic_byte, bool notLoram, bool notHiram)
	{
		return notLoram ? basic_byte : ram_byte;
	}

	// Default for I/O 1 and 2 is open bus
	virtual uint8_t ReadIO1(uint16_t adr, uint8_t bus_byte) { return bus_byte; }
	virtual void WriteIO1(uint16_t adr, uint8_t byte) { }
	virtual uint8_t ReadIO2(uint16_t adr, uint8_t bus_byte) { return bus_byte; }
	virtual void WriteIO2(uint16_t adr, uint8_t byte) { }

	virtual void FF00Trigger() { }

	// Memory mapping control lines
	bool notEXROM = true;
	bool notGAME = true;
};


// No cartridge
class NoCartridge : public Cartridge {
public:
	NoCartridge() { }
	~NoCartridge() { }
};


// Base class for cartridge with ROM
class ROMCartridge : public Cartridge {
public:
	ROMCartridge(unsigned rom_size);
	~ROMCartridge();

protected:
	uint8_t * rom = nullptr;	// Pointer to ROM contents
};


// 8K ROM cartridge (EXROM = 0, GAME = 1)
class Cartridge8K : public ROMCartridge {
public:
	Cartridge8K();

	uint8_t ReadROML(uint16_t adr, uint8_t ram_byte, bool notLoram) override;
};


// 16K ROM cartridge (EXROM = 0, GAME = 0)
class Cartridge16K : public ROMCartridge {
public:
	Cartridge16K();

	uint8_t ReadROML(uint16_t adr, uint8_t ram_byte, bool notLoram) override;
	uint8_t ReadROMH(uint16_t adr, uint8_t ram_byte, uint8_t basic_byte, bool notLoram, bool notHiram) override;
};


#endif // ndef _CARTRIDGE_H
