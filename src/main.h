/*
 *  main.h - Main program
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

#ifndef _MAIN_H
#define _MAIN_H

#include <filesystem>


class C64;


/*
 *  BeOS specific stuff
 */

#ifdef __BEOS__
#include <AppKit.h>
#include <StorageKit.h>

// Global variables
extern bool FromShell;			// true: Started from shell, SAM can be used
extern BEntry AppDirectory;		// Application directory


// Message codes
const uint32_t MSG_STARTUP = 'strt';			// Start emulation
const uint32_t MSG_PREFS = 'pref';			// Show preferences editor
const uint32_t MSG_PREFS_DONE = 'pdon';		// Preferences editor closed
const uint32_t MSG_RESET = 'rset';			// Reset C64
const uint32_t MSG_NMI = 'nmi ';				// Raise NMI
const uint32_t MSG_SAM = 'sam ';				// Invoke SAM
const uint32_t MSG_NEXTDISK = 'ndsk';		// Insert next disk in drive 8
const uint32_t MSG_TOGGLE_1541 = '1541';		// Toggle processor-level 1541 emulation
const uint32_t MSG_OPEN_SNAPSHOT = 'opss';	// Open snapshot file
const uint32_t MSG_SAVE_SNAPSHOT = 'svss';	// Save snapshot file
const uint32_t MSG_OPEN_SNAPSHOT_RETURNED = 'opsr';	// Open snapshot file panel returned
const uint32_t MSG_SAVE_SNAPSHOT_RETURNED = 'svsr';	// Save snapshot file panel returned


// Application signature
const char APP_SIGNATURE[] = "application/x-vnd.cebix-Frodo";


// Application class
class Frodo : public BApplication {
public:
	Frodo();
	virtual void ArgvReceived(int32_t argc, char **argv);
	virtual void RefsReceived(BMessage *message);
	virtual void ReadyToRun();
	virtual void MessageReceived(BMessage *msg);
	virtual bool QuitRequested();
	virtual void AboutRequested();

private:
	void load_rom(const char *which, const char *path, uint8_t *where, size_t size, const uint8_t *builtin);
	void load_rom_files();

	char prefs_path[1024];		// Pathname of current preferences file
	bool prefs_showing;			// true: Preferences editor is on screen

	BMessenger this_messenger;
	BFilePanel *open_panel;
	BFilePanel *save_panel;
};

#endif


/*
 *  SDL specific stuff
 */

#ifdef HAVE_SDL

class Prefs;

class Frodo {
public:
	Frodo();
	void ArgvReceived(int argc, char **argv);
	void ReadyToRun();
	bool RunPrefsEditor();

private:
	void load_rom(const char *which, const char *path, uint8_t *where, size_t size, const uint8_t *builtin);
	void load_rom_files();

	std::filesystem::path prefs_path;		// Pathname of current preferences file
	std::filesystem::path snapshot_path;	// Directory for saving snapshots
};

extern Frodo *TheApp;  // Pointer to Frodo object

#endif


// Global C64 object
extern C64 *TheC64;


#endif
