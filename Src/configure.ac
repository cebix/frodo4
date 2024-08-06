dnl Process this file with autoconf to produce a configure script.

AC_INIT([Frodo], [4.2])
AC_CONFIG_SRCDIR(main.cpp)
AC_PREREQ(2.52)
AC_CONFIG_HEADER(sysconfig.h)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

dnl Configure options.
AC_ARG_ENABLE(opengl, [  --enable-opengl         use SDL/OpenGL for rendering [default=yes]], , enable_opengl=yes)

dnl -fomit-frame-pointer makes things faster
if [[ x"$GXX" = "xyes" ]]; then
  CFLAGS="$CFLAGS -fomit-frame-pointer -fno-exceptions"
fi

AC_AIX
AC_ISC_POSIX

dnl Checks for libraries.
AC_CHECK_LIB(vga, vga_setmode, HAVE_SVGA_LIB=yes, HAVE_SVGA_LIB=no)
AM_PATH_SDL(1.2.0, HAVE_SDL=yes, HAVE_SDL=no)

EMBEDDED_EXTENSION=none
AC_MSG_CHECKING([if Frodo should be compiled with extensions for an embedded device])
AC_ARG_WITH([embedded-extension], [AS_HELP_STRING([--with-embedded-extension=qtopia|maemo],[Specify which extension for Frodo on an embedded device should be used.])])
if [[ x"$with_embedded_extension" == "xqtopia" ]]; then
  EMBEDDED_EXTENSION=$with_embedded_extension
  AC_DEFINE(QTOPIA, 1, [Extension for Qtopia is enabled])
elif [[ x"$with_embedded_extension" == "xmaemo" ]]; then
  EMBEDDED_EXTENSION=$with_embedded_extension
  AC_DEFINE(MAEMO, 1, [Extension for Maemo is enabled])
fi
AC_MSG_RESULT([$EMBEDDED_EXTENSION])

if [[ x"$EMBEDDED_EXTENSION" == "xmaemo" ]]; then
  PKG_CHECK_MODULES(OSSO, [libosso >= 0.9.19], AC_DEFINE(HAVE_LIBOSSO, [], [Weather libosso is present on the system]), [AC_MSG_NOTICE([libosso not present.])])
fi

if [[ x"$EMBEDDED_EXTENSION" == "xnone" ]]; then
  HAVE_GLADE=no
  PKG_CHECK_MODULES(GLADE, [libglade-2.0 libgnomeui-2.0 >= 1.110.0], HAVE_GLADE=yes)
  if [[ $HAVE_GLADE = yes ]]; then
    AC_DEFINE(HAVE_GLADE, 1, [Glade support is enabled])
    LDFLAGS="$LDFLAGS -Wl,-E"
  fi
else
  GUIOBJS="sdlgui.o file.o dlgFileSelect.o dlgMain.o dlgDrives.o dlgOptions.o dlgVideoSound.o dlgAdvanced.o dlgInput.o"
  GUISRCS="sdlgui.cpp file.cpp dlgFileSelect.cpp dlgMain.cpp dlgDrives.cpp dlgOptions.cpp dlgVidoSound.cpp dlgAdvanced.cpp dlgInput.cpp"
fi

AC_PATH_XTRA

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h fcntl.h sys/time.h sys/types.h utime.h string.h strings.h values.h ncurses.h)
AC_CHECK_HEADERS(sys/vfs.h sys/mount.h sys/select.h sys/param.h sys/statfs.h sys/statvfs.h sys/stat.h)

dnl Checks for types.
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(void *)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_STRUCT_ST_BLOCKS
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS(gettimeofday sigaction mkdir rmdir select strerror strstr statfs usleep)

KBD_LANG=0
AC_ARG_ENABLE(kbd-lang-de,[  --enable-kbd-lang-de    Use german keyboard layout],[KBD_LANG=1],[])
AC_ARG_ENABLE(kbd-lang-us,[  --enable-kbd-lang-us    Use american keyboard layout],[KBD_LANG=0],[])

AC_MSG_CHECKING(which target to use)
if [[ x"$no_x" = "xyes" ]]; then
  if [[ x"$HAVE_SVGA_LIB" = "xyes" ]]; then
    AC_MSG_RESULT([SVGAlib])
    TARGET=svgalib
    CFLAGS="$CFLAGS -D__svgalib__"
    LIBS="$LIBS -lvga"
  else
    AC_MSG_RESULT([Ummm...])
    AC_MSG_ERROR([Neither X nor SVGAlib found, don't know what target to use.])
  fi
elif [[ x"$HAVE_SDL" = "xyes" ]]; then
  AC_MSG_RESULT([SDL])
  TARGET=sdl
  CFLAGS="$CFLAGS $SDL_CFLAGS -DHAVE_SDL"
  LIBS="$LIBS $SDL_LIBS"
  if [[ "x$enable_opengl" = "xyes" ]]; then
    AC_MSG_CHECKING(for OpenGL support)
    have_opengl=no
    AC_TRY_COMPILE([
     #include <GL/glew.h>
    ],[
    ],[
    have_opengl=yes
    ])
    AC_MSG_RESULT($have_opengl)
    if test x$have_opengl = xyes; then
      AC_DEFINE(ENABLE_OPENGL, 1, [OpenGL support is enabled])
      LIBS="$LIBS -lGLEW"
    else
      AC_MSG_WARN([Could not find OpenGL, ignoring --enable-opengl.])
    fi
  fi
else
  AC_MSG_RESULT([X Window System])
  TARGET=x11
  CFLAGS="$CFLAGS $X_CFLAGS -I$ac_cv_x_include"
  LIBS="$LIBS $X_LIBS $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS"
fi

AC_SUBST(TARGET)
AC_SUBST(SET_MAKE)
AC_SUBST(top_srcdir)
AC_SUBST(HPUX_REV)
AC_SUBST(KBD_LANG)
AC_SUBST(GUIOBJS)
AC_SUBST(GUISRCS)

dnl Generate Makefile.
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl Print summary.
echo
echo "Configuration done. Now type \"make\"."
