# Frodo V4

Frodo is a free, portable Commodore 64 emulator that runs on a variety
of platforms, with a focus on the exact reproduction of special graphical
effects possible on the C64.

Frodo comes in two flavours: The regular "Frodo" which uses a cycle-exact
emulation, and the simplified "Frodo Lite" which is less compatible but runs
better on slower machines.

## Installation on Unix-like Systems

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

## Beginner's Guide

### Configuring Important Settings

When you start Frodo you are presented with a settings window. Most settings
have reasonable defaults, so there should be very little you have to configure:

* Under “Input” you can assign game controllers to the two C64 joystick ports
  (these have to be plugged in before starting Frodo).

C64 joysticks are typically digital devices with eight directions and just one
button. Frodo lets you use the D-pad and left stick of the controller for the
direction, and the “A” and “B” (or Cross and Circle) face buttons as the
joystick button.

Note that most C64 games use a joystick in Port 2 by default, even if they are
single-player. If you don't get a reaction from controller input, try the “Swap
Joysticks” option to exchange the port assignments.

If you don't have (or want to use) a game controller, Frodo also offers a
joystick simulation on the numeric keypad. The “5” and “0” keys act as the
joystick button while the other number keys control the direction. Press the
Num Lock key to switch between joystick Port 1 and 2.

* Under “Video/Sound” you can choose between windowed and fullscreen display,
  and set the initial window size.

You can also use the Enter key on the numeric keypad to change the display mode
on the fly while the emulator is running.

* The “Drives” tab is where you make software available for the emulator to
  load and run.

Frodo only supports software written for the Commodore 1541 disk drive. This
typically comes in the form of .D64 disk image files *(no, they are not called
“ROMs”!)*. Press the file selection button next to the “Drive 8” label to choose
a .D64 file to mount in the simulated drive with drive number 8.

*(For advanced users: If you deselect the “Enable Full 1541 Emulation”, Frodo
will let you use up to four simulated disk drives and switch to a faster and
more flexible “virtual” 1541 drive emulation which also lets you access entire
directories of the host computer from the C64. This works well with simple,
single-file games, but for most uses the “full” 1541 emulation is recommended.)*

### Starting the Emulator and Running Software

Click on the “Start” button to close the settings window and start the emulator.
You will now see the beautiful, blue startup screen of the Commodore 64 with the
iconic “READY.” prompt.

Important key combinations inside the emulator window:

* Pressing **F10** pauses the emulation and takes you back to the settings
  window where you can, for example, swap .D64 files.
* Pressing **F12** resets the C64.
* Hold down the **right trigger** on the game controller or the “+” (**Plus**)
  key on the numeric keypad to fast-forward at 4x speed (useful for example,
  while software is loading from the disk drive).
* Hold down the **left trigger** on the game controller or the “−” (**Minus**)
  key on the numeric keypad to rewind by up to 30 seconds (useful when you
  messed up in one of those “Commodore-hard” retro games...).
* Pressing the **Enter** key on the numeric keypad switches between fullscreen
  and windowed display.

The C64 is a Personal Computer, not a games console, and it doesn't autostart
software from the disk drive. Instead it comes with a built-in text adventure
game in the form of an interpreter for (a simple implementation of) the BASIC
programming language.

*(If you're feeling adventurous, take a look at the
[Commodore 64 User's Guide](https://archive.org/details/commodore-64-user-guide)
and work through the included BASIC programming exercises. Maybe you'll
eventually become a famous software developer and author of a C64 emulator!)*

Getting software to load and run from the simulated disk drive also involves
typing in some BASIC commands. Unfortunately, there is no general standard for
which commands to use here, but 90% of the time entering this will do the trick:

    LOAD"*",8,1

Frodo simulates the original layout of the Commodore 64 keyboard, so the "
(quote) character is on Shift-2, and the * (star) is most likely to be on the
“]” (right bracket) key, or the equivalent key in the same position on national
keyboards. Don't worry, you'll get used to that.

If you get a “LOADING” message from entering the above command, things are
looking good. If you instead get an error message (most likely “FILE NOT FOUND”,
unless you mistyped the command or forgot to mount a .D64 file), then check any
documentation that came with the software for the command that is used to run it
(this was often printed on the disk label). It may be something different like
LOAD"BOOT",8,1. If this still doesn't give you results, then try:

    LOAD"$",8

Wait for that to finish and get back to the “READY.” prompt, then type:

    LIST

This will give you a listing of the directory contents of the .D64 file (if it
scrolls off the screen too quickly you can hold down the Tab or Ctrl key to
slow it down). Sometimes there are instructions for how to load the software
embedded in the directory listing. Otherwise, note down the name of the first
file with a “PRG” designation next to it and try entering:

    LOAD"<filename>",8,1

where *“\<filename\>”* refers to the file name you noted down.

If you successfully got a “LOADING” message, there's basically two things that
can happen next:

1. The game takes over control and starts. This should occur within a few
   seconds and is usually indicated by a loading screen, a simple title screen,
   color bars, or at least a screen color change. In this case, just sit back
   and wait for things to unfold. If you are being asked to swap disks, press
   F10 to bring up the settings window again and mount the appropriate .D64
   file (the “Next Disk” button also usually produces the desired result).
2. If the “LOADING” message stays on the screen for more than 10 seconds, you'll
   probably have to start the game manually after the loading process has
   finished. Because the Commodore 1541 is one of the slowest storage devices
   known to mankind, this may take up to two minutes. If you're too impatient
   to enjoy the full, authentic C64 loading experience, hold down the right
   trigger on your game controller or the “+” (Plus) key on the numeric keypad
   to speed things up a bit. *(For advanced users: Frodo does support
   software-based “fast loaders”, so if you know what you are doing you can
   try running one of those before loading your game.)*

If the game did not autostart and you're back at the “READY.” prompt after
loading, enter this to start the game:

    RUN

You should now be good to go. Some games (especially those which had their
original copy protection removed by some friendly people caring about game
preservation) may show weird characters or flickering color bars for a few
seconds before things start. This is normal. Some games also  present an intro
or title screen that needs to be skipped by pressing the Space bar or a joystick
button. If a game asks you to press “RUN/STOP”, hit the Escape key. The
“Commodore” or “C=” key is mapped to the Alt keys.

Now have fun!

To quit Frodo, either close the emulation window, press **Alt-F4**, or press
**F10** to bring up the settings window and click on “Quit”.

For more detailed information, see the included
[HTML documentation](docs/index.html).
