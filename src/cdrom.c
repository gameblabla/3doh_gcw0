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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
FILE *fcdrom;

int fsInit()
{
	return 1;
}

int fsClose()
{
	return 1;
}

void fsReadBios(char *biosFile, void *prom)
{
	FILE* bios1;
	long fsize;
	int readcount;

	bios1 = fopen(biosFile, "rb");

	fseek(bios1, 0, SEEK_END);
	fsize = ftell(bios1);
	rewind(bios1);

	readcount = fread(prom, 1, fsize, bios1);
	(void)readcount;
	fclose(bios1);
}

int fsOpenIso(char *path)
{
	fcdrom = fopen(path, "rb");
	if (!fcdrom) {
		printf("ERROR: can't load game file, exiting\n");
		return 0;
	}
	return 1;
}

int fsCloseIso()
{
	fclose(fcdrom);
	return 1;
}


int fsReadBlock(void *buffer, int sector)
{
	fseek(fcdrom, 2048 * sector, SEEK_SET);
	fread(buffer, 1, 2048, fcdrom);
	rewind(fcdrom);
	return 1;
}

char *fsReadSize()
{
	char *buffer = (char*)malloc(sizeof(char) * 4);

	rewind(fcdrom);
	fseek(fcdrom, 80, SEEK_SET);
	fread(buffer, 1, 4, fcdrom);
	rewind(fcdrom);
	return buffer;
}

unsigned int fsReadDiscSize()
{
	unsigned int size;
	/*char sectorZero[2048];*/
	unsigned int temp;
	char *ssize;

	ssize = fsReadSize();
	memcpy(&temp, ssize, 4);
	size = (temp & 0x000000FFU) << 24 | (temp & 0x0000FF00U) << 8 |
	       (temp & 0x00FF0000U) >> 8 | (temp & 0xFF000000U) >> 24;
	return size;
}
