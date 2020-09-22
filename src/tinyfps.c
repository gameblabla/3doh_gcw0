#include <stdio.h>

#include "tinyfps.h"
#include "video.h"

unsigned short miniDecimalFonts[FPS_FONT_NUM_PIXELS];

void initFpsFonts()
{
	int i, j, n, x, y;
	unsigned char miniDecimalPixels[FPS_FONT_NUM_PIXELS];

	int k = 0;
	for (i = 0; i<FPS_FONT_NUM_PIXELS / 8; i++)
	{
		unsigned char  d = miniDecimalData[i];
		for (j = 0; j<8; j++)
		{
			unsigned char c = (d & 0x80) >> 7;
			miniDecimalPixels[k++] = c;
			d <<= 1;
		}
	}

	i = 0;
	for (n = 0; n<FPS_FONTS_NUM; n++)
	{
		for (y = 0; y<FPS_FONT_HEIGHT; y++)
		{
			for (x = 0; x<FPS_FONT_WIDTH; x++)
			{
				miniDecimalFonts[i++] = miniDecimalPixels[n*FPS_FONT_WIDTH + x + y*FPS_FONT_WIDTH*FPS_FONTS_NUM] * 0xFFFF;
			}
		}
	}
}

void drawFont(int decimal, int posX, int posY, unsigned short *vram)
{
	int x, y;
	unsigned short *fontData = (unsigned short*)&miniDecimalFonts[decimal * FPS_FONT_WIDTH * FPS_FONT_HEIGHT];
	vram += posY * SCREEN_WIDTH + posX;

	for (y = 0; y<FPS_FONT_HEIGHT; y++)
	{
		for (x = 0; x<FPS_FONT_WIDTH; x++)
		{
			*vram++ = *fontData++;
		}
		vram += -FPS_FONT_WIDTH + SCREEN_WIDTH;
	}
}

void drawDecimal(unsigned int number, int posX, int posY, unsigned short *vram)
{
	char buffer[8];
	sprintf(buffer, "%d", number);
	int i = 0;
	while (i < 8 && buffer[i] != 0)
	{
		drawFont(buffer[i] - 48, posX + i * FPS_FONT_WIDTH, posY, vram);
		i++;
	}
}
