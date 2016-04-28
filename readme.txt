3D'oh! - A libre 3do emulator 
Author: Guaripolo - gabriel@arcadenea.com.ar
============================================



1. What's this: 
===============
A libre (GPL) 3do emulator for GNU/Linux, built around SDL for audio, video and input. It also includes an optional binding for OpenGL video output, that enables a nice video scaling/stretch.
3D'oh! uses FREEDO as the core emulator. It's a 3do emulation library made by Alexander Troosh, Maxim Grishin, Allen Wright, John Sammons and Felix Lazarev.



2. Usage
========
in command line run: 3doh -b biosfile -i isofile


3. Building from source
=======================
To build from source, in command line just type 'make'. You will need the SDL libraries installed in your system. To enable OpenGL support, the -USEGL flag must be set in CFLAGS. You will need also the GL libraries.



3. History:
===========
12/01/2014: 
	- Fixed some input code, now you can play with 5 more friends :)
	- Fixed audio code, it was WRONG, now it's playing 44100 hz STEREO.
	- Initial Dreamcast Port. Not tested yet in the real hardware, but it should work.
	- Fixed some code in Freedo, resulting in lower RAM usage and a little speed improvement.

- First Release: 
	- Alpha release: unfinished work in progress, keyboard and joystick support. 



4. License: 
===========
3D'oh! it's released under the GPLv2 license.

