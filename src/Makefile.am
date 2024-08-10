bin_PROGRAMS = Frodo FrodoSC

common_SOURCES = \
    main.cpp main.h Display.cpp Display.h Prefs.cpp Prefs.h SID.cpp SID.h REU.cpp REU.h \
    IEC.cpp IEC.h 1541fs.cpp 1541fs.h 1541d64.cpp 1541d64.h 1541t64.cpp 1541t64.h 1541job.cpp 1541job.h \
    SAM.cpp SAM.h CmdPipe.cpp CmdPipe.h \
    1541_ROM.h Basic_ROM.h Char_ROM.h Kernal_ROM.h Version.h

Frodo_SOURCES = $(common_SOURCES) \
    C64.cpp C64.h CPUC64.cpp CPUC64.h VIC.cpp VIC.h CIA.cpp CIA.h CPU1541.cpp CPU1541.h CPU_emulline.h

FrodoSC_SOURCES = $(common_SOURCES) \
    C64_SC.cpp CPUC64_SC.cpp VIC_SC.cpp CIA_SC.cpp CPU1541_SC.cpp CPU_common.cpp CPU_common.h CPU_emulcycle.h

EXTRA_Frodo_SOURCES = sysdeps.h debug.h file.cpp file.h \
    main_x.h C64_x.h C64_Embedded.h Display_SDL.h Display_EmbeddedSDL.h Display_x.h \
    SID_SDL.h SID_catweasel.h SID_linux.h \
    Prefs_glade.h Prefs_x.h \
    sdlgui.cpp sdlgui.h dlgFileSelect.cpp dlgFileSelect.h dlgMain.cpp dlgDrives.cpp dlgOptions.cpp dlgVideoSound.cpp dlgAdvanced.cpp dlgInput.cpp

common_CPPFLAGS = $(GUI_CFLAGS) $(OSSO_CFLAGS) \
    -DKBD_LANG=$(KBD_LANG) -DDATADIR=\"$(pkgdatadir)/\"

Frodo_CPPFLAGS = -DPRECISE_CPU_CYCLES=1 -DPRECISE_CIA_CYCLES=1 -DPC_IS_POINTER=0 $(common_CPPFLAGS)
FrodoSC_CPPFLAGS = -DFRODO_SC $(common_CPPFLAGS)

common_LDADD = $(GUI_OBJS) $(OSSO_LIBS)

Frodo_LDADD = $(common_LDADD)
FrodoSC_LDADD = $(common_LDADD)

dist_pkgdata_DATA = glade/Frodo.glade
