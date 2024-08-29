/*
 *  main_SDL.h - Main program, SDL specific stuff
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

#include "Version.h"

#ifdef HAVE_GLADE
#include <gtk/gtk.h>
#endif

#include <SDL.h>

#include <cstdlib>
#include <ctime>

#include <iostream>


// Global variables
Frodo * TheApp = nullptr;


/*
 *  Create application object and start it
 */

int main(int argc, char **argv)
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
	TheApp->ArgvReceived(argc, argv);
	TheApp->ReadyToRun();

	// Shutdown
	delete TheApp;
	SDL_Quit();

	return 0;
}


/*
 *  Constructor: Initialize member variables
 */

Frodo::Frodo()
{
	TheC64 = nullptr;
}


/*
 *  Process command line arguments
 */

void Frodo::ArgvReceived(int argc, char **argv)
{
	if (argc == 2) {
		prefs_path = argv[1];
	}
}


/*
 *  Arguments processed, run emulation
 */

void Frodo::ReadyToRun()
{
	getcwd(AppDirPath, 256);

	// Load preferences
	if (prefs_path.empty()) {
		prefs_path = SDL_GetPrefPath("cebix", "Frodo");
		prefs_path += "config";
	}
	ThePrefs.Load(prefs_path.c_str());

	// Show preferences editor
#ifdef HAVE_GLADE
	if (!ThePrefs.ShowEditor(true, prefs_path.c_str()))
		return;  // "Quit" clicked
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
	Prefs *prefs = new Prefs(ThePrefs);
	bool result = prefs->ShowEditor(false, prefs_path.c_str());
	if (result) {
		TheC64->NewPrefs(prefs);
		ThePrefs = *prefs;
	}
	delete prefs;
	return result;
}
