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

#ifdef HAVE_GTK
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


/*
 *  Process command line arguments
 */

void Frodo::ProcessArgs(int argc, char ** argv)
{
#ifdef HAVE_GTK
	gchar * config_file = nullptr;
	gchar ** remaining_args = nullptr;

	static GOptionEntry entries[] = {
		{ "config", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, &config_file, "Run with a different configuration file than the default", "FILE" },
		{ G_OPTION_REMAINING, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING_ARRAY, &remaining_args, nullptr, "[ITEM=VALUE…]" },
		{ nullptr, 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr }
	};

	GOptionContext * context = g_option_context_new("- Commodore 64 emulator");
	g_option_context_add_main_entries(context, entries, nullptr);

	GError * error = nullptr;
	if (! g_option_context_parse(context, &argc, &argv, &error)) {
		g_print("Error on command line: %s\n", error->message);
		SDL_Quit();
		exit(1);
	}

	if (config_file) {
		prefs_path = config_file;
		g_free(config_file);
	}

	if (remaining_args) {
		for (auto arg = remaining_args; *arg != nullptr; ++arg) {
			prefs_override.push_back(*arg);
		}
		g_strfreev(remaining_args);
	}
#endif
}


/*
 *  Arguments processed, run emulation
 */

int Frodo::ReadyToRun()
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

		SDL_free(path);
	}
	ThePrefs.Load(prefs_path);

	// Override preferences items given on command line
	for (auto & item : prefs_override) {
		ThePrefs.ParseItem(item);
	}

#ifdef HAVE_GTK
	// Show preferences editor
	if (! ThePrefs.AutoStart) {
		if (! ThePrefs.ShowEditor(true, prefs_path, snapshot_path))
			return 0;  // "Quit" clicked
	}
#endif

	// Create and start C64
	TheC64 = new C64;
	int exit_code = TheC64->Run();

	// Shutdown
	delete TheC64;

	// Save preferences
	ThePrefs.Save(prefs_path);

	return exit_code;
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
 *  Create application object and start it
 */

int main(int argc, char ** argv)
{
#ifdef HAVE_GTK
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
		fprintf(stderr, "Cannot initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	// Seed RNG
	std::srand(std::time(0));

	// Run Frodo application
	TheApp = new Frodo();
	TheApp->ProcessArgs(argc, argv);
	int exit_code = TheApp->ReadyToRun();

	// Shutdown
	delete TheApp;
	SDL_Quit();

	return exit_code;
}
