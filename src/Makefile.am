bin_PROGRAMS = Frodo FrodoSC

common_SOURCES = \
    main.cpp main.h Display.cpp Display.h Prefs.cpp Prefs.h SID.cpp SID.h REU.cpp REU.h \
    IEC.cpp IEC.h 1541fs.cpp 1541fs.h 1541d64.cpp 1541d64.h 1541t64.cpp 1541t64.h 1541job.cpp 1541job.h \
    SAM.cpp SAM.h 1541_ROM.h Basic_ROM.h Char_ROM.h Kernal_ROM.h Version.h

Frodo_SOURCES = $(common_SOURCES) \
    C64.cpp C64.h CPUC64.cpp CPUC64.h VIC.cpp VIC.h CIA.cpp CIA.h CPU1541.cpp CPU1541.h CPU_emulline.h

FrodoSC_SOURCES = $(common_SOURCES) \
    C64_SC.cpp CPUC64_SC.cpp VIC_SC.cpp CIA_SC.cpp CPU1541_SC.cpp CPU_common.cpp CPU_common.h CPU_emulcycle.h

EXTRA_Frodo_SOURCES = sysdeps.h debug.h file.cpp file.h \
    main_SDL.h C64_SDL.h Display_SDL.h SID_SDL.h SID_catweasel.h Prefs_glade.h Prefs_none.h \
    sdlgui.cpp sdlgui.h dlgFileSelect.cpp dlgFileSelect.h dlgMain.cpp dlgDrives.cpp dlgOptions.cpp dlgVideoSound.cpp dlgAdvanced.cpp dlgInput.cpp

common_CPPFLAGS = -DDATADIR=\"$(pkgdatadir)/\"

Frodo_CPPFLAGS = -DPRECISE_CPU_CYCLES=1 -DPRECISE_CIA_CYCLES=1 -DPC_IS_POINTER=0 $(common_CPPFLAGS)
FrodoSC_CPPFLAGS = -DFRODO_SC $(common_CPPFLAGS)

common_LDADD = $(GUI_OBJS)

Frodo_LDADD = $(common_LDADD)
FrodoSC_LDADD = $(common_LDADD)

dist_pkgdata_DATA = Frodo.ui Frodo_Logo.png
