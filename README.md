# Frodo V4
Frodo is a free, portable Commodore 64 emulator that runs on a variety
of platforms, with a focus on the exact reproduction of special graphical
effects possible on the C64.

Frodo comes in two flavours: The "normal" Frodo with a line-based
emulation, and the single-cycle emulation "Frodo SC" that is slower
but far more compatible.

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
