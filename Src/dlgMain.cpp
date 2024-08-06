/*
 *  dlgMain.cpp - Main SDL GUI dialog
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

#include "dlgMain.h"
#include "sdlgui.h"

extern void Dialog_Drives(Prefs &prefs);
extern void Dialog_Options(Prefs &prefs);
extern void Dialog_VideoSound(Prefs &prefs);
extern void Dialog_Advanced(Prefs &prefs);
extern void Dialog_Input(Prefs &prefs);

enum MAINDLG {
	box_main,
	box_settings,
	text_settings,
	DRIVES,
	VIDEOSOUND,
	INPUT,
	OPTIONS,
	ADVANCED,
	APPLY,
	CANCEL,
	SAVE,
	box_quickaccess,
	text_quickaccess,
	RESET,
	QUIT,
	ABOUT
};

/* The keyboard dialog: */
/* Spalte, Zeile, Länge, Höhe*/
static SGOBJ maindlg[] =
{
	{ SGBOX, SG_BACKGROUND, 0, 0,0, 35,20, NULL },
	{ SGBOX, 0, 0, 1,2, 33,10, NULL },
	{ SGTEXT, 0, 0, 2, 1, 16, 1, " Frodo Settings "},
	
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 2, 3, 14,1, "Drives" },
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 2, 5, 14,1, "Video/Sound" },
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 2, 7, 14,1, "Input" },
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 18,3, 14,1, "Options" },
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 18,5, 14,1, "Advanced" },

	{ SGBUTTON, SG_SELECTABLE|SG_EXIT|SG_DEFAULT, 0, 2,10,8,1, "Apply" },
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 13,10, 8,1, "Cancel" },
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 24,10, 8,1, "Save" },

	{ SGBOX, 0, 0, 1,15, 33,3, NULL },
	{ SGTEXT, 0, 0, 2, 14, 22, 1, " Quick-Access-Buttons "},

	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 2,16,8,1, "Reset" },
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 13,16, 8,1, "Quit" },
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 24,16, 8,1, "About" },

	{ -1, 0, 0, 0,0, 0,0, NULL }
};

int Dialog_Main(Prefs &prefs)
{
	// apply
	while (1)
	{
		switch (SDLGui_DoDialog(maindlg))
		{
			case DRIVES:
				Dialog_Drives(prefs);
				break;
			case OPTIONS:
				Dialog_Options(prefs);
				break;
			case VIDEOSOUND:
				Dialog_VideoSound(prefs);
				break;
			case ADVANCED:
				Dialog_Advanced(prefs);
				break;
			case INPUT:
				Dialog_Input(prefs);
				break;
			case APPLY:
				return DO_USEPREFS;
			case SAVE:
				return DO_SAVEPREFS;
			case RESET:
				return DO_RESET;
			case QUIT:
				return DO_QUIT;
			case CANCEL:
				return DO_NOTHING;
		}
	}
	return DO_NOTHING;
}

