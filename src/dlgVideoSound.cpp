/*
 *  dlgVideoSound.cpp - SDL GUI dialog for C64 video and sound options
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

#include "Prefs.h"

enum VIDEOSOUNDDDLG {
	box_main,
	box_timing,
	text_timing,
	text_line_cpu,
	LINE_CPU,
	LINE_UP_CPU,
	LINE_DOWN_CPU,
	text_bad_line_cpu,
	BAD_LINE_CPU,
	BAD_LINE_UP_CPU,
	BAD_LINE_DOWN_CPU,
	text_line_cia,
	LINE_CIA,
	LINE_UP_CIA,
	LINE_DOWN_CIA,
	text_line_1541,
	LINE_1541,
	LINE_UP_1541,
	LINE_DOWN_1541,
	box_advancedoptions,
	text_advancedoptions,
	CLEAR_CIA_ICR,
	OK,
	CANCEL
};

/* The keyboard dialog: */
/* Spalte, Zeile, Länge, Höhe*/
static SGOBJ videosounddlg[] =
{
	{ SGBOX, SG_BACKGROUND, 0, 0,0, 35,20, NULL },
	{ SGBOX, 0, 0, 1,2, 33, 7, NULL },
	{ SGTEXT, 0, 0, 2, 1, 15, 1, " Video Options"},
	{ SGCHECKBOX, SG_SELECTABLE, 0, 2, 3, 30, 1, "Display Sprites"},
	{ SGCHECKBOX, SG_SELECTABLE, 0, 2, 5, 30, 1, "Detect Sprite Collisions"},

	{ SGBOX, 0, 0, 1, 11, 33, 5, NULL },
	{ SGTEXT, 0, 0, 2, 10, 15, 1, " Sound Options"},
	{ SGTEXT, 0, 0, 2, 12, 10, 1, "Sound Emu:"},
	{ SGCHECKBOX, SG_SELECTABLE|SG_RADIO, 0, 13, 12, 12, 1, "Off" },
	{ SGCHECKBOX, SG_SELECTABLE|SG_RADIO, 0, 20, 12, 20,1, "Software" },
	{ SGCHECKBOX, SG_SELECTABLE, 0, 2, 14, 30, 1, "Enable SID Filters"},

	{SGBUTTON, SG_SELECTABLE|SG_EXIT|SG_DEFAULT, 0, 1, 18, 6, 1, "OK"},
	{SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 9, 18, 6, 1, "Cancel"},

	{ -1, 0, 0, 0,0, 0,0, NULL }
};

void Dialog_VideoSound(Prefs &prefs)
{
	switch (SDLGui_DoDialog(videosounddlg))
	{
	}
}

