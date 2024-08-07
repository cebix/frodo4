/*
 *  dlgAdvanced.cpp - SDL GUI dialog for C64 advanced options
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

enum ADVANCEDDLG {
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

static char Cycles[4][4];

/* The keyboard dialog: */
/* Spalte, Zeile, Länge, Höhe*/
static SGOBJ advanceddlg[] =
{
	{ SGBOX, SG_BACKGROUND, 0, 0,0, 35,20, NULL },
	{ SGBOX, 0, 0, 1,2, 33,9, NULL },
	{ SGTEXT, 0, 0, 2, 1, 16, 1, " Timing Control"},

	{ SGTEXT, 0, 0, 2, 3, 16, 1, "Cycles / Line (CPU):"},
	{ SGEDITFIELD, 0, 0, 26, 3, sizeof(Cycles[0])-1, 1, Cycles[0]},
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 3, 1, 1, "\x01"},
	/* Arrow up */
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 32, 3, 1, 1, "\x02"},
	/* Arrow down */

	{ SGTEXT, 0, 0, 2, 5, 16, 1, "Cycles / bad line (CPU):"},
	{ SGEDITFIELD, 0, 0, 26, 5, sizeof(Cycles[1])-1, 1, Cycles[1]},
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 5, 1, 1, "\x01"},
	/* Arrow up */
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 32, 5, 1, 1, "\x02"},
	/* Arrow down */

	{ SGTEXT, 0, 0, 2, 7, 16, 1, "Cycles / Line (CIA):"},
	{ SGEDITFIELD, 0, 0, 26, 7, sizeof(Cycles[2])-1, 1, Cycles[2]},
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 7, 1, 1, "\x01"},
	/* Arrow up */
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 32, 7, 1, 1, "\x02"},
	/* Arrow down */

	{ SGTEXT, 0, 0, 2, 9, 16, 1, "Cycles / Line (1541):"},
	{ SGEDITFIELD, 0, 0, 26, 9, sizeof(Cycles[3])-1, 1, Cycles[3]},
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 9, 1, 1, "\x01"},
	/* Arrow up */
	{ SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 32, 9, 1, 1, "\x02"},
	/* Arrow down */

	{ SGBOX, 0, 0, 1,13, 33,3, NULL },
	{ SGTEXT, 0, 0, 2, 12, 22, 1, " Advanced Options"},
	{ SGCHECKBOX, SG_SELECTABLE, 0, 2, 14, 30, 1, "Clear CIA ICR on Write Access"},

	{ SGBUTTON, SG_SELECTABLE|SG_EXIT|SG_DEFAULT, 0, 1, 18, 6, 1, "OK"},
	{ SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 9, 18, 6, 1, "Cancel"},

	{ -1, 0, 0, 0,0, 0,0, NULL }
};

void Dialog_Advanced(Prefs &prefs)
{
	switch (SDLGui_DoDialog(advanceddlg))
	{
	}
}

