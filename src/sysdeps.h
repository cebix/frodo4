/*
 *  sysdeps.h - Try to include the right system headers and get other
 *              system-specific stuff right
 *
 *  Frodo Copyright (C) Christian Bauer
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

#include "sysconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include <cstdint>

#include <vector>
using std::vector;

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <direct.h>
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define LITTLE_ENDIAN_UNALIGNED 1
#endif

#ifdef __BEOS__
#include <support/SupportDefs.h>
#endif
