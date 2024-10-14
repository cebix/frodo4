bin_PROGRAMS = Frodo FrodoLite

common_SOURCES = \
    main.cpp main.h Display.cpp Display.h Prefs.cpp Prefs.h SID.cpp SID.h REU.cpp REU.h \
    IEC.cpp IEC.h 1541fs.cpp 1541fs.h 1541d64.cpp 1541d64.h 1541t64.cpp 1541t64.h 1541gcr.cpp 1541gcr.h \
    Cartridge.cpp Cartridge.h SAM.cpp SAM.h 1541_ROM.h Basic_ROM.h Char_ROM.h Kernal_ROM.h Version.h \
    C64.h CPUC64.h CPU1541.h VIC.h CIA.h VIA.h

Frodo_SOURCES = $(common_SOURCES) \
    C64_SC.cpp CPUC64_SC.cpp VIC_SC.cpp CIA_SC.cpp CPU1541_SC.cpp VIA_SC.cpp \
    CPU_common.cpp CPU_common.h CPU_emulcycle.h

FrodoLite_SOURCES = $(common_SOURCES) \
    C64.cpp CPUC64.cpp VIC.cpp CIA.cpp CPU1541.cpp VIA.cpp \
    CPU_emulline.h

EXTRA_Frodo_SOURCES = sysdeps.h debug.h SID_catweasel.h Prefs_gtk.h Prefs_none.h

common_CPPFLAGS = -DDATADIR=\"$(pkgdatadir)/\" -DHTMLDIR=\"$(htmldir)/\"

Frodo_CPPFLAGS = -DFRODO_SC $(common_CPPFLAGS)
FrodoLite_CPPFLAGS = -DPRECISE_CPU_CYCLES=1 -DPRECISE_CIA_CYCLES=1 $(common_CPPFLAGS)

dist_pkgdata_DATA = Frodo.ui Frodo_Logo.png
