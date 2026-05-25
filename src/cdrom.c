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


static void safe_copy_path(char *dst, const char *src, size_t dst_size)
{
   if (!dst || !dst_size)
      return;
   if (!src)
      src = "";
   strncpy(dst, src, dst_size - 1);
   dst[dst_size - 1] = '\0';
}

static const char *path_basename_const(const char *path)
{
   const char *base;
   const char *slash;
   const char *backslash;

   if (!path)
      return "";

   base = path;
   slash = strrchr(path, '/');
   backslash = strrchr(path, '\\');
   if (slash && slash + 1 > base)
      base = slash + 1;
   if (backslash && backslash + 1 > base)
      base = backslash + 1;
   return base;
}

static char *fsFindCueSiblingImage(const char *cue_path, const char *referenced_path)
{
   enum { PATH_MAX_LOCAL = 2048 };
   static const char *fallback_exts[] = { ".iso", ".ISO", ".bin", ".BIN", ".img", ".IMG" };
   char dir[PATH_MAX_LOCAL];
   char cue_base[PATH_MAX_LOCAL];
   char ref_ext[64];
   const char *cue_name;
   const char *dot;
   char *slash;
   char *backslash;
   size_t dir_len;
   size_t base_len;
   int i;

   if (!cue_path)
      return NULL;

   safe_copy_path(dir, cue_path, sizeof(dir));
   slash = strrchr(dir, '/');
   backslash = strrchr(dir, '\\');
   if (backslash && (!slash || backslash > slash))
      slash = backslash;
   if (slash)
      slash[1] = '\0';
   else
      dir[0] = '\0';

   cue_name = path_basename_const(cue_path);
   safe_copy_path(cue_base, cue_name, sizeof(cue_base));
   dot = strrchr(cue_base, '.');
   if (dot)
      cue_base[dot - cue_base] = '\0';

   ref_ext[0] = '\0';
   if (referenced_path) {
      const char *ref_name = path_basename_const(referenced_path);
      dot = strrchr(ref_name, '.');
      if (dot && strlen(dot) < sizeof(ref_ext))
         safe_copy_path(ref_ext, dot, sizeof(ref_ext));
   }

   dir_len = strlen(dir);
   base_len = strlen(cue_base);

   for (i = -1; i < (int)(sizeof(fallback_exts) / sizeof(fallback_exts[0])); ++i) {
      const char *ext = (i < 0) ? ref_ext : fallback_exts[i];
      char candidate[PATH_MAX_LOCAL];
      FILE *fp;
      if (!ext[0])
         continue;
      if (dir_len + base_len + strlen(ext) + 1 > sizeof(candidate))
         continue;
      snprintf(candidate, sizeof(candidate), "%s%s%s", dir, cue_base, ext);
      fp = fopen(candidate, "rb");
      if (fp) {
         char *out;
         fclose(fp);
         out = (char *)malloc(strlen(candidate) + 1);
         if (out)
            strcpy(out, candidate);
         return out;
      }
   }

   return NULL;
}

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
   int path_is_cue = cue_is_cue_path(path);
   const char *cd_image_path;
   char *fallback_cd_image_path = NULL;

   if (path_is_cue && (!cue_file || !cue_file->cd_image))
   {
      cue_free(cue_file);
      return 0;
   }

   fsDetectCDFormat(path, cue_file);

   cd_image_path = path_is_cue ? cue_file->cd_image : path;
   fcdrom = fopen(cd_image_path, "rb");

   if (!fcdrom && path_is_cue)
   {
      fallback_cd_image_path = fsFindCueSiblingImage(path, cd_image_path);
      if (fallback_cd_image_path)
         fcdrom = fopen(fallback_cd_image_path, "rb");
   }

   free(fallback_cd_image_path);
   cue_free(cue_file);

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
