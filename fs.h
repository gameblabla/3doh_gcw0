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

#ifndef FS_H_
#define FS_H_


#ifdef DREAMCAST
	#include "fatfs/fs_fat.h"
	#include <dc/sd.h>
	#include <kos/blockdev.h>
	#include <ext2/fs_ext2.h>
#else

#endif


/*init and close functions*/
int fsInit();
int fsClose();

/*cdrom related functions*/
int fsOpenIso(char *path);
int fsReadBlock(void *buffer,int sector);
int fsCloseIso();
unsigned int fsReadDiscSize();

/*bios related functions*/
void fsReadBios(char *biosFile, void *prom);

#endif
