/*
 *  dlgOptions.cpp - SDL GUI dialog for C64 emulator options
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

enum OPTIONSDDLG {
	box_main,
	box_speedcontrol,
	text_speedcontrol,
	text_draweverynthframe,
	DRAW_EVERYNTH_FRAME,
	DRAW_EVERYNTH_FRAME_UP,
	DRAW_EVERYNTH_FRAME_DOWN,
	LIMIT_SPEED_TO100PERCENT,
	FAST_RESET,
	box_memoryexpansion,
	text_memoryexpansion,
	text_reu_size,
	RADIO_REU_NONE,
	RADIO_REU_128K,
	RADIO_REU_256K,
	RADIO_REU_512K,
	OK,
	CANCEL
};

/* The keyboard dialog: */
/* Spalte, Zeile, Länge, Höhe*/
static SGOBJ optionsdlg[] =
{
	{ SGBOX, SG_BACKGROUND, 0, 0,0, 35,20, NULL },
	{ SGBOX, 0, 0, 1,2, 33,9, NULL },
	{ SGTEXT, 0, 0, 2, 1, 15, 1, " Speed Control"},

	{ SGTEXT, 0, 0, 2, 3, 16, 1, "Draw every n-th frame:"},
	{ SGEDITFIELD, 0, 0, 26, 3, 3, 1, "   "},
	{SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 30, 3, 1, 1, "\x01"},
	/* Arrow up */
	{SGBUTTON, SG_SELECTABLE | SG_TOUCHEXIT, 0, 32, 3, 1, 1, "\x02"},
	/* Arrow down */
	{ SGCHECKBOX, SG_SELECTABLE, 0, 2, 5, 30, 1, "Limit Speed to 100%"},
	{ SGCHECKBOX, SG_SELECTABLE, 0, 2, 7, 30, 1, "Fast Reset"},

	{ SGBOX, 0, 0, 1,13, 33,3, NULL },
	{ SGTEXT, 0, 0, 2, 12, 18, 1, " Memory Expansion"},
	{ SGTEXT, 0, 0, 2, 14, 30, 1, "REU Size:"},
	{ SGCHECKBOX, SG_SELECTABLE|SG_RADIO, 0, 2, 15, 4, 1, "None" },
	{ SGCHECKBOX, SG_SELECTABLE|SG_RADIO, 0, 10, 15, 20,1, "128K" },
	{ SGCHECKBOX, SG_SELECTABLE|SG_RADIO, 0, 18, 15, 20,1, "256K" },
	{ SGCHECKBOX, SG_SELECTABLE|SG_RADIO, 0, 26, 15, 20,1, "512K" },

	{SGBUTTON, SG_SELECTABLE|SG_EXIT|SG_DEFAULT, 0, 1, 18, 6, 1, "OK"},
	{SGBUTTON, SG_SELECTABLE|SG_EXIT, 0, 9, 18, 6, 1, "Cancel"},

	{ -1, 0, 0, 0,0, 0,0, NULL }
};

void Dialog_Options(Prefs &prefs)
{
	// Set values from prefs
// TODO	optionsdlg[DRAW_EVERYNTH_FRAME].txt = prefs.SkipFrames;
	optionsdlg[LIMIT_SPEED_TO100PERCENT].state |= prefs.LimitSpeed == true ? SG_SELECTED : 0;
	optionsdlg[FAST_RESET].state |= prefs.FastReset == true ? SG_SELECTED : 0;
	switch (prefs.REUSize)
	{
		case REU_NONE:
			optionsdlg[RADIO_REU_NONE].state |= SG_SELECTED;	
			break;
		case REU_128K:
			optionsdlg[RADIO_REU_128K].state |= SG_SELECTED;
			break;
		case REU_256K:
			optionsdlg[RADIO_REU_256K].state |= SG_SELECTED;
			break;
		case REU_512K:
			optionsdlg[RADIO_REU_512K].state |= SG_SELECTED;
			break;		
	}
	switch (SDLGui_DoDialog(optionsdlg))
	{
		case OK:
			// Set values to prefs
			prefs.LimitSpeed = optionsdlg[LIMIT_SPEED_TO100PERCENT].state &= SG_SELECTED ? true : false;
			prefs.FastReset = optionsdlg[FAST_RESET].state &= SG_SELECTED ? true : false;
			for (int i = RADIO_REU_NONE; i <= RADIO_REU_512K; i++)
			{
				if (optionsdlg[i].state &= SG_SELECTED)
				{
					prefs.REUSize = i - RADIO_REU_NONE;
					break;
				}
			}
			break;
	}
}

