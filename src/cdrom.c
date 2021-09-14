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
#include "cuefile.h"
FILE *fcdrom;

static int cd_sector_size;
static int cd_sector_offset;

static void fsDetectCDFormat(const char *path)
{
   CD_format cd_format;
   cueFile *cue_file = cue_get(path);
   if (cue_file)
   {
      cd_format = cue_file->cd_format;
      //printf("[4DO]: File format from cue file resolved to %s\n", cue_get_cd_format_name(cd_format));
      free(cue_file);
   }
   else
   {
      int size = 0;
      FILE *fp = fopen(path, "r");
      if (fp) {
         fseek(fp, 0L, SEEK_END);
	     size = ftell(fp);
	     fclose(fp);
      }
      cd_format = MODE1_2048;
      if (size % SECTOR_SIZE_2352 == 0)
      {
    	 cd_format = MODE1_2352;
      }
   }

   switch (cd_format)
   {
	   //case MODE1_2048:
	   default:
		  cd_sector_size = SECTOR_SIZE_2048;
		  cd_sector_offset = SECTOR_OFFSET_MODE1_2048;
		break;
	   case MODE1_2352:
		  cd_sector_size = SECTOR_SIZE_2352;
		  cd_sector_offset = SECTOR_OFFSET_MODE1_2352;
		break;
	   case MODE2_2352:
		  cd_sector_size = SECTOR_SIZE_2352;
		  cd_sector_offset = SECTOR_OFFSET_MODE2_2352;
		break;
   }
}

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
	uint_fast8_t i;
	char cue_path_base[256];
	const char *exts[] = {".cue", ".CUE"};
	const char *exts_bin[] = {".bin", ".BIN"};
	
	/* Detect if input file is .cue or .CUE and if so, change it to bin */
	for(i=0;i<2;i++)
	{
		if (strcmp(strrchr(path, '.'), exts[i]) == 0)
		{
			strncpy(cue_path_base, path, 256);
			char *last_dot = strrchr(cue_path_base, '.');
			if (last_dot == NULL) return 0;
			*(last_dot) = '\0';
			strcpy(path, cue_path_base);
			strcat(path, exts_bin[i]);
			break;
		}
	}

	fcdrom = fopen(path, "rb");
	if (!fcdrom)
	{
		printf("ERROR: can't load game file, exiting\n");
		return 0;
	}
	fsDetectCDFormat(path);
	return 1;
}

int fsCloseIso()
{
	if (fcdrom)
	{
		fclose(fcdrom);
	}
	return 1;
}


int fsReadBlock(void *buffer, int sector)
{
	fseek(fcdrom, (cd_sector_size * sector) + cd_sector_offset, SEEK_SET);
	fread(buffer, 1, SECTOR_SIZE_2048, fcdrom);
	rewind(fcdrom);
	return 1;
}

char *fsReadSize()
{
	char *buffer = (char*)malloc(sizeof(char) * 4);

	rewind(fcdrom);
	fseek(fcdrom, 80 + cd_sector_offset, SEEK_SET);
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
