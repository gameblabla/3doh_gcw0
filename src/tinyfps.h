#ifndef TINY_FPS_H
#define TINY_FPS_H

#define FPS_FONT_WIDTH 4
#define FPS_FONT_HEIGHT 5
#define FPS_FONTS_NUM 10
#define FPS_FONT_NUM_PIXELS (FPS_FONTS_NUM * FPS_FONT_WIDTH * FPS_FONT_HEIGHT)

/*
1110 0010 1110 1110 1010 1110 1110 1110 1110 1110
1010 0010 0010 0010 1010 1000 1000 0010 1010 1010
1010 0010 1110 0110 1110 1110 1110 0010 1110 1110
1010 0010 1000 0010 0010 0010 1010 0010 1010 0010
1110 0010 1110 1110 0010 1110 1110 0010 1110 1110
*/

static const unsigned char miniDecimalData[] = {	0xE2, 0xEE, 0xAE, 0xEE, 0xEE,
													0xA2, 0x22, 0xA8, 0x82, 0xAA,
													0xA2, 0xE6, 0xEE, 0xE2, 0xEE,
													0xA2, 0x82, 0x22, 0xA2, 0xA2,
													0xE2, 0xEE, 0x2E, 0xE2, 0xEE };



void initFpsFonts();
void drawDecimal(unsigned int number, int posX, int posY, unsigned short *vram);

#endif
