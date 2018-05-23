Description
===========

3D'oh! is a 3DO emulator originally written by Guaripolo (gabriel@arcadenea.com.ar).

However, he gave up working on it. I later took his emulator, completely switched it to C rather than C++, (for portability, size and speed reasons) and ported it to the GCW0 in the process.

Then i later let it gather dust... Dmitry however forked my code and finally fixed the audio code which was broken.

He also was working on a MIPS recompiler for it, which i never sadly worked on... So you're stuck with the interpreter code for now

which is somewhat decently fast on a reasonably fast device. (2 Ghz at least, it is not playable fullspeed at 1Ghz on an ARM or MIPS proc)

So yeah, it does not run fullspeed on the GCW0...

It uses FreeDO as its core. FreeDO was written by Alexander Troosh, Maxim Grishin, Allen Wright, John Sammons and Felix Lazarev.

Compiling
=========

On a Linux PC, just run make -f Makefile.rel for the Release build.

You'll get an executable called 3doh that you can then run from a terminal (or shell script).


Installation
============

3Doh requires a 3DO bios called bios.bin in $HOME/.3doh. ($HOME being your home directory on Linux/GCW0)

Legal
======

The BIOS file is copyrighted by the 3DO Corporation.

Even the 3DO Corporation is long gone, copyright generally lasts up until 90 years in the US and less so in other countries.

It is doubtful that the current copyright owners will or can sue redistribution of the bios file.

In fact, it is unknown who currently owns the copyright on it but 3DO never allowed retribution of the BIOS. (or even the devkit)

If you live in a country part of the WIPO, you cannot use it without dumping it yourself or at the very least owning the console.

You are solely responsible if anything goes wrong.

3DOh is licensed under the GPLv2.

Running
=======

./3doh pathtomyiso.iso

Supply the emulator with a game in ISO format and you're good to go.

On the GCW0, it is even more simple, just choose your ISO file with the file manager.

Apart from commercial titltes, tou can also run homebrew games or Icebreaker 2, which was allowed redistribution by its author.
