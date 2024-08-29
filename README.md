# Frodo V4
Frodo is a free, portable Commodore 64 emulator that runs on a variety
of platforms, with a focus on the exact reproduction of special graphical
effects possible on the C64.

Frodo comes in two flavours: "Frodo SC" which is a cycle-exact emulation,
and the "regular", line-based Frodo which is less compatible but runs better
on slower machines.

## Installation on Unix-like systems
Prerequisites:
* A C++20 compiler
* SDL (Simple DirectMedia Layer) 2.x
* GTK 3

Frodo can be compiled and installed in the usual way:
```
$ ./autogen.sh
$ make
$ sudo make install
```
Installation is only necessary if you intend to use the GTK-based
preferences GUI.
