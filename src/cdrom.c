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

static void fsDetectCDFormat(const char *path, cueFile *cue_file)
{
   CD_format cd_format;
   if (cue_file)
   {
      cd_format = cue_file->cd_format;
      //printf("[4DO]: File format from cue file resolved to %s\n", cue_get_cd_format_name(cd_format));
   }
   else
   {
      size_t size = 0;
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
   cueFile *cue_file = cue_get(path);
   fsDetectCDFormat(path, cue_file);

   const char *cd_image_path = cue_is_cue_path(path) ? cue_file->cd_image : path;
   fcdrom = fopen(cd_image_path, "rb");

   free(cue_file);

   if(!fcdrom)
      return 0;

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

unsigned int fsReadDiscSize()
{
	unsigned int size;
	/*char sectorZero[2048];*/
	unsigned int temp;
	char ssize[4];
	
	rewind(fcdrom);
	fseek(fcdrom, 80 + cd_sector_offset, SEEK_SET);
	fread(ssize, 1, 4, fcdrom);
	rewind(fcdrom);

	memcpy(&temp, ssize, 4);
	size = (temp & 0x000000FFU) << 24 | (temp & 0x0000FF00U) << 8 |
	       (temp & 0x00FF0000U) >> 8 | (temp & 0xFF000000U) >> 24;
	return size;
}
