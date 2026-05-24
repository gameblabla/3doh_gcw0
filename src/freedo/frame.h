// Frame.h - For frame extraction and filtering options.

#ifndef FRAME_3DO_HEADER
#define FRAME_3DO_HEADER

#ifdef __cplusplus
extern "C" {
#endif

void _frame_Init(void);

void Get_Frame_Bitmap(
	struct VDLFrame* sourceFrame,
	void* destinationBitmap,
	uint_fast32_t copyWidth,
	uint_fast32_t copyHeight);

void Get_Frame_Bitmap_Pitched(
	struct VDLFrame* sourceFrame,
	void* destinationBitmap,
	uint_fast32_t copyWidth,
	uint_fast32_t copyHeight,
	uint_fast32_t destinationPitchPixels);

#ifdef __cplusplus
}
#endif

#endif
