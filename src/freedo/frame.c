#include <stdint.h>
#include <stdbool.h>

#include "freedocore.h"
#include "frame.h"

#define VDL_CLUTBYPASSEN 0x02000000U
#define VDL_VINTEN       0x00000004U
#define VDL_HINTEN       0x00000008U
#define VDL_BLSB_BLUE    0x00000020U

static uint8_t FIXED_CLUT[32];

static inline uint8_t expand5(uint8_t value)
{
	return (uint8_t)((value << 3) | (value >> 2));
}

void _frame_Init(void)
{
	uint_fast32_t j;
	for (j = 0; j < 32; j++)
		FIXED_CLUT[j] = expand5((uint8_t)j);
}

#if BPP_TYPE == 32
static inline void decodeVDLPixel32(const struct VDLLine *linePtr, uint16_t pixel,
                                    uint8_t *r, uint8_t *g, uint8_t *b)
{
	if ((pixel & 0x7fffU) == 0) {
		uint32_t bg = linePtr->xBACKGROUND;
		*r = (uint8_t)((bg >> 16) & 0xffU);
		*g = (uint8_t)((bg >> 8) & 0xffU);
		*b = (uint8_t)(bg & 0xffU);
		return;
	}

	uint8_t ri = (uint8_t)((pixel >> 10) & 0x1fU);
	uint8_t gi = (uint8_t)((pixel >> 5) & 0x1fU);
	uint8_t bi = (uint8_t)(pixel & 0x1fU);
	bool fixed = ((linePtr->xOUTCONTROLL & VDL_CLUTBYPASSEN) != 0) && ((pixel & 0x8000U) != 0);

	if (fixed) {
		*r = FIXED_CLUT[ri];
		*g = FIXED_CLUT[gi];
		*b = FIXED_CLUT[bi];
	} else {
		/* The 3DO VDLP expands 15-bit bitmap components through three
		 * independent 32-entry CLUTs.  Each CLUT entry is 8-bit, so this
		 * path must remain RGB888 all the way to the browser framebuffer.
		 */
		*r = linePtr->xCLUTR[ri];
		*g = linePtr->xCLUTG[gi];
		*b = linePtr->xCLUTB[bi];

		/* Some 24-bit image/VDL paths use pixel bit 15 as the least
		 * significant blue bit.  The previous code only treated bit 15 as a
		 * fixed-CLUT selector, which made examples such as slide_show_24bit
		 * and FMV-style content lose one blue bit or select the wrong path.
		 */
		if ((linePtr->xOUTCONTROLL & VDL_BLSB_BLUE) != 0)
			*b = (uint8_t)((*b & 0xfeU) | ((pixel >> 15) & 1U));
	}
}

static inline uint8_t avg8(uint8_t a, uint8_t b)
{
	return (uint8_t)(((uint16_t)a + (uint16_t)b + 1U) >> 1);
}

static inline bool lineUsesVDLInterpolation(const struct VDLLine *linePtr)
{
	return linePtr->xHasCurrentLine && ((linePtr->xOUTCONTROLL & (VDL_HINTEN | VDL_VINTEN)) != 0);
}

void Get_Frame_Bitmap_Pitched(struct VDLFrame* sourceFrame, void* destinationBitmap,
                              uint_fast32_t copyWidth, uint_fast32_t copyHeight,
                              uint_fast32_t destinationPitchPixels)
{
	uint_fast32_t i, pix;
	uint8_t *destPtr = (uint8_t*)destinationBitmap;
	uint_fast32_t rowAdvance;

	if (destinationPitchPixels < copyWidth)
		destinationPitchPixels = copyWidth;
	rowAdvance = (destinationPitchPixels - copyWidth) * 4U;

	for (i = 0; i < copyHeight; i++) {
		const struct VDLLine* linePtr = (const struct VDLLine*)&sourceFrame->lines[i];
		const uint16_t *srcPtr = linePtr->line;
		const uint16_t *curPtr = linePtr->currentLine;
		bool blendVDL = lineUsesVDLInterpolation(linePtr);

		for (pix = 0; pix < copyWidth; pix++) {
			uint8_t r, g, b;
			decodeVDLPixel32(linePtr, srcPtr[pix], &r, &g, &b);

			if (blendVDL && curPtr[pix] != srcPtr[pix]) {
				uint8_t cr, cg, cb;
				decodeVDLPixel32(linePtr, curPtr[pix], &cr, &cg, &cb);
				r = avg8(r, cr);
				g = avg8(g, cg);
				b = avg8(b, cb);
			}

			*destPtr++ = r;
			*destPtr++ = g;
			*destPtr++ = b;
			*destPtr++ = 255;
		}
		destPtr += rowAdvance;
	}
}

void Get_Frame_Bitmap(struct VDLFrame* sourceFrame, void* destinationBitmap,
                      uint_fast32_t copyWidth, uint_fast32_t copyHeight)
{
	Get_Frame_Bitmap_Pitched(sourceFrame, destinationBitmap, copyWidth, copyHeight, copyWidth);
}
#else
static inline uint16_t rgb888To565(uint8_t r, uint8_t g, uint8_t b)
{
	return (uint16_t)(((uint16_t)(r & 0xf8U) << 8) |
	                  ((uint16_t)(g & 0xfcU) << 3) |
	                  ((uint16_t)b >> 3));
}

static inline uint16_t decodeVDLPixel16(const struct VDLLine *linePtr, uint16_t pixel)
{
	uint8_t r, g, b;

	if ((pixel & 0x7fffU) == 0) {
		uint32_t bg = linePtr->xBACKGROUND;
		return rgb888To565((uint8_t)((bg >> 16) & 0xffU),
		                   (uint8_t)((bg >> 8) & 0xffU),
		                   (uint8_t)(bg & 0xffU));
	}

	if (((linePtr->xOUTCONTROLL & VDL_CLUTBYPASSEN) != 0) && ((pixel & 0x8000U) != 0)) {
		r = FIXED_CLUT[(pixel >> 10) & 0x1fU];
		g = FIXED_CLUT[(pixel >> 5) & 0x1fU];
		b = FIXED_CLUT[pixel & 0x1fU];
	} else {
		r = linePtr->xCLUTR[(pixel >> 10) & 0x1fU];
		g = linePtr->xCLUTG[(pixel >> 5) & 0x1fU];
		b = linePtr->xCLUTB[pixel & 0x1fU];
	}

	return rgb888To565(r, g, b);
}

void Get_Frame_Bitmap_Pitched(struct VDLFrame* sourceFrame, void* destinationBitmap,
                              uint_fast32_t copyWidth, uint_fast32_t copyHeight,
                              uint_fast32_t destinationPitchPixels)
{
	uint_fast32_t i, pix;
	uint16_t *destPtr = (uint16_t*)destinationBitmap;
	uint_fast32_t rowAdvance;

	if (destinationPitchPixels < copyWidth)
		destinationPitchPixels = copyWidth;
	rowAdvance = destinationPitchPixels - copyWidth;
	for (i = 0; i < copyHeight; i++) {
		const struct VDLLine* linePtr = (const struct VDLLine*)&sourceFrame->lines[i];
		const uint16_t *srcPtr = linePtr->line;
		for (pix = 0; pix < copyWidth; pix++)
			*destPtr++ = decodeVDLPixel16(linePtr, srcPtr[pix]);
		destPtr += rowAdvance;
	}
}

void Get_Frame_Bitmap(struct VDLFrame* sourceFrame, void* destinationBitmap,
                      uint_fast32_t copyWidth, uint_fast32_t copyHeight)
{
	Get_Frame_Bitmap_Pitched(sourceFrame, destinationBitmap, copyWidth, copyHeight, copyWidth);
}
#endif
