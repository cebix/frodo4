/*
 *  C64.cpp - Put the pieces together
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

#include "C64.h"
#include "CPUC64.h"
#include "CPU1541.h"
#include "VIC.h"
#include "SID.h"
#include "CIA.h"
#include "REU.h"
#include "IEC.h"
#include "1541job.h"
#include "Display.h"
#include "Prefs.h"

#include <memory>


#ifdef FRODO_SC
bool IsFrodoSC = true;
#else
bool IsFrodoSC = false;
#endif


// Snapshot magic header
#define SNAPSHOT_HEADER "FrodoSnapshot4\0\0"

// Snapshot flags
#define SNAPSHOT_FLAG_1541_PROC 1

// Snapshot data structure
struct Snapshot {
	uint8_t magic[16];
	uint16_t flags;
	uint8_t delay;		// Frodo SC: Number of cycles the saved C64 CPU lags behind the other state
	uint8_t driveDelay;	// Frodo SC: Number of cycles the saved 1541 CPU lags behind the other state

	char drive8Path[256];

	uint8_t ram[C64_RAM_SIZE];
	uint8_t color[COLOR_RAM_SIZE];

	uint8_t driveRam[DRIVE_RAM_SIZE];

	MOS6510State cpu;
	MOS6569State vic;
	MOS6581State sid;
	MOS6526State cia1;
	MOS6526State cia2;

	MOS6502State driveCpu;
	Job1541State driveJob;

	// TODO: REU state is not saved
};


/*
 *  Constructor: Allocate objects and memory
 */

C64::C64()
{
	uint8_t *p;

	// The thread is not yet running
	thread_running = false;
	quit_thyself = false;
	have_a_break = false;

	// System-dependent things
	c64_ctor1();

	// Open display
	TheDisplay = new C64Display(this);

	// Allocate RAM/ROM memory
	RAM = new uint8_t[C64_RAM_SIZE];
	Basic = new uint8_t[BASIC_ROM_SIZE];
	Kernal = new uint8_t[KERNAL_ROM_SIZE];
	Char = new uint8_t[CHAR_ROM_SIZE];
	Color = new uint8_t[COLOR_RAM_SIZE];
	RAM1541 = new uint8_t[DRIVE_RAM_SIZE];
	ROM1541 = new uint8_t[DRIVE_ROM_SIZE];

	// Create the chips
	TheCPU = new MOS6510(this, RAM, Basic, Kernal, Char, Color);

	TheJob1541 = new Job1541(RAM1541);
	TheCPU1541 = new MOS6502_1541(this, TheJob1541, TheDisplay, RAM1541, ROM1541);

	TheVIC = TheCPU->TheVIC = new MOS6569(this, TheDisplay, TheCPU, RAM, Char, Color);
	TheSID = TheCPU->TheSID = new MOS6581(this);
	TheCIA1 = TheCPU->TheCIA1 = new MOS6526_1(TheCPU, TheVIC);
	TheCIA2 = TheCPU->TheCIA2 = TheCPU1541->TheCIA2 = new MOS6526_2(TheCPU, TheVIC, TheCPU1541);
	TheIEC = TheCPU->TheIEC = new IEC(TheDisplay);
	TheREU = TheCPU->TheREU = new REU(TheCPU);

	// Initialize RAM with powerup pattern
	p = RAM;
	for (unsigned i=0; i<512; i++) {
		for (unsigned j=0; j<64; j++)
			*p++ = 0;
		for (unsigned j=0; j<64; j++)
			*p++ = 0xff;
	}

	// Initialize color RAM with random values
	p = Color;
	for (unsigned i=0; i<COLOR_RAM_SIZE; i++)
		*p++ = rand() & 0x0f;

	// Clear 1541 RAM
	memset(RAM1541, 0, DRIVE_RAM_SIZE);

	// Open joystick drivers if required
	open_close_joysticks(0, 0, ThePrefs.Joystick1Port, ThePrefs.Joystick2Port);
	joykey = 0xff;

	CycleCounter = 0;

	// System-dependent things
	c64_ctor2();
}


/*
 *  Destructor: Delete all objects
 */

C64::~C64()
{
	open_close_joysticks(ThePrefs.Joystick1Port, ThePrefs.Joystick2Port, 0, 0);

	delete TheJob1541;
	delete TheREU;
	delete TheIEC;
	delete TheCIA2;
	delete TheCIA1;
	delete TheSID;
	delete TheVIC;
	delete TheCPU1541;
	delete TheCPU;
	delete TheDisplay;

	delete[] RAM;
	delete[] Basic;
	delete[] Kernal;
	delete[] Char;
	delete[] Color;
	delete[] RAM1541;
	delete[] ROM1541;

	c64_dtor();
}


/*
 *  Reset C64
 */

void C64::Reset(void)
{
	TheCPU->AsyncReset();
	TheCPU1541->AsyncReset();
	TheSID->Reset();
	TheCIA1->Reset();
	TheCIA2->Reset();
	TheIEC->Reset();
}


/*
 *  NMI C64
 */

void C64::NMI(void)
{
	TheCPU->AsyncNMI();
}


/*
 *  The preferences have changed. prefs is a pointer to the new
 *  preferences, ThePrefs still holds the previous ones.
 *  The emulation must be in the paused state!
 */

void C64::NewPrefs(const Prefs *prefs)
{
	open_close_joysticks(ThePrefs.Joystick1Port, ThePrefs.Joystick2Port, prefs->Joystick1Port, prefs->Joystick2Port);
	PatchKernal(prefs->FastReset, prefs->Emul1541Proc);

	TheDisplay->NewPrefs(prefs);

	TheIEC->NewPrefs(prefs);
	TheJob1541->NewPrefs(prefs);

	TheREU->NewPrefs(prefs);
	TheSID->NewPrefs(prefs);

	// Reset 1541 processor if turned on
	if (!ThePrefs.Emul1541Proc && prefs->Emul1541Proc)
		TheCPU1541->AsyncReset();
}


/*
 *  Patch kernal IEC routines
 */

void C64::PatchKernal(bool fast_reset, bool emul_1541_proc)
{
	if (fast_reset) {
		Kernal[0x1d84] = 0xa0;
		Kernal[0x1d85] = 0x00;
	} else {
		Kernal[0x1d84] = orig_kernal_1d84;
		Kernal[0x1d85] = orig_kernal_1d85;
	}

	if (emul_1541_proc) {
		Kernal[0x0d40] = 0x78;
		Kernal[0x0d41] = 0x20;
		Kernal[0x0d23] = 0x78;
		Kernal[0x0d24] = 0x20;
		Kernal[0x0d36] = 0x78;
		Kernal[0x0d37] = 0x20;
		Kernal[0x0e13] = 0x78;
		Kernal[0x0e14] = 0xa9;
		Kernal[0x0def] = 0x78;
		Kernal[0x0df0] = 0x20;
		Kernal[0x0dbe] = 0xad;
		Kernal[0x0dbf] = 0x00;
		Kernal[0x0dcc] = 0x78;
		Kernal[0x0dcd] = 0x20;
		Kernal[0x0e03] = 0x20;
		Kernal[0x0e04] = 0xbe;
	} else {
		Kernal[0x0d40] = 0xf2;	// IECOut
		Kernal[0x0d41] = 0x00;
		Kernal[0x0d23] = 0xf2;	// IECOutATN
		Kernal[0x0d24] = 0x01;
		Kernal[0x0d36] = 0xf2;	// IECOutSec
		Kernal[0x0d37] = 0x02;
		Kernal[0x0e13] = 0xf2;	// IECIn
		Kernal[0x0e14] = 0x03;
		Kernal[0x0def] = 0xf2;	// IECSetATN
		Kernal[0x0df0] = 0x04;
		Kernal[0x0dbe] = 0xf2;	// IECRelATN
		Kernal[0x0dbf] = 0x05;
		Kernal[0x0dcc] = 0xf2;	// IECTurnaround
		Kernal[0x0dcd] = 0x06;
		Kernal[0x0e03] = 0xf2;	// IECRelease
		Kernal[0x0e04] = 0x07;
	}

	// 1541
	ROM1541[0x2ae4] = 0xea;		// Don't check ROM checksum
	ROM1541[0x2ae5] = 0xea;
	ROM1541[0x2ae8] = 0xea;
	ROM1541[0x2ae9] = 0xea;
	ROM1541[0x2c9b] = 0xf2;		// DOS idle loop
	ROM1541[0x2c9c] = 0x00;
	ROM1541[0x3594] = 0x20;		// Write sector
	ROM1541[0x3595] = 0xf2;
	ROM1541[0x3596] = 0xf5;
	ROM1541[0x3597] = 0xf2;
	ROM1541[0x3598] = 0x01;
	ROM1541[0x3b0c] = 0xf2;		// Format track
	ROM1541[0x3b0d] = 0x02;
}


/*
 *  Save state to snapshot (emulation must be paused and in VBlank)
 *
 *  To be able to use SC snapshots with SL, the state of the SC C64 and 1541
 *  CPUs are not saved in the middle of an instruction. Instead the state is
 *  advanced cycle by cycle until the current instruction has finished. The
 *  number of cycles this takes is saved in the snapshot and will be
 *  reconstructed when the snapshot is loaded into FrodoSC again.
 */

void C64::MakeSnapshot(Snapshot * s)
{
	memset(s, 0, sizeof(*s));

	memcpy(s->magic, SNAPSHOT_HEADER, sizeof(s->magic));

	strcpy(s->drive8Path, ThePrefs.DrivePath[0]);

	TheVIC->GetState(&(s->vic));
	TheSID->GetState(&(s->sid));
	TheCIA1->GetState(&(s->cia1));
	TheCIA2->GetState(&(s->cia2));

#ifdef FRODO_SC
	while (true) {
		TheCPU->GetState(&(s->cpu));

		if (s->cpu.instruction_complete)
			break;

		// Advance C64 state by one cycle
		if (TheVIC->EmulateCycle()) {
			TheSID->EmulateLine();
		}
		TheCIA1->CheckIRQs();
		TheCIA2->CheckIRQs();
		TheCIA1->EmulateCycle();
		TheCIA2->EmulateCycle();
		TheCPU->EmulateCycle();

		s->delay++;
	}
#else
	TheCPU->GetState(&(s->cpu));
#endif

	memcpy(s->ram, RAM, C64_RAM_SIZE);
	memcpy(s->color, Color, COLOR_RAM_SIZE);

	if (ThePrefs.Emul1541Proc) {
		s->flags |= SNAPSHOT_FLAG_1541_PROC;

		TheJob1541->GetState(&(s->driveJob));

#ifdef FRODO_SC
		while (true) {
			TheCPU1541->GetState(&(s->driveCpu));

			if (s->driveCpu.idle || s->driveCpu.instruction_complete)
				break;

			// Advance 1541 state by one cycle
			TheCPU1541->CountVIATimers(1);
			if (!TheCPU1541->Idle) {
				TheCPU1541->EmulateCycle();
			}

			s->driveDelay++;
		}
#else
		TheCPU1541->GetState(&(s->driveCpu));
#endif
	}

	memcpy(s->driveRam, RAM1541, DRIVE_RAM_SIZE);
}


/*
 *  Restore state from snapshot (emulation must be paused and in VBlank)
 *
 *  Note: The magic header is not checked by this function.
 */

void C64::RestoreSnapshot(const Snapshot * s)
{
	// SL CPU64::SetState() overwrites ram[0/1], so we need to restore that
	// first in case we're loading an SC snapshot
	memcpy(RAM, s->ram, C64_RAM_SIZE);
	memcpy(Color, s->color, COLOR_RAM_SIZE);

	TheCPU->SetState(&(s->cpu));
	TheVIC->SetState(&(s->vic));
	TheSID->SetState(&(s->sid));
	TheCIA1->SetState(&(s->cia1));
	TheCIA2->SetState(&(s->cia2));

#ifdef FRODO_SC
	// Advance C64 to saved CPU state
	for (unsigned i = 0; i < s->delay; ++i) {
		if (TheVIC->EmulateCycle()) {
			TheSID->EmulateLine();
		}
		TheCIA1->CheckIRQs();
		TheCIA2->CheckIRQs();
		TheCIA1->EmulateCycle();
		TheCIA2->EmulateCycle();
	}
#endif

	if (s->flags & SNAPSHOT_FLAG_1541_PROC) {

		memcpy(RAM1541, s->driveRam, DRIVE_RAM_SIZE);

		// Switch on 1541 processor emulation if it is off
		if (! ThePrefs.Emul1541Proc) {
			auto prefs = std::make_unique<Prefs>(ThePrefs);
			strcpy(prefs->DrivePath[0], s->drive8Path);
			prefs->Emul1541Proc = true;
			NewPrefs(prefs.get());
			ThePrefs = *prefs;
		}

		TheCPU1541->SetState(&(s->driveCpu));
		TheJob1541->SetState(&(s->driveJob));

#ifdef FRODO_SC
		// Advance 1541 to saved CPU state
		for (unsigned i = 0; i < s->driveDelay; ++i) {
			TheCPU1541->CountVIATimers(1);
		}
#endif

	} else {

		// Switch off 1541 processor emulation if it is on
		if (ThePrefs.Emul1541Proc) {
			auto prefs = std::make_unique<Prefs>(ThePrefs);
			prefs->Emul1541Proc = false;
			NewPrefs(prefs.get());
			ThePrefs = *prefs;
		}
	}
}


/*
 *  Save snapshot file (emulation must be paused and in VBlank)
 */

bool C64::SaveSnapshot(const char * filename)
{
	FILE * f = fopen(filename, "wb");
	if (f == nullptr) {
		ShowRequester("Can't create snapshot file", "OK", nullptr);
		return false;
	}

	auto s = std::make_unique<Snapshot>();
	MakeSnapshot(s.get());

	if (fwrite(s.get(), sizeof(Snapshot), 1, f) != 1) {
		ShowRequester("Error writing to snapshot file", "OK", nullptr);
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}


/*
 *  Load snapshot file (emulation must be paused and in VBlank)
 */

bool C64::LoadSnapshot(const char * filename)
{
	FILE * f = f = fopen(filename, "rb");
	if (f == nullptr) {
		ShowRequester("Can't open snapshot file", "OK", nullptr);
		return false;
	}

	auto s = std::make_unique<Snapshot>();

	if (fread(s.get(), sizeof(Snapshot), 1, f) != 1) {
		ShowRequester("Error reading snapshot file", "OK", nullptr);
		fclose(f);
		return false;
	}

	if (memcmp(s->magic, SNAPSHOT_HEADER, sizeof(s->magic)) != 0) {
		ShowRequester("Not a Frodo snapshot file", "OK", nullptr);
		fclose(f);
		return false;
	}

	RestoreSnapshot(s.get());

	fclose(f);
	return true;
}


#if defined(__BEOS__)
#include "C64_Be.h"

#elif defined(HAVE_SDL)
#include "C64_SDL.h"

#elif defined(WIN32)
#include "C64_WIN32.h"
#endif
