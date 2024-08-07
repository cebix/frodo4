/*
 *  dlgDrives.cpp - SDL GUI dialog for C64 drive settings
 *
 *  (C) 2006 Bernd Lachner
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
#include "sdlgui.h"
#include "file.h"

#include "Prefs.h"

#include "dlgFileSelect.h"


enum DRIVESDLG {
	box_main,
	box_drives,
	text_drives,
	text_drive8,
	PATH8TXT,
	PATH8,
	text_drive9,
	PATH9TXT,
	PATH9,
	text_drive10,
	PATH10TXT,
	PATH10,
	text_drive11,
	PATH11TXT,
	PATH11,
	box_options,
	text_options,
	ENABLE_FULL_1541,
	MAP_FILE_NAMES,
	OK,
	CANCEL
};

static char DrivePath[4][18];

/* The keyboard dialog: */
/* Spalte, Zeile, Länge, Höhe*/
static SGOBJ drivesdlg[] =
{
	{ SGBOX, SG_BACKGROUND, 0, 0,0, 35,20, NULL },
	{ SGBOX, 0, 0, 1,2, 33,9, NULL },
	{ SGTEXT, 0, 0, 2, 1, 13, 1, " Drive Paths"},

	{ SGTEXT, 0, 0, 2, 3, 8, 1, "Drive 8:"},
	{ SGEDITFIELD, 0, 0, 12, 3, sizeof(DrivePath[0])-1, 1, DrivePath[0]},
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 3, 4, 1, "Path"},

	{ SGTEXT, 0, 0, 2, 5, 16, 1, "Drive 9:"},
	{ SGEDITFIELD, 0, 0, 12, 5, sizeof(DrivePath[1])-1, 1, DrivePath[1]},
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 5, 4, 1, "Path"},

	{ SGTEXT, 0, 0, 2, 7, 16, 1, "Drive 10:"},
	{ SGEDITFIELD, 0, 0, 12, 7, sizeof(DrivePath[2])-1, 1, DrivePath[2]},
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 7, 4, 1, "Path"},

	{ SGTEXT, 0, 0, 2, 9, 16, 1, "Drive 11:"},
	{ SGEDITFIELD, 0, 0, 12, 9, sizeof(DrivePath[3])-1, 1, DrivePath[3]},
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 9, 4, 1, "Path"},

	{ SGBOX, 0, 0, 1,13, 33,3, NULL },
	{ SGTEXT, 0, 0, 2, 12, 8, 1, " Options"},
	{ SGCHECKBOX, SG_SELECTABLE, 0, 2, 14, 30, 1, "Enable Full 1541 Emulation"},
	{ SGCHECKBOX, SG_SELECTABLE, 0, 2, 15, 30, 1, "Map '/' to '\\` in File Names"},

	{SGBUTTON, SG_SELECTABLE|SG_EXIT|SG_DEFAULT, 0, 1, 18, 6, 1, "OK"},
	{SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 9, 18, 6, 1, "Cancel"},

	{ -1, 0, 0, 0,0, 0,0, NULL }
};

void Dialog_Drives(Prefs &prefs)
{
	// Set values from prefs
	// TODO Check length
	char path[4][MAX_FILENAME_LENGTH];
	for (int i = 0; i < 4; i++)
	{
		int widget = PATH8 + i*3;
		char shrinkedpath[MAX_FILENAME_LENGTH];
		File_ShrinkName(shrinkedpath, prefs.DrivePath[i], drivesdlg[widget-1].w);
		strcpy(drivesdlg[widget-1].txt, shrinkedpath);
		strcpy(path[i], prefs.DrivePath[i]);
	}
	drivesdlg[ENABLE_FULL_1541].state |= prefs.Emul1541Proc == true ? SG_SELECTED : 0;
	drivesdlg[MAP_FILE_NAMES].state |= prefs.MapSlash == true ? SG_SELECTED : 0;
	while (1)
	{
		int widget = SDLGui_DoDialog(drivesdlg);
		switch (widget)
		{
			case PATH8:
			case PATH9:
			case PATH10:
			case PATH11:
				{	
					if (SDLGui_FileSelect(path[(widget-PATH8)/3], false))
					{
						char shrinkedpath[MAX_FILENAME_LENGTH];
						File_ShrinkName(shrinkedpath, path[(widget-PATH8)/3], drivesdlg[widget-1].w);
						strcpy(drivesdlg[widget-1].txt, shrinkedpath);
					}
				}
			break;
			case OK:
				for (int i = 0; i < 4; i++)
				{
					strcpy(prefs.DrivePath[i], path[i]);
					fprintf (stderr, "Path(%s)\n", prefs.DrivePath[i]);
				}
				prefs.Emul1541Proc = drivesdlg[ENABLE_FULL_1541].state &= SG_SELECTED ? true : false;
				prefs.MapSlash = drivesdlg[MAP_FILE_NAMES].state &= SG_SELECTED ? true : false;
				return;
			case CANCEL:
				return;
		}		
	}
}

