dnl Information on the package.
AC_PREREQ([2.71])
AC_INIT([Frodo],[4.2], [], [Frodo])
AC_CONFIG_SRCDIR([src/main.cpp])
AM_INIT_AUTOMAKE([1.12 foreign])

dnl Checks for programs.
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX(20, noext, mandatory)
PKG_PROG_PKG_CONFIG

dnl Checks for libraries.
AM_PATH_SDL2(2.30.0, AC_DEFINE(HAVE_SDL, 1, [SDL support is enabled]), AC_MSG_ERROR([*** Required SDL version not found]))
CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

HAVE_GLADE=no
PKG_CHECK_MODULES(GLADE, [gtk+-3.0 >= 3.24], HAVE_GLADE=yes)
if [[ $HAVE_GLADE = yes ]]; then
  AC_DEFINE(HAVE_GLADE, 1, [GTK preferences GUI is enabled])
  CPPFLAGS="$CPPFLAGS $GLADE_CFLAGS"
  LIBS="$LIBS $GLADE_LIBS"
  LDFLAGS="$LDFLAGS -rdynamic"
fi
GUI_OBJS=""
dnl GUI_OBJS="sdlgui.o file.o dlgFileSelect.o dlgMain.o dlgDrives.o dlgOptions.o dlgVideoSound.o dlgAdvanced.o dlgInput.o"

dnl Checks for headers.
AC_HEADER_DIRENT

AC_CHECK_HEADERS(unistd.h fcntl.h sys/time.h sys/types.h utime.h string.h)
AC_CHECK_HEADERS(sys/vfs.h sys/mount.h sys/select.h sys/param.h sys/statfs.h sys/statvfs.h sys/stat.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_STRUCT_TM

dnl Checks for library functions.
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS(gettimeofday sigaction mkdir rmdir select strerror strstr statfs usleep)

AC_CONFIG_HEADERS([src/sysconfig.h])
AC_SUBST(HAVE_SDL)
AC_SUBST(GUI_OBJS)

dnl Generate Makefile.
AC_CONFIG_FILES([Makefile src/Makefile docs/Makefile])
AC_OUTPUT

dnl Print summary.
echo
echo "Configuration done. Now type \"make\"."
