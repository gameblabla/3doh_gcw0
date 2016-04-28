/*
    This file is part of 3d'oh, a multiplatform 3do emulator written by Gabriel Ernesto Cabral.

    3d'oh is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    3d'oh is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with 3d'oh.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "sound.h"
#include "video.h"
#include "input.h"
#include "config.h"
#include "frame.h"
#include "fs.h"

#ifndef DREAMCAST
#define uint32 unsigned int
#else

#endif

typedef struct
{
		char recordType;               // 1 byte
		char syncBytes[5];       // 5 bytes
		char recordVersion;            // 1 byte
		char flags;                    // 1 byte
		char comment[32];        // 32 bytes
		char label[32];          // 32 bytes
		uint32 id;                     // 4 bytes
		uint32 blockSize;              // 4 bytes
		uint32 blockCount;             // 4 bytes
		uint32 rootDirId;              // 4 bytes
		uint32 rootDirBlocks;          // 4 bytes
		uint32 rootDirBlockSize;       // 4 bytes
		uint32 lastRootDirCopy;        // 4 bytes
		uint32 rootDirCopies[8]; // 32 bytes
}NvRamStr;


void	_3do_Init();

