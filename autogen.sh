#! /bin/sh
# Run this to generate all the initial makefiles, etc.

DIE=0

PROG=Frodo

# Check how echo works in this /bin/sh
case `echo -n` in
-n) _echo_n=   _echo_c='\c';;
*)  _echo_n=-n _echo_c=;;
esac

(autoreconf --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autoconf installed to compile $PROG."
        echo "Download the appropriate package for your distribution,"
        echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
        DIE=1
}

if test "$DIE" -eq 1; then
        exit 1
fi

(echo $_echo_n " + Running autoreconf: $_echo_c"; \
    autoreconf --install; \
 echo "done.") && \

rm -f config.cache

if [ x"$NO_CONFIGURE" = "x" ]; then
    echo " + Running 'configure $@':"
    if [ -z "$*" ]; then
	echo "   ** If you wish to pass arguments to ./configure, please"
        echo "   ** specify them on the command line."
    fi
    ./configure "$@"
fi
