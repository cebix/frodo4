/*
 *  main.cpp - Main program
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

#include "main.h"
#include "C64.h"
#include "Prefs.h"
#include "Version.h"

#ifdef HAVE_GLADE
#include <gtk/gtk.h>
#endif

#include <SDL.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <memory>
namespace fs = std::filesystem;


// Global variables
Frodo * TheApp = nullptr;	// Application object
C64 * TheC64 = nullptr;		// Global C64 object


// ROM file names
#ifndef DATADIR
#define DATADIR
#endif

#define BASIC_ROM_FILE DATADIR "Basic ROM"
#define KERNAL_ROM_FILE DATADIR "Kernal ROM"
#define CHAR_ROM_FILE DATADIR "Char ROM"
#define DRIVE_ROM_FILE DATADIR "1541 ROM"

// Builtin ROMs
#include "Basic_ROM.h"
#include "Kernal_ROM.h"
#include "Char_ROM.h"
#include "1541_ROM.h"


/*
 *  Process command line arguments
 */

void Frodo::ProcessArgs(int argc, char ** argv)
{
	if (argc == 2) {
		prefs_path = argv[1];
	}
}


/*
 *  Arguments processed, run emulation
 */

#ifdef HAVE_GLADE
static gboolean pump_sdl_events(gpointer user_data)
{
	SDL_Event event;
	SDL_WaitEventTimeout(&event, 5);
	return true;
}
#endif

void Frodo::ReadyToRun()
{
	// Load preferences
	if (prefs_path.empty()) {
		auto path = SDL_GetPrefPath("cebix", "Frodo");
		prefs_path = fs::path(path) / "config";
		snapshot_path = fs::path(path) / "snapshots";

		// Create snapshot directory if it doesn't exist
		if (! fs::exists(snapshot_path)) {
			fs::create_directories(snapshot_path);
		}
	}
	ThePrefs.Load(prefs_path);

#ifdef HAVE_GLADE
	// Show preferences editor
	if (!ThePrefs.ShowEditor(true, prefs_path, snapshot_path))
		return;  // "Quit" clicked

	// Keep SDL event loop running while preferences editor is open the next time
	g_idle_add(pump_sdl_events, nullptr);
#endif

	// Create and start C64
	TheC64 = new C64;
	load_rom_files();
	TheC64->Run();

	delete TheC64;
}


/*
 *  Run preferences editor
 */

bool Frodo::RunPrefsEditor()
{
	auto prefs = std::make_unique<Prefs>(ThePrefs);
	bool result = prefs->ShowEditor(false, prefs_path, snapshot_path);
	if (result) {
		TheC64->NewPrefs(prefs.get());
		ThePrefs = *prefs;
	}
	return result;
}


/*
 *  Load C64 ROM files
 */

static void load_rom(const char *which, const char *path, uint8_t *where, size_t size, const uint8_t *builtin)
{
	FILE *f = fopen(path, "rb");
	if (f) {
		size_t actual = fread(where, 1, size, f);
		fclose(f);
		if (actual == size)
			return;
	}

	// Use builtin ROM
	printf("%s ROM file (%s) not readable, using builtin.\n", which, path);
	memcpy(where, builtin, size);
}

void Frodo::load_rom_files()
{
	load_rom("Basic", BASIC_ROM_FILE, TheC64->Basic, BASIC_ROM_SIZE, builtin_basic_rom);
	load_rom("Kernal", KERNAL_ROM_FILE, TheC64->Kernal, KERNAL_ROM_SIZE, builtin_kernal_rom);
	load_rom("Char", CHAR_ROM_FILE, TheC64->Char, CHAR_ROM_SIZE, builtin_char_rom);
	load_rom("1541", DRIVE_ROM_FILE, TheC64->ROM1541, DRIVE_ROM_SIZE, builtin_drive_rom);
}


/*
 *  Create application object and start it
 */

int main(int argc, char ** argv)
{
#ifdef HAVE_GLADE
	gtk_init(&argc, &argv);
#else
	printf(
		"%s Copyright (C) Christian Bauer\n"
		"This is free software with ABSOLUTELY NO WARRANTY.\n"
		, VERSION_STRING
	);
	fflush(stdout);
#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "Couldn't initialize SDL (%s)\n", SDL_GetError());
		return 1;
	}

	// Seed RNG
	std::srand(std::time(0));

	// Run Frodo application
	TheApp = new Frodo();
	TheApp->ProcessArgs(argc, argv);
	TheApp->ReadyToRun();

	// Shutdown
	delete TheApp;
	SDL_Quit();

	return 0;
}
