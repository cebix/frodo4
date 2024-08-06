/*
 *
 * This file is taken from the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * This file is distributed under the GNU Public License, version 2 or at
 * your option any later version. Read the file gpl.txt for details.
 *
 */

#ifndef _SDLGUI_H
#define _SDLGUI_H

#include <SDL.h>

enum
{
  SGBOX,
  SGTEXT,
  SGEDITFIELD,
  SGBUTTON,
  SGCHECKBOX,
  SGPOPUP
};


/* Object flags: */
#define SG_TOUCHEXIT     1
#define SG_EXIT          2
#define SG_BUTTON_RIGHT  4
#define SG_DEFAULT       8
#define SG_SELECTABLE   16
#define SG_BACKGROUND   32
#define SG_RADIO        64

/* Object states: */
#define SG_SELECTED   1
#define SG_HIDDEN     2
#define SG_DISABLED   4

/* Special characters: */
#define SGCHECKBOX_RADIO_NORMAL   12
#define SGCHECKBOX_RADIO_SELECTED 13
#define SGCHECKBOX_NORMAL         14
#define SGCHECKBOX_SELECTED       15
#define SGARROWUP    1
#define SGARROWDOWN  2
#define SGFOLDER     5


typedef struct
{
  int type;             /* What type of object */
  int flags;            /* Object flags */
  int state;		/* Object state */
  int x, y;             /* The offset to the upper left corner */
  unsigned int w, h;             /* Width and height */
  char *txt;            /* Text string */
}  SGOBJ;

typedef struct
{
  int object;
  int position;
  int blink_counter;
  bool blink_state;
} cursor_state;

extern void screenlock();
extern void screenunlock();

bool SDLGui_Init(SDL_Surface *GUISurface);
int SDLGui_UnInit(void);
int SDLGui_DoDialog(SGOBJ *dlg);
int SDLGui_PrepareFont(void);
void SDLGui_FreeFont(void);

SDL_Rect *SDLGui_GetFirstBackgroundRect(void);
SDL_Rect *SDLGui_GetNextBackgroundRect(void);

SDL_Event getEvent(SGOBJ *dlg, cursor_state *cursor);

#endif /* _SDLGUI_H */
