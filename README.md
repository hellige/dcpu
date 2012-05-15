DCPU-16 Playground
==================

This is an emulator, assembler, and an implementation of the Forth programming
language for Notch's DCPU-16. See http://0x10c.com/doc/dcpu-16.txt for details,
although that spec is now unofficially superseded. Newer specs are scattered
about:

  * DCPU-16 v1.7: http://pastebin.com/raw.php?i=Q4JvQvnM
  * NE_LEM1802: http://0x10c.com/highnerd/rc_1/lem1802.txt
  * Keyboard: http://dcpu.com/highnerd/rc_1/keyboard.txt
  * Clock: http://dcpu.com/highnerd/rc_1/clock.txt

There are also a couple of useful scripts for swapping endianness of binary
images and dumping to hex in a format supported by some other emulators.


Quick Start
-----------

To build, just run:

    make

To run the goforth interpreter image:

    ./goforth

Other images can be run with:

    ./dcpu blah.img

For options, just try:

    ./dcpu


The Emulator
------------

The emulator is written in C and supports both curses and SDL (if available at
compile-time) for video display. It's well tested and has a number of nice
options. It builds on both Mac OS and Linux with gcc. I assume it may also work
on Windows with either Cygwin or MinGW, but I haven't tried it myself and don't
intend to.

The emulator supports the 'standard' LEM-1802 display, keyboard, and clock.
I'm not sure what other emulators are doing, but this one will refuse to
overflow the interrupt queue: new interrupts will be dropped and the emulator
will break to the debugger on attempted overflow. This seems friendlier than
'catching fire'.

To enable the SDL graphics, pass the -g command-line option. The SDL graphics
support the entire LEM-1802 spec including custom font and color palette. When
using the SDL graphics window, keyboard input is still read from the terminal,
so be sure to keep your keyboard focus in the terminal window. The use of SDL
introduces a few quirks on Mac OS, such as insisting on opening a new
"application" even if the SDL graphics window is disabled. This is unfortunate,
but apparently unavoidable. It's a minor irritation in any case.

I think the curses display support is about as close to the current consensus
specs as is achievable with ncurses. Obviously bitmapped graphics aren't
supported, so the LEM-1802 support doesn't support MEM_MAP_FONT or
MEM_MAP_PALETTE. (They generate an emulator warning, but are otherwise silently
ignored.) It may be possible to provided limited palette support, at least on
fancier terminals, but it's not a high priority unless someone asks for it.
It's also difficult or impossible to fully support the documented key scan
codes in curses, since for example it's impossible to detect presses of
shift/control in isolation.

For best colors, run in a terminal with good color support. You may have to
fiddle around to get curses to realize your colors are good. For example, 
try setting

    TERM=xterm-256color

if you're not having any luck. You can use

    ./dcpu colortest.img

to test things out. Apparently ncurses 5 doesn't really support 256 distinct
color pairs (well, it supports 256, but the first pair is reserved). So if
you have ncurses 5 (most Linux distros, it seems), white-on-white will probably
appear as your default terminal colors instead.


goforth
-------

> 'i figured i owned just dark skies, and that darkness fit me best.  
>  go, folks, go forth. trust your brain! trust your body!'

The real fun starts in goforth.dasm and goforth.ft. goforth is an
implementation of the Forth programming language for DCPU-16. There's still
plenty of work to be done, but it's already quite usable. For example, it
contains a built-in assembler, decompiler/disassembler and other amenities.
(In fact, you might take a look at the files asm.ft and disasm.ft for examples
of some non-trivial code written in goforth.)

The bootstrapped goforth image should run on any DCPU-16 1.7 emulator with
a compatible display/keyboard. (It'll run on others, too, but won't do much,
although custom images would be quite easy to create...)

goforth has been tested on several emulators. I am most interested in hearing
whether it works on others, but I haven't seen a good list of tools which have
been updated to the 1.7 specs. As far as I know, there is not yet any official
1.7-compatible emulator from Notch, although the old (DCPU-16 1.1) version of
goforth worked without modification in the old version of Notch's emulator.

  * http://0x10co.de/hiwhx

    Post-1.7, the keyboard handling is better specified and things look
    very good. There are a couple of fishy things with the keyboard, one
    of which at least is surely an 0x10co.de bug.

  * http://fasm.elasticbeanstalk.com/?proj=rs5s77

    (The link is to a slightly out-of-date goforth image.) This looks very
    good. You won't see a cursor, because it seems to interpret the blink bit
    as "do not show", and it seems slower than it should be, but apart from
    that, everything looks good.

  * deNULL's web emulator: http://dcpu.ru

    Here, too, things look pretty good, but signed math is broken. It thinks
    -50/5, 50/-5 and 50/5 are all equal to 50 under DVI. DIV does know that
    50/5 is 10.

In order to bootstrap the image, I've augmented the DCPU instruction set with 
three new instructions (all in non-basic format):

    img: dump ram to the file core.img, from address 0x0000 to (but not
         including) the address in the 'a' argument. if 'a' is 0, dump the
         entire heap (0x0000 to 0x10000)
    die: exit the emulator
    dbg: enter the emulator debugger

The latter two instructions are only used in special cases and are easily 
avoided. img is needed only to bootstrap the forth image.

**ONCE AGAIN, THE BOOTSTRAPPED IMAGE WILL RUN ON ANY EMULATOR.**


The Assembler
-------------

The assembler (masm) is written in Python and has a number of idiosyncrasies.
The syntax has diverged from the consensus DCPU-16 assembler syntax in some
rather pointless ways, and it's quite limited. It works for my purposes, but
I don't recommend that you use it. For a more full-featured and standard
assembler, I recommend Jon Povey's das: https://github.com/jonpovey/das.


Credits
-------

The built-in disassembler includes code from [Brian Swetland's DCPU-16
emulator][1], see the file disassembler.c for license and copyright info.

Thanks to Corbin Simpson for various improvements to goforth.

Thanks to Juan Felipe Garcia Catalan for contributing the goforth assembler
(see asm.ft).

[1]: https://github.com/swetland/dcpu16
