ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src docs

dist_doc_DATA = CHANGES COPYING

desktopdir = $(datadir)/applications
desktop_DATA = Frodo.desktop FrodoLite.desktop
icondir = $(datadir)/icons/hicolor/128x128/apps
icon_DATA = Frodo.png
mimedir = $(datadir)/mime/packages
mime_DATA = vnd.cbm-Frodo.xml

EXTRA_DIST = Frodo.spec autogen.sh $(desktop_DATA) $(icon_DATA) $(mime_DATA)
