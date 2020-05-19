#include <stdint.h>
#include <stdbool.h>

#include "freedocore.h"
#include "frame.h"

static uint8_t FIXED_CLUTR[32];

void _frame_Init(void)
{
	uint_fast32_t j;
	for (j = 0; j < 32; j++) {
		FIXED_CLUTR[j] = (uint8_t)(((j & 0x1f) << 3) | ((j >> 2) & 7));
	}
}

void Get_Frame_Bitmap(struct VDLFrame* sourceFrame, void* destinationBitmap, uint_fast32_t copyWidth, uint_fast32_t copyHeight)
{
	uint_fast32_t i, pix;
	int16_t *destPtr = (int16_t*)destinationBitmap;
	for (i = 0; i < copyHeight; i++)
	{
		struct VDLLine* linePtr = (struct VDLLine*)&sourceFrame->lines[i];
		int16_t *srcPtr = (int16_t*)linePtr;
		bool allowFixedClut = (linePtr->xOUTCONTROLL & 0x2000000) > 0;
		for (pix = 0; pix < copyWidth; pix++)
		{
			int16_t bPart = 0;
			int16_t gPart = 0;
			int16_t rPart = 0;
			if (*srcPtr == 0)
			{
				bPart = (int16_t)(linePtr->xBACKGROUND & 0x1F);
				gPart = (int16_t)((linePtr->xBACKGROUND >> 5) & 0x1F);
				rPart = (int16_t)((linePtr->xBACKGROUND >> 10) & 0x1F);
			}
			else if (allowFixedClut && (*srcPtr & 0x8000) > 0)
			{
				bPart = (int16_t)FIXED_CLUTR[(*srcPtr) & 0x1F];
				gPart = (int16_t)FIXED_CLUTR[((*srcPtr) >> 5) & 0x1F];
				rPart = (int16_t)FIXED_CLUTR[(*srcPtr) >> 10 & 0x1F];
			}
			else
			{
				bPart = (int16_t)(linePtr->xCLUTB[(*srcPtr) & 0x1F]);
				gPart = (int16_t)linePtr->xCLUTG[((*srcPtr) >> 5) & 0x1F];
				rPart = (int16_t)linePtr->xCLUTR[(*srcPtr) >> 10 & 0x1F];
			}
			*destPtr++=(int16_t)(((rPart << 0x8)&0xF800) | (((gPart << 0x3))&0x7E0) | (bPart >>0x3));
			srcPtr++;
		}
	}
}
