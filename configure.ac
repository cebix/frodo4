dnl Information on the package.
AC_PREREQ([2.69])
AC_INIT([Frodo],[4.5], [], [Frodo])
AC_CONFIG_SRCDIR([src/main.cpp])
AM_INIT_AUTOMAKE([1.12 foreign])
AC_CANONICAL_HOST

dnl Checks for programs.
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX(20, noext, mandatory)
PKG_PROG_PKG_CONFIG

dnl Checks for libraries.
AM_PATH_SDL2(2.30.0, AC_DEFINE(HAVE_SDL, 1, [SDL support is enabled]), AC_MSG_ERROR([*** Required SDL version not found]))
CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= 3.24], HAVE_GTK=yes, HAVE_GTK=no)
if [[ $HAVE_GTK = yes ]]; then
  AC_DEFINE(HAVE_GTK, 1, [GTK preferences GUI is enabled])
  CPPFLAGS="$CPPFLAGS $GTK_CFLAGS"
  LIBS="$LIBS $GTK_LIBS"
  case "${host_os}" in
    mingw*)
      ;;
    *)
      LDFLAGS="$LDFLAGS -rdynamic"
      ;;
  esac
fi

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T

AC_CONFIG_HEADERS([src/sysconfig.h])
AC_SUBST(HAVE_SDL)

dnl Generate Makefile.
AC_CONFIG_FILES([Makefile src/Makefile docs/Makefile])
AC_OUTPUT

dnl Print summary.
echo
echo "Configuration done. Now type \"make\"."
