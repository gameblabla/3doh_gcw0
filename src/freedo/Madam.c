/*
   www.freedo.org
   The first and only working 3DO multiplayer emulator.

   The FreeDO licensed under modified GNU LGPL, with following notes:

 *   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
 *   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
        of the sources, or any other material published by FreeDO have to be accompanied with full credits.
 *   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
        or any other material published by FreeDO is strictly forbidden without owners approval.

   The above notes are taking precedence over GNU LGPL in conflicting situations.

   Project authors:

   Alexander Troosh
   Maxim Grishin
   Allen Wright
   John Sammons
   Felix Lazarev
 */

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "retro_inline.h"

#include "Madam.h"
#include "Clio.h"
#include "vdlp.h"
#include "arm.h"

#include "bitop.h"
struct BitReaderBig bitoper;

extern int sf;
extern int sdf;
extern int unknownflag11;
extern int fixmode;
extern int speedfixes;

/* === CCB control word flags === */
#define CCB_SKIP        0x80000000
#define CCB_LAST        0x40000000
#define CCB_NPABS       0x20000000
#define CCB_SPABS       0x10000000
#define CCB_PPABS       0x08000000
#define CCB_LDSIZE      0x04000000
#define CCB_LDPRS       0x02000000
#define CCB_LDPPMP      0x01000000
#define CCB_LDPLUT      0x00800000
#define CCB_CCBPRE      0x00400000
#define CCB_YOXY        0x00200000
#define CCB_ACSC        0x00100000
#define CCB_ALSC        0x00080000
#define CCB_ACW         0x00040000
#define CCB_ACCW        0x00020000
#define CCB_TWD         0x00010000
#define CCB_LCE         0x00008000
#define CCB_ACE         0x00004000
#define CCB_reserved13  0x00002000
#define CCB_MARIA       0x00001000
#define CCB_PXOR        0x00000800
#define CCB_USEAV       0x00000400
#define CCB_PACKED      0x00000200
#define CCB_POVER_MASK  0x00000180
#define CCB_PLUTPOS     0x00000040
#define CCB_BGND        0x00000020
#define CCB_NOBLK       0x00000010
#define CCB_PLUTA_MASK  0x0000000F

#define CCB_POVER_SHIFT  7
#define CCB_PLUTA_SHIFT  0

#define PMODE_PDC   ((0x00000000) << CCB_POVER_SHIFT) /* Normal */
#define PMODE_ZERO  ((0x00000002) << CCB_POVER_SHIFT)
#define PMODE_ONE   ((0x00000003) << CCB_POVER_SHIFT)

//  === CCBCTL0 flags ===
#define B15POS_MASK   0xC0000000
#define B0POS_MASK    0x30000000
#define SWAPHV        0x08000000
#define ASCALL        0x04000000
#define _CCBCTL0_u25  0x02000000
#define CFBDSUB       0x01000000
#define CFBDLSB_MASK  0x00C00000
#define PDCLSB_MASK   0x00300000

#define B15POS_SHIFT  30
#define B0POS_SHIFT   28
#define CFBD_SHIFT    22
#define PDCLSB_SHIFT  20

//  B15POS_MASK definitions
#define B15POS_0    0x00000000
#define B15POS_1    0x40000000
#define B15POS_PDC  0xC0000000

//  B0POS_MASK definitions
#define B0POS_0     0x00000000
#define B0POS_1     0x10000000
#define B0POS_PPMP  0x20000000
#define B0POS_PDC   0x30000000

/*
   //  CFBDLSB_MASK definitions
 #define CFBDLSB_0      0x00000000
 #define CFBDLSB_CFBD0  0x00400000
 #define CFBDLSB_CFBD4  0x00800000
 #define CFBDLSB_CFBD5  0x00C00000

   //  PDCLSB_MASK definitions
 #define PDCLSB_0     0x00000000
 #define PDCLSB_PDC0  0x00100000
 #define PDCLSB_PDC4  0x00200000
 #define PDCLSB_PDC5  0x00300000
 */

/* === Cel first preamble word flags === */
#define PRE0_LITERAL    0x80000000
#define PRE0_BGND       0x40000000
#define PREO_reservedA  0x30000000
#define PRE0_SKIPX_MASK 0x0F000000
#define PREO_reservedB  0x00FF0000
#define PRE0_VCNT_MASK  0x0000FFC0
#define PREO_reservedC  0x00000020
#define PRE0_LINEAR     0x00000010
#define PRE0_REP8       0x00000008
#define PRE0_BPP_MASK   0x00000007

#define PRE0_SKIPX_SHIFT 24
#define PRE0_VCNT_SHIFT  6
#define PRE0_BPP_SHIFT   0

/* PRE0_BPP_MASK definitions */
#define PRE0_BPP_1   0x00000001
#define PRE0_BPP_2   0x00000002
#define PRE0_BPP_4   0x00000003
#define PRE0_BPP_6   0x00000004
#define PRE0_BPP_8   0x00000005
#define PRE0_BPP_16  0x00000006

/* Subtract this value from the actual vertical source line count */
#define PRE0_VCNT_PREFETCH    1


/* === Cel second preamble word flags === */
#define PRE1_WOFFSET8_MASK   0xFF000000
#define PRE1_WOFFSET10_MASK  0x03FF0000
#define PRE1_NOSWAP          0x00004000
#define PRE1_TLLSB_MASK      0x00003000
#define PRE1_LRFORM          0x00000800
#define PRE1_TLHPCNT_MASK    0x000007FF

#define PRE1_WOFFSET8_SHIFT   24
#define PRE1_WOFFSET10_SHIFT  16
#define PRE1_TLLSB_SHIFT      12
#define PRE1_TLHPCNT_SHIFT    0

#define PRE1_TLLSB_0     0x00000000
#define PRE1_TLLSB_PDC0  0x00001000 /* Normal */
#define PRE1_TLLSB_PDC4  0x00002000
#define PRE1_TLLSB_PDC5  0x00003000

/* Subtract this value from the actual word offset */
#define PRE1_WOFFSET_PREFETCH 2
/* Subtract this value from the actual pixel count */
#define PRE1_TLHPCNT_PREFETCH 1

#define PPMP_0_SHIFT 0
#define PPMP_1_SHIFT 16

#define PPMPC_1S_MASK  0x00008000
#define PPMPC_MS_MASK  0x00006000
#define PPMPC_MF_MASK  0x00001C00
#define PPMPC_SF_MASK  0x00000300
#define PPMPC_2S_MASK  0x000000C0
#define PPMPC_AV_MASK  0x0000003E
#define PPMPC_2D_MASK  0x00000001

#define PPMPC_MS_SHIFT  13
#define PPMPC_MF_SHIFT  10
#define PPMPC_SF_SHIFT  8
#define PPMPC_2S_SHIFT  6
#define PPMPC_AV_SHIFT  1

/* PPMPC_1S_MASK definitions */
#define PPMPC_1S_PDC   0x00000000
#define PPMPC_1S_CFBD  0x00008000

/* PPMPC_MS_MASK definitions */
#define PPMPC_MS_CCB         0x00000000
#define PPMPC_MS_PIN         0x00002000
#define PPMPC_MS_PDC_MFONLY  0x00004000
#define PPMPC_MS_PDC         0x00004000

/* PPMPC_MF_MASK definitions */
#define PPMPC_MF_1  0x00000000
#define PPMPC_MF_2  0x00000400
#define PPMPC_MF_3  0x00000800
#define PPMPC_MF_4  0x00000C00
#define PPMPC_MF_5  0x00001000
#define PPMPC_MF_6  0x00001400
#define PPMPC_MF_7  0x00001800
#define PPMPC_MF_8  0x00001C00

/* PPMPC_SF_MASK definitions */
#define PPMPC_SF_2   0x00000100
#define PPMPC_SF_4   0x00000200
#define PPMPC_SF_8   0x00000300
#define PPMPC_SF_16  0x00000000

/* PPMPC_2S_MASK definitions */
#define PPMPC_2S_0     0x00000000
#define PPMPC_2S_CCB   0x00000040
#define PPMPC_2S_CFBD  0x00000080
#define PPMPC_2S_PDC   0x000000C0

/* PPMPC_2D_MASK definitions */
#define PPMPC_2D_1  0x00000000
#define PPMPC_2D_2  0x00000001


#ifndef DONTPACK
#pragma pack(push,1)
#endif

struct cp1btag {
	uint16_t c : 1;
	uint16_t pad : 15;
};
struct cp2btag {
	uint16_t c : 2;
	uint16_t pad : 14;
};
typedef struct cp4btag {
	uint16_t c : 4;
	uint16_t pad : 12;
} cp4b;
struct cp6btag {
	uint16_t c : 5;
	uint16_t pw : 1;
	uint16_t pad : 10;
};
struct cp8btag {
	uint16_t c : 5;
	uint16_t mpw : 1;
	uint16_t m : 2;
	uint16_t pad : 8;
};
struct cp16btag {
	uint16_t c : 5;
	uint16_t mb : 3;
	uint16_t mg : 3;
	uint16_t mr : 3;
	uint16_t pad : 1;
	uint16_t pw : 1;
};
struct up8btag {
	uint16_t b : 2;
	uint16_t g : 3;
	uint16_t r : 3;
	uint16_t pad : 8;
};
struct up16btag {
	uint16_t bw : 1;
	uint16_t b : 4;
	uint16_t g : 5;
	uint16_t r : 5;
	uint16_t p : 1;
};
struct res16btag {
	uint16_t b : 5;
	uint16_t g : 5;
	uint16_t r : 5;
	uint16_t p : 1;
};

union pdeco {
	unsigned int raw;
	struct cp1btag c1b;
	struct cp2btag c2b;
	struct cp4btag c4b;
	struct cp6btag c6b;
	struct cp8btag c8b;
	struct cp16btag c16b;
	struct up8btag u8b;
	struct up16btag u16b;
	struct res16btag r16b;
};

struct avtag {
	uint8_t NEG : 1;
	uint8_t XTEND : 1;
	uint8_t nCLIP : 1;
	uint8_t dv3 : 2;
	uint8_t pad : 3;
};

union AVS {
	struct avtag avsignal;
	unsigned int raw;
};

struct pixctag {
	uint8_t dv2 : 1;
	uint8_t av : 5; // why int don't work???
	uint8_t s2 : 2;
	uint8_t dv1 : 2;
	uint8_t mxf : 3;
	uint8_t ms : 2;
	uint8_t s1 : 1;
};

union   PXC {
	struct pixctag meaning;
	unsigned int raw;
};

#ifndef DONTPACK
#pragma pack(pop)
#endif


//*******************************************
#ifndef DONTPACK
#pragma pack(push,1)
#endif
struct MADAMDatum {
	uint32_t mregs[2048 + 64];
	uint16_t PLUT[32];
	uint8_t PBUSQueue[20];
	int32_t RMOD;
	int32_t WMOD;
	unsigned int _madam_FSM;
};
#ifndef DONTPACK
#pragma pack(pop)
#endif
static struct MADAMDatum madam;

uint32_t Get_madam_FSM(void)
{
	return madam._madam_FSM;
}

void Set_madam_FSM(uint32_t val)
{
	madam._madam_FSM = val;
}

uint32_t _madam_SaveSize(void)
{
	return sizeof(struct MADAMDatum);
}

void _madam_Save(void *buff)
{
	memcpy(buff, &madam, sizeof(struct MADAMDatum));
}

void _madam_Load(void *buff)
{
	memcpy(&madam, buff, sizeof(struct MADAMDatum));
}

#define mregs madam.mregs
#define PLUT madam.PLUT
#define PBUSQueue madam.PBUSQueue
#define RMOD madam.RMOD
#define WMOD madam.WMOD
#define _madam_FSM madam._madam_FSM
//*******************************************

uint32_t PXOR1, PXOR2;


#define PDV(x) ((((x) - 1) & 3) + 1)


#define MIN(x, y) (x) + (((signed int)((y) - (x)) >> 31 & ((y) - (x))))
#define MAX(x, y) (y) - (((signed int)((y) - (x)) >> 31 & ((y) - (x))))

#define TESTCLIP(cx, cy) ( ((cx) >= 0) && ((cx) <= CLIPXVAL) && ((cy) >= 0) && ((cy) <= CLIPYVAL) )


#define  XY2OFF(a, b, c)   (  (((b) >> 1) * (c) /*bitmap width*/)   + (((int)(b) & 1) << 1) +    (a)    )


#define PBMASK 0x80000000
#define KUP    0x08000000
#define KDN    0x10000000
#define KRI    0x04000000
#define KLE    0x02000000
#define KA     0x01000000
#define KB     0x00800000
#define KC     0x00400000
#define KP     0x00200000
#define KX     0x00100000
#define KRS    0x00080000
#define KLS    0x00040000
#define FIXP16_SHIFT     16
#define FIXP16_MAG       65536
#define FIXP16_DP_MASK   0x0000ffff
#define FIXP16_WP_MASK   0xffff0000
#define FIXP16_ROUND_UP  0x0000ffff //0x8000



// TYPES ///////////////////////////////////////////////////////////////////


// CLASSES /////////////////////////////////////////////////////////////////
uint32_t  mread(uint32_t addr);
void  mwrite(uint32_t addr, uint32_t val);
int TestInitVisual(int packed);
int Init_Line_Map(void);
void Init_Scale_Map(void);
void Init_Arbitrary_Map(void);
void TexelDraw_BitmapRow(uint16_t LAMV, int xcur, int ycur, int cnt);
void TexelDraw_Line(uint16_t CURPIX, uint16_t LAMV, int xcur, int ycur, int cnt);
int  TexelDraw_Scale(uint16_t CURPIX, uint16_t LAMV, int xcur, int ycur, int deltax, int deltay);
int  TexelDraw_Arbitrary(uint16_t CURPIX, uint16_t LAMV, int xA, int yA, int xB, int yB, int xC, int yC, int xD, int yD);
void  DrawPackedCel_New(void);
void  DrawLiteralCel_New(void);
void  DrawLRCel_New(void);
void HandleDMA8(void);
void DMAPBus(void);


uint32_t MAPPING;

// general 3D vertex class

#define INT1220(a)       ((signed int)(a) >> 20)
#define INT1220up(a) ((signed int)((a) + (1 << 19)) >> 20)

static struct {
	uint32_t plutaCCBbits;
	uint32_t pixelBitsMask;
	bool tmask;
} pdec;

static struct {
	uint32_t pmode;
	uint32_t pmodeORmask;
	uint32_t pmodeANDmask;
	bool Transparent;
} pproj;

unsigned int pbus = 0;
uint8_t * Mem;
unsigned int retuval;
unsigned int BITADDR;
unsigned int BITBUFLEN;
unsigned int BITBUF;
unsigned int CCBFLAGS, /*PLUTDATA*/ PIXC, PRE0, PRE1, TARGETPROJ, SRCDATA, debug;
int SPRWI, SPRHI;
unsigned int PLUTF, PDATF, NCCBF;
int CELCYCLES, __smallcycles;
bool ADD;
//static SDL_Event cpuevent;
int BITCALC;


uint16_t bitbuf;        //bit buffer
uint8_t subbitbuf;      // bit sub buffer
int bitcount;           // bit counter
long compsize;          // size of commpressed!!! in bytes!!! actually pixcount*bpp/8!!!
unsigned int gFINISH;
uint16_t RRR;
int USECEL;

unsigned int const BPP[8] = { 1, 1, 2, 4, 6, 8, 16, 1 };

uint8_t PSCALAR[8][4][32];

uint16_t MAPu8b[256 + 64], MAPc8bAMV[256 + 64], MAPc16bAMV[8 * 8 * 8 + 64];


int currentrow;
unsigned int bpp;
int pixcount;
unsigned int type;
unsigned int offsetl;
unsigned int offset;
//static unsigned int begining;
unsigned int eor;
int calcx;
int nrows;

uint16_t ttt;

unsigned int OFF;

unsigned int pSource;

bool celNeedsFramePixel = false;
bool celNeedsPPROC = false;
bool celNeedsPPROJ = false;

//AString str;

//CelEngine STATBits
#define STATBITS        mregs[0x28]

#define SPRON           0x10
#define SPRPAU          0x20

//CelEngine Registers
#define SPRSTRT         0x100
#define SPRSTOP         0x104
#define SPRCNTU         0x108
#define SPRPAUS         0x10c

#define CCBCTL0         mregs[0x110]
#define REGCTL0         mregs[0x130]
#define REGCTL1         mregs[0x134]
#define REGCTL2         mregs[0x138]
#define REGCTL3         mregs[0x13c]

#define CLIPXVAL        ((int)mregs[0x134] & 0x3ff)
#define CLIPYVAL        ((int)(mregs[0x134] >> 16) & 0x3ff)

#define PIXSOURCE       (mregs[0x138])
#define FBTARGET        (mregs[0x13c])

#define CURRENTCCB      mregs[0x5a0]
//next ccb == 0 stop the engine
#define NEXTCCB         mregs[0x5a4]
#define PLUTDATA        mregs[0x5a8]
#define PDATA           mregs[0x5ac]
#define ENGAFETCH       mregs[0x5b0]
#define ENGALEN         mregs[0x5b4]
#define ENGBFETCH       mregs[0x5b8]
#define ENGBLEN         mregs[0x5bc]
#define PAL_EXP         (&mregs[0x5d0])


//////////////////////////////////////////////////////////////////////
// Quick divide helper
//////////////////////////////////////////////////////////////////////

#define QUICK_DIVIDE_CACHE_SIZE 512
static int QUICK_DIVIDE_UBOUND = 0;
static int QUICK_DIVIDE_LBOUND = 0;

static int16_t quickDivide_lookups[QUICK_DIVIDE_CACHE_SIZE][QUICK_DIVIDE_CACHE_SIZE];

static void quickDivide_init(void)
{
	int a, b;

	QUICK_DIVIDE_UBOUND  = (QUICK_DIVIDE_CACHE_SIZE / 2) - 1;
	QUICK_DIVIDE_LBOUND  = -(QUICK_DIVIDE_CACHE_SIZE / 2);
	for (a = QUICK_DIVIDE_LBOUND; a <= QUICK_DIVIDE_UBOUND; a++) {
		for (b = QUICK_DIVIDE_LBOUND; b <= QUICK_DIVIDE_UBOUND; b++) {
			if (b == 0)
				quickDivide_lookups[a - QUICK_DIVIDE_LBOUND][b - QUICK_DIVIDE_LBOUND] = 0;
			else
				quickDivide_lookups[a - QUICK_DIVIDE_LBOUND][b - QUICK_DIVIDE_LBOUND] = a / b;
		}
	}
}

static INLINE int quickDivide(int a, int b)
{
	if (a >= QUICK_DIVIDE_LBOUND
	    && a <= QUICK_DIVIDE_UBOUND
	    && b >= QUICK_DIVIDE_LBOUND
	    && b <= QUICK_DIVIDE_UBOUND)
		return quickDivide_lookups[a - QUICK_DIVIDE_LBOUND][b - QUICK_DIVIDE_LBOUND];
	return a / b;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//void MapCoord(poly *pol);
//void RenderPoly(void);

unsigned int  _madam_Peek(unsigned int addr)
{
	//	if((addr>=0x400)&&(addr<=0x53f))
	//		printf("#Madam Peek [%X]=%X\n",addr,mregs[addr]);


	if ((addr >= 0x400) && (addr <= 0x53f)) {
		//we need to return actual fifo status!!!!
		return _clio_FIFOStruct(addr);
	}


	if (addr == 0x28) { // STATUS OF CEL
		switch (_madam_FSM) {
		case FSM_IDLE:
			return 0x0;
		case FSM_SUSPENDED:
			return 0x30;
		case FSM_INPROCESS:
			return 0x10;
		}
	}
#if 0
	if (addr >= 0x580 && addr < 0x5A0) {
		//sprintf(str,"CLUT - MADAM Read madam[0x%X]\n",addr);
		//CDebug::DPrint(str);
	}
#endif
	return mregs[addr];
}


void  _madam_Poke(unsigned int addr, unsigned int val)
{
	/*
	   if(addr==0x13c)
	   {
	   sprintf(str,"Switch screen SWI 0x%8X addr 0x%8X\n",last_SWI,val);
	   CDebug::DPrint(str);
	   }*/

	if ((addr >= 0x400) && (addr <= 0x53f)) {

		_clio_SetFIFO(addr, val);

		return;

	}else
		switch (addr) {
		case 0x4:
			val = 0x29;
			mregs[addr] = val;
			break;

		case 0x8:
			mregs[addr] = val;
			HandleDMA8();
			break;
		case 0x580:
			_vdl_ProcessVDL(val);
			return;
		case 0x584:
		case 0x588:
		case 0x58C:
		case 0x590:
		case 0x594:
		case 0x598:
		case 0x59C:
			//_3do_DPrint(str.print("CLUT - MADAM Write madam[0x%X] = 0x%8.8X\n",addr,val));
			mregs[addr] = val;
			return;
		case 0x0:
			return;
		case SPRSTRT:
			if (_madam_FSM == FSM_IDLE)
				_madam_FSM = FSM_INPROCESS;
			return;

		case SPRSTOP:
			_madam_FSM = FSM_IDLE;
			NEXTCCB = 0;
			return;

		case SPRCNTU:
			if (_madam_FSM == FSM_SUSPENDED)
				_madam_FSM = FSM_INPROCESS;
			return;

		case SPRPAUS:
			if (_madam_FSM == FSM_INPROCESS)
				_madam_FSM = FSM_SUSPENDED;
			return;

			//Matrix engine macros
#define M00  ((int64_t)(int32_t)mregs[0x600])
#define M01  ((int64_t)(int32_t)mregs[0x604])
#define M02  ((int64_t)(int32_t)mregs[0x608])
#define M03  ((int64_t)(int32_t)mregs[0x60C])
#define M10  ((int64_t)(int32_t)mregs[0x610])
#define M11  ((int64_t)(int32_t)mregs[0x614])
#define M12  ((int64_t)(int32_t)mregs[0x618])
#define M13  ((int64_t)(int32_t)mregs[0x61C])
#define M20  ((int64_t)(int32_t)mregs[0x620])
#define M21  ((int64_t)(int32_t)mregs[0x624])
#define M22  ((int64_t)(int32_t)mregs[0x628])
#define M23  ((int64_t)(int32_t)mregs[0x62C])
#define M30  ((int64_t)(int32_t)mregs[0x630])
#define M31  ((int64_t)(int32_t)mregs[0x634])
#define M32  ((int64_t)(int32_t)mregs[0x638])
#define M33  ((int64_t)(int32_t)mregs[0x63C])

#define  V0  ((int64_t)(int32_t)mregs[0x640])
#define  V1  ((int64_t)(int32_t)mregs[0x644])
#define  V2  ((int64_t)(int32_t)mregs[0x648])
#define  V3  ((int64_t)(int32_t)mregs[0x64C])

#define Rez0 mregs[0x660]
#define Rez1 mregs[0x664]
#define Rez2 mregs[0x668]
#define Rez3 mregs[0x66C]

#define Nfrac16 (((int64_t)mregs[0x680] << 32) | (unsigned int)mregs[0x684])

		// Matix engine
		case 0x7fc:

			mregs[0x7fc] = 0; // Ours matrix engine already ready

			static int64_t Rez0T, Rez1T, Rez2T, Rez3T;

			switch (val) { // Cmd
			case 0: //printf("#Matrix = NOP\n");
				Rez0 = Rez0T;
				Rez1 = Rez1T;
				Rez2 = Rez2T;
				Rez3 = Rez3T;
				return; // NOP


			case 1: //multiply a 4x4 matrix of 16.16 values by a vector of 16.16 values

				Rez0 = Rez0T;
				Rez1 = Rez1T;
				Rez2 = Rez2T;
				Rez3 = Rez3T;


				Rez0T = (int)((M00 * V0 + M01 * V1 + M02 * V2 + M03 * V3) >> 16);
				Rez1T = (int)((M10 * V0 + M11 * V1 + M12 * V2 + M13 * V3) >> 16);
				Rez2T = (int)((M20 * V0 + M21 * V1 + M22 * V2 + M23 * V3) >> 16);
				Rez3T = (int)((M30 * V0 + M31 * V1 + M32 * V2 + M33 * V3) >> 16);

				return;
			case 2: //multiply a 3x3 matrix of 16.16 values by a vector of 16.16 values
				Rez0 = Rez0T;
				Rez1 = Rez1T;
				Rez2 = Rez2T;
				Rez3 = Rez3T;

				Rez0T = (int)((M00 * V0 + M01 * V1 + M02 * V2) >> 16);
				Rez1T = (int)((M10 * V0 + M11 * V1 + M12 * V2) >> 16);
				Rez2T = (int)((M20 * V0 + M21 * V1 + M22 * V2) >> 16);
				//printf("#Matrix CMD2, R0=0x%8.8X, R1=0x%8.8X, R2=0x%8.8X\n",Rez0,Rez1,Rez2);
				return;

			case 3: // Multiply a 3x3 matrix of 16.16 values by multiple vectors, then multiply x and y by n/z
			{       // Return the result vectors {x*n/z, y*n/z, z}


				Rez0 = Rez0T;
				Rez1 = Rez1T;
				Rez2 = Rez2T;
				Rez3 = Rez3T;

				int64_t M = Nfrac16;

				Rez2T = (signed int)((M20 * V0 + M21 * V1 + M22 * V2) >> 16);       // z
				if (Rez2T != 0)
					M /= (int64_t)Rez2T;                                             // n/z

				Rez0T = (signed int)((M00 * V0 + M01 * V1 + M02 * V2) >> 16);
				Rez1T = (signed int)((M10 * V0 + M11 * V1 + M12 * V2) >> 16);


				Rez0T = (int64_t)((Rez0T * M) >> 32);      // x * n/z
				Rez1T = (int64_t)((Rez1T * M) >> 32);      // y * n/z

			}
			break;
			default:
				break;
			}
			break;
		case 0x130:
			mregs[addr] = val; //modulo variables :)
			RMOD = ((val & 1) << 7) + ((val & 12) << 8) + ((val & 0x70) << 4);
			val >>= 8;
			WMOD = ((val & 1) << 7) + ((val & 12) << 8) + ((val & 0x70) << 4);
			break;
		default:
			mregs[addr] = val;
			break;
		}
}

unsigned int OFFSET;
unsigned int temp1;
unsigned int Flag;



static int HDDX1616, HDDY1616, HDX1616, HDY1616, VDX1616, VDY1616, XPOS1616, YPOS1616, HDX1616_2, HDY1616_2;
static unsigned int CEL_ORIGIN_VH_VALUE;
static int8_t TEXEL_FUN_NUMBER;
static int TEXTURE_WI_START, TEXTURE_HI_START, TEXEL_INCX, TEXEL_INCY;
static int TEXTURE_WI_LIM, TEXTURE_HI_LIM;


void LoadPLUT(unsigned int pnt, int n)
{
	int i;

	for (i = 0; i < n; i++)
#ifdef MSB_FIRST
		PLUT[i] = _mem_read16((((pnt >> 1) + i)) << 1);
#else
		PLUT[i] = _mem_read16((((pnt >> 1) + i) ^ 1) << 1);
#endif
}

int CCBCOUNTER;
int _madam_HandleCEL(void)
{
	__smallcycles = CELCYCLES = 0;
	if (NEXTCCB != 0)
		CCBCOUNTER = 0;
	STATBITS |= SPRON;
	Flag = 0;


	while ((NEXTCCB != 0) && (!Flag)) {
		//if(_madam_FSM==FSM_INPROCESS)
		CCBCOUNTER++;
		if ((NEXTCCB == 0) || (Flag)) {
			_madam_FSM = FSM_IDLE;
			return CELCYCLES;
		}
		//1st step -- parce CCB and load it into registers
		CURRENTCCB = NEXTCCB & 0xfffffc;
		if ((CURRENTCCB >> 20) > 2) {
			_madam_FSM = FSM_IDLE;
			return CELCYCLES;
		}
		OFFSET = CURRENTCCB;


		CCBFLAGS = mread(CURRENTCCB);

		CURRENTCCB += 4;


		if (CCBFLAGS & CCB_PXOR) {
			PXOR1 = 0;
			PXOR2 = 0x1f1f1f1f;
		} else {
			PXOR1 = 0xFFffFFff;
			PXOR2 = 0;
		}
		Flag = 0;
		PLUTF = PDATF = NCCBF = 0;

		NEXTCCB = mread(CURRENTCCB) & (~3);


		if (!(CCBFLAGS & CCB_NPABS)) {
			NEXTCCB += CURRENTCCB + 4;
			NEXTCCB &= 0xffffff;
		}
		if (NEXTCCB == 0)
			NCCBF = 1;
		if ((NEXTCCB >> 20) > 2)
			NCCBF = 1;

		CURRENTCCB += 4;


		PDATA = mread(CURRENTCCB) & (~3);
		//if((PDATA==0))
		//	PDATF=1;
		if (!(CCBFLAGS & CCB_SPABS)) {
			PDATA += CURRENTCCB + 4;
			PDATA &= 0xffffff;
		}
		if ((PDATA >> 20) > 2)
			PDATF = 1;
		CURRENTCCB += 4;

		if ((CCBFLAGS & CCB_LDPLUT)) {
			PLUTDATA = mread(CURRENTCCB) & (~3);
			//if((PLUTDATA==0))
			//    PLUTF=1;
			if (!(CCBFLAGS & CCB_PPABS)) {
				PLUTDATA += CURRENTCCB + 4;
				PLUTDATA &= 0xffffff;
			}
			if ((PLUTDATA >> 20) > 2)
				PLUTF = 1;
		}
		CURRENTCCB += 4;


		if (NCCBF)
			CCBFLAGS |= CCB_LAST;


		if (CCBFLAGS & CCB_LAST)
			Flag = 1;


		if (CCBFLAGS & CCB_YOXY) {
			XPOS1616 = mread(CURRENTCCB);
			CURRENTCCB += 4;
			YPOS1616 = mread(CURRENTCCB);
			CURRENTCCB += 4;
		}else
			CURRENTCCB += 8;

		// Get the VH value for this cel. This is done in case the
		// cel later decides to use the position as the source of
		// its VH values in the projector.
		CEL_ORIGIN_VH_VALUE = (XPOS1616 & 0x1) | ((YPOS1616 & 0x1) << 15);

		//if((CCBFLAGS&CCB_SKIP)&& debug)
		//	printf("###Cel skipped!!! PDATF=%d PLUTF=%d NCCBF=%d\n",PDATF,PLUTF,NCCBF);


		if (CCBFLAGS & CCB_LAST)
			NEXTCCB = 0;
		if (CCBFLAGS & CCB_LDSIZE) {
			HDX1616 = ((int)mread(CURRENTCCB)) >> 4;
			CURRENTCCB += 4;
			HDY1616 = ((int)mread(CURRENTCCB)) >> 4;

			CURRENTCCB += 4;
			VDX1616 = mread(CURRENTCCB);
			CURRENTCCB += 4;
			VDY1616 = mread(CURRENTCCB);
			CURRENTCCB += 4;
		}
		if (CCBFLAGS & CCB_LDPRS) {
			HDDX1616 = ((int)mread(CURRENTCCB)) >> 4;
			CURRENTCCB += 4;
			HDDY1616 = ((int)mread(CURRENTCCB)) >> 4;
			CURRENTCCB += 4;
		}
		if (CCBFLAGS & CCB_LDPPMP) {
			PIXC = mread(CURRENTCCB);
			CURRENTCCB += 4;
		}
		if (CCBFLAGS & CCB_CCBPRE) {
			PRE0 = mread(CURRENTCCB);
			CURRENTCCB += 4;
			if (!(CCBFLAGS & CCB_PACKED)) {
				PRE1 = mread(CURRENTCCB);
				CURRENTCCB += 4;
			}
		} else if (!PDATF) {
			PRE0 = mread(PDATA);
			PDATA += 4;
			if (!(CCBFLAGS & CCB_PACKED)) {
				PRE1 = mread(PDATA);
				PDATA += 4;
			}
		}

		{       // PDEC data compute
			//pdec.mode=PRE0&PRE0_BPP_MASK;
			switch (PRE0 & PRE0_BPP_MASK) {
			case 0:
			case 7:
				continue;
			case 1:
				pdec.plutaCCBbits = (CCBFLAGS & 0xf) * 4;
				pdec.pixelBitsMask = 1; // 1 bit
				break;
			case 2:
				pdec.plutaCCBbits = (CCBFLAGS & 0xe) * 4;
				pdec.pixelBitsMask = 3;         // 2 bit
				break;
			default:                                //case 3:
				pdec.plutaCCBbits = (CCBFLAGS & 0x8) * 4;
				pdec.pixelBitsMask = 15;        // 4 bit
				break;
			}
			pdec.tmask = !(CCBFLAGS & CCB_BGND);

			pproj.pmode = (CCBFLAGS & CCB_POVER_MASK);
			pproj.pmodeORmask = (pproj.pmode == PMODE_ONE ) ? 0x8000 : 0x0000;
			pproj.pmodeANDmask = (pproj.pmode != PMODE_ZERO) ? 0xFFFF : 0x7FFF;
		}

		if ((CCBFLAGS & CCB_LDPLUT) && !PLUTF) { //load PLUT
			switch (PRE0 & PRE0_BPP_MASK) {
			case 1:
				LoadPLUT(PLUTDATA, 2);
				break;
			case 2:
				LoadPLUT(PLUTDATA, 4);
				break;
			case 3:
				LoadPLUT(PLUTDATA, 16);
				break;
			default:
				LoadPLUT(PLUTDATA, 32);
			}
			;
		}

		//ok -- CCB decoded -- let's print out our current status
		//step#2 -- getting CEL data
		//*
		if (!(CCBFLAGS & CCB_SKIP) && !PDATF) {
			if (CCBFLAGS & CCB_PACKED)
				DrawPackedCel_New();
			else{

				if ((PRE1 & PRE1_LRFORM) && (BPP[PRE0 & PRE0_BPP_MASK] == 16))
					DrawLRCel_New();
				else
					DrawLiteralCel_New();

			}

		}       //if(!(CCBFLAGS& CCB_SKIP))
	}               //while

	//STATBITS&=~SPRON;
	if ((NEXTCCB == 0) || (Flag)) {
		_madam_FSM = FSM_IDLE;
	}

	return CELCYCLES;
}//HandleCEL

void HandleDMA8(void)
{
	if (mregs[0x8] & 0x8000) {// pbus transfer
		DMAPBus();
		mregs[0x8] &= ~0x8000; // dma done

		_clio_GenerateFiq(0, 1);
	}
}

void DMAPBus(void)
{
	unsigned int i = 0;

	if ((int)mregs[0x574] < 0)
		return;

	mregs[0x574] -= 4;
	mregs[0x570] += 4;
	mregs[0x578] += 4;

	while ((int)mregs[0x574] > 0) {
		if (i < 5) WriteIO(mregs[0x570], ((unsigned int*)PBUSQueue)[i]);
		else WriteIO(mregs[0x570], 0xffffffff);
		mregs[0x574] -= 4;
		mregs[0x570] += 4;
		mregs[0x578] += 4;
		i++;
	}

	mregs[0x574] = 0xfffffffc;
}

void _madam_KeyPressed(uint8_t* data, unsigned int num)
{
	if (num > 16)
		num = 16;
	if (num)
		memcpy(PBUSQueue, data, num);
	memset(&PBUSQueue[num], -1, 20 - num);
}


void _madam_Init(uint8_t *memory)
{
	int i, j, n;

	ADD = 0;
	debug = 0;
	USECEL = 1;
	CELCYCLES = 0;
	Mem = memory;

	bitoper.bitset = 1;

	quickDivide_init();

	MAPPING = 1;

	_madam_FSM = FSM_IDLE;

	for (i = 0; i < 2048; i++)
		mregs[i] = 0;

	mregs[004] = 0x29;      // DRAM dux init
	mregs[574] = 0xfffffffc;

#if 1
	mregs[000] = 0x01020000; // for Green matrix engine autodetect
	//mregs[000]=0x02022000; // for Green matrix engine autodetect
#else
	mregs[000] = 0x01020001; // for ARM soft emu of matrix engine
#endif

	for (i = 0; i < 32; i++)
		for (j = 0; j < 8; j++)
			for (n = 0; n < 4; n++)
				PSCALAR[j][n][i] = ((i * (j + 1)) >> PDV(n));

	for (i = 0; i < 256; i++) {
		union pdeco pix1, pix2;
		uint16_t pres, resamv;

		pix1.raw = i;
		pix2.r16b.b = (pix1.u8b.b << 3) + (pix1.u8b.b << 1) + (pix1.u8b.b >> 1);
		pix2.r16b.g = (pix1.u8b.g << 2) + (pix1.u8b.g >> 1);
		pix2.r16b.r = (pix1.u8b.r << 2) + (pix1.u8b.r >> 1);
		pres = pix2.raw;
		pres &= 0x7fff; //pmode=0;
		MAPu8b[i] = pres;

		resamv = (pix1.c8b.m << 1) + pix1.c8b.mpw;
		resamv = (resamv << 6) + (resamv << 3) + resamv;
		MAPc8bAMV[i] = resamv;
	}
	for (i = 0; i < (8 * 8 * 8); i++) {
		union pdeco pix1;

		pix1.raw = i << 5;
		MAPc16bAMV[i] = (pix1.c16b.mr << 6) + (pix1.c16b.mg << 3) + pix1.c16b.mb;
	}
}

extern void _3do_InternalFrame(int cycles);

void exteraclocker(void)
{
	if ((CELCYCLES - __smallcycles) >> 7) {
		__smallcycles = CELCYCLES;
		//_3do_InternalFrame(64);
	}
}

static INLINE uint16_t readPixelLR(uint32_t src, int x, int y, int offset)
{
	src += XY2OFF((x << 2), y, offset);
	#ifdef MSB_FIRST
		return *((uint16_t*)&Mem[src]);
	#else
		return *((uint16_t*)&Mem[src ^ 2]);
	#endif
}

static INLINE uint16_t readFramebufferPixel(uint32_t src, int x, int y)
{
	src += XY2OFF((x << 2), y, RMOD);
	#ifdef MSB_FIRST
		return *((uint16_t*)&Mem[src]);
	#else
		return *((uint16_t*)&Mem[src ^ 2]);
	#endif
}

static INLINE void writeFramebufferPixel(uint32_t src, int x, int y, uint16_t pix)
{
	src += XY2OFF((x << 2), y, WMOD);
	#ifdef MSB_FIRST
		*((uint16_t*)&Mem[src]) = pix;
	#else
		*((uint16_t*)&Mem[src ^ 2]) = pix;
	#endif
}

unsigned int  mread(unsigned int addr)
{
	unsigned int val;

#ifdef SAFEMEMACCESS
	//	addr&=0x3FFFFF;
#endif
	val = _mem_read32(addr);
	CELCYCLES += 1;
	//exteraclocker();
	return val;
}

void  mwrite(unsigned int addr, unsigned int val)
{
#ifdef SAFEMEMACCESS
	addr &= 0x3FFFFF;
#endif
	_mem_write32(addr, val);
	CELCYCLES += 2;
	//exteraclocker();

}

void  mwriteh(unsigned int addr, uint16_t val)
{
#ifdef SAFEMEMACCESS
	addr &= 0x3fffff;
#endif
	CELCYCLES += 2;
#ifdef MSB_FIRST
	_mem_write16((addr), val);
#else
	_mem_write16((addr ^ 2), val);
#endif
	//exteraclocker();
}

uint16_t  mreadh(unsigned int addr)
{
#ifdef SAFEMEMACCESS
	//	addr&=0x3FFFFF;
#endif
	CELCYCLES += 1;
	//exteraclocker();
#ifdef MSB_FIRST
	return _mem_read16((addr));
#else
	return _mem_read16((addr ^ 2));
#endif
}

unsigned int  readPLUTDATA(unsigned int offset)
{
	CELCYCLES += 4;
	if (PLUTDATA == 0)
		return 0;
	
	/* Gameblabla - this causes a compliation issue : To check ! TODO */
	//return *(uint16_t*)(PLUTDATA + (offset ^ 2));
	return ((uint16_t*)PAL_EXP)[((offset^2)>>1)];
}

static uint16_t PDEC(uint16_t pixel, uint16_t * amv)
{
	union pdeco pix1;
	uint16_t resamv, pres;

	pix1.raw = pixel;

	switch (PRE0 & PRE0_BPP_MASK) {
	default:
		//case 1: // 1 bit
		//case 2: // 2 bits
		//case 3: // 4 bits
		pres = PLUT[(pdec.plutaCCBbits + ((pix1.raw & pdec.pixelBitsMask) * 2)) >> 1];
		resamv = 0x49;
		break;

	case 4: // 6 bits

		pres = PLUT[pix1.c6b.c];
		pres = (pres & 0x7FFF) + (pix1.c6b.pw << 15); //pmode=pix1.c6b.pw; ???

		resamv = 0x49;
		break;

	case 5: // 8 bits

		if (PRE0 & PRE0_LINEAR) {
			// (Uncoded 8 bit CEL)

			pres = MAPu8b[pix1.raw & 0xFF];

			resamv = 0x49;
		} else {
			// (Coded 8 bit CEL)

			pres = PLUT[pix1.c8b.c];

			resamv = MAPc8bAMV[pix1.raw & 0xFF];
		}
		break;

	case 6: // 16 bits
	case 7:
		//*amv=0;
		//pres=0;
		//Transparent=0;
		if ((PRE0 & PRE0_LINEAR)) {
			// (Uncoded 16 bit CEL)

			pres = pix1.raw;
			//pres&=0x7ffe;

			//pres=0x11;
			// pres=(pres&0x7fff)+(pix1.u16b.p<<15);//pmode=pix1.u16b.p; ???
			resamv = 0x49;

		} else {
			// (Coded 16 bit CEL)

			pres = PLUT[pix1.c16b.c];
			pres = (pres & 0x7fff) | (pixel & 0x8000);
			resamv = MAPc16bAMV[(pix1.raw >> 5) & 0x1FF];
			//nop: pres=(pres&0x7fff)+(pix1.c16b.pw<<15);//pmode=pix1.c16b.pw; ???
		}

		break;
	}

	*amv = resamv;

	// (Conceptual end of DECODER)

	//////////////////////
	// TODO: Do PROJECTOR functions now?
	// They'll be done before using the PROCESSOR.



	//if(!(PRE1&PRE1_NOSWAP) && (CCBCTL0&(1<<27)))
	//			pres=(pres&0x7ffe)|((pres&0x8000)>>15)|((pres&1)<<15);

	//if(!(CCBCTL0&0x80000000))pres=(pres&0x7fff)|((CCBCTL0>>15)&0x8000);

	//pres=(pres|pdec.pmodeORmask)&pdec.pmodeANDmask;


	pproj.Transparent = ( ((pres & 0x7fff) == 0x0) & pdec.tmask );

	return pres;
}

unsigned int  PPROJ_OUTPUT(unsigned int pdec_output, unsigned int pproc_output, unsigned int pframe_input)
{
	unsigned int VHOutput;

	///////////////////////////
	// CCB_PLUTPOS flag
	// Determine projector's originating source of VH values.
	if (CCBFLAGS & CCB_PLUTPOS) {
		// Use pixel decoder output.
		VHOutput = (pdec_output & 0x8001);
	} else {
		// Use VH values determined from the CEL's origin.
		VHOutput = CEL_ORIGIN_VH_VALUE;
	}

	//////////////////////////
	// SWAPHV flag
	// Swap the H and V values now if requested.
	if (CCBCTL0 & SWAPHV) {
		// TODO: I have read that PRE1 is only set for unpacked CELs.
		//       So... should this be ignored if using packed CELs? I don't know.
		if (!(PRE1 & PRE1_NOSWAP))
			VHOutput = (VHOutput >> 15) | ((VHOutput & 1) << 15);
	}

	//////////////////////////
	// CFBDSUB flag
	// Substitute the VH values from the frame buffer if requested.
	if (CCBCTL0 & CFBDSUB) {
		// TODO: This should be re-enabled sometime. However, it currently
		//       causes the wing commander 3 movies to screw up again! There
		//       must be some missing mbehavior elsewhere.
		//VHOutput = (pframe_input & 0x8001);
		(void)pframe_input;
	}

	//////////////////////////
	// B15POS_MASK settings
	// Substitute the V value explicitly if requested.
	unsigned int b15mode = (CCBCTL0 & B15POS_MASK);
	if (b15mode == B15POS_PDC) {
		// Don't touch it.
	} else if (b15mode == B15POS_0) {
		VHOutput = (VHOutput & ~0x8000);
	} else if (b15mode == B15POS_1) {
		VHOutput |= 0x8000;
	}

	//////////////////////////
	// B15POS_MASK settings
	// Substitute the H value explicitly if requested.
	int b0mode = (CCBCTL0 & B0POS_MASK);
	if (b0mode == B0POS_PDC) {
		// Don't touch it.
	} else if (b0mode == B0POS_PPMP) {
		// Use LSB from pixel processor output.
		VHOutput = (VHOutput & ~0x1) | (pproc_output & 0x1);
	} else if (b0mode == B0POS_0) {
		VHOutput = (VHOutput & ~0x1);
	} else if (b0mode == B0POS_1) {
		VHOutput |= 0x01;
	}

	return (pproc_output & 0x7FFE) | VHOutput;
}

unsigned int  PPROC(unsigned int pixel, unsigned int fpix, unsigned int amv)
{
	union AVS AV;
	union PXC pixc;

	union pdeco input1, out, pix1;

	// Set PMODE according to the values set up in the CCBFLAGS word.
	// (This merely uses masks here because it's faster).
	// This is a duty of the PROJECTOR, but we'll do it here because its easier.
	pixel = (pixel | pproj.pmodeORmask) & pproj.pmodeANDmask;

	pixc.raw = PIXC & 0xffff;
	if ((pixel & 0x8000))
		pixc.raw = PIXC >> 16;

	//pres,fpix

	//now let's select the sources
	//1. av
	//2. input1
	//3. input2
	//pixc.raw=0;

	if (CCBFLAGS & CCB_USEAV)
		AV.raw = pixc.meaning.av;
	else{
		AV.avsignal.dv3 = 0;
		AV.avsignal.nCLIP = 0;
		AV.avsignal.XTEND = 0;
		AV.avsignal.NEG = 0;
	}

	if (!pixc.meaning.s1)
		input1.raw = pixel;
	else
		input1.raw = fpix;


#pragma pack(push,1)
	union {
		unsigned int raw;
		struct {
			int8_t R;
			int8_t B;
			int8_t G;
			int8_t a;
		};
	} color1, color2, AOP, BOP;
#pragma pack(pop)

	switch (pixc.meaning.s2) {
	case 0:
		color2.raw = 0;
		break;
	case 1:
		color2.R = color2.G = color2.B = (pixc.meaning.av >> AV.avsignal.dv3);
		break;
	case 2:
		pix1.raw = fpix;
		color2.R = (pix1.r16b.r) >> AV.avsignal.dv3;
		color2.G = (pix1.r16b.g) >> AV.avsignal.dv3;
		color2.B = (pix1.r16b.b) >> AV.avsignal.dv3;
		break;
	case 3:
		pix1.raw = pixel;
		color2.R = (pix1.r16b.r) >> AV.avsignal.dv3;
		color2.G = (pix1.r16b.g) >> AV.avsignal.dv3;
		color2.B = (pix1.r16b.b) >> AV.avsignal.dv3;
		break;
	}


	switch (pixc.meaning.ms) {
	case 0:
		color1.R = PSCALAR[pixc.meaning.mxf][pixc.meaning.dv1][input1.r16b.r];
		color1.G = PSCALAR[pixc.meaning.mxf][pixc.meaning.dv1][input1.r16b.g];
		color1.B = PSCALAR[pixc.meaning.mxf][pixc.meaning.dv1][input1.r16b.b];
		break;
	case 1:
		color1.R = PSCALAR[(amv >> 6) & 7][pixc.meaning.dv1][input1.r16b.r];
		color1.G = PSCALAR[(amv >> 3) & 7][pixc.meaning.dv1][input1.r16b.g];
		color1.B = PSCALAR[amv & 7][pixc.meaning.dv1][input1.r16b.b];
		break;
	case 2:
		pix1.raw = pixel;
		color1.R = PSCALAR[pix1.r16b.r >> 2][pix1.r16b.r & 3][input1.r16b.r];
		color1.G = PSCALAR[pix1.r16b.g >> 2][pix1.r16b.g & 3][input1.r16b.g];
		color1.B = PSCALAR[pix1.r16b.b >> 2][pix1.r16b.b & 3][input1.r16b.b];
		break;
	case 3:
		color1.R = PSCALAR[4][pixc.meaning.dv1][input1.r16b.r];
		color1.G = PSCALAR[4][pixc.meaning.dv1][input1.r16b.g];
		color1.B = PSCALAR[4][pixc.meaning.dv1][input1.r16b.b];
		break;
	}

	/*
	   // Use this to render magenta for testing.
	   if (false)
	   {
	   union pdeco fakeColor;
	   fakeColor.r16b.r = 0x1b;
	   fakeColor.r16b.g = 0x00;
	   fakeColor.r16b.b = 0x1b;
	   fakeColor.r16b.p = 1;

	   return fakeColor.raw;
	   }
	 */

	//ok -- we got the sources -- now RGB processing
	//AOP/BOP calculation
	AOP.raw = color1.raw & PXOR1;
	color1.raw &= PXOR2;


	if (AV.avsignal.NEG)
		BOP.raw = color2.raw ^ 0x00ffffff;
	else{
		BOP.raw = color2.raw ^ color1.raw;
	}

	if (AV.avsignal.XTEND) {
		BOP.R = (BOP.R << 3) >> 3;
		BOP.B = (BOP.B << 3) >> 3;
		BOP.G = (BOP.G << 3) >> 3;
	}

	color2.R = (AOP.R + BOP.R + AV.avsignal.NEG) >> pixc.meaning.dv2;
	color2.G = (AOP.G + BOP.G + AV.avsignal.NEG) >> pixc.meaning.dv2;
	color2.B = (AOP.B + BOP.B + AV.avsignal.NEG) >> pixc.meaning.dv2;


	//fprintf(flog,"%d %d %02x\t%02d %02d %02d\n", pixc.meaning.s2, pixc.meaning.ms, AV.raw, color2.R, color2.G, color2.B);

	if (!AV.avsignal.nCLIP) {
		if (color2.R < 0) color2.R = 0;
		else if (color2.R > 31) color2.R = 31;

		if (color2.G < 0) color2.G = 0;
		else if (color2.G > 31) color2.G = 31;

		if (color2.B < 0) color2.B = 0;
		else if (color2.B > 31) color2.B = 31;

	}



	out.raw = 0;
	out.r16b.r = color2.R;
	out.r16b.g = color2.G;
	out.r16b.b = color2.B;

	// TODO: Is this something the PROJECTOR should do?
	//if(!(CCBFLAGS&CCB_NOBLK) && out.raw==0) out.raw=1<<10;

	//if(!(PRE1&PRE1_NOSWAP) && (CCBCTL0&(1<<27)))
	//out.raw=(out.raw&0x7ffe)|((out.raw&0x8000)>>15)|((out.raw&1)<<15);

	//if(!(CCBCTL0&0x80000000))out.raw=(out.raw&0x7fff)|((CCBCTL0>>15)&0x8000);

	return out.raw;
}



unsigned int * _madam_GetRegs(void)
{
	return mregs;
}

void  DrawPackedCel_New(void)
{
	sf = 100000;
	uint16_t CURPIX, LAMV;

	int lastaddr;
	int xcur = 0, ycur = 0, xvert, yvert, xdown, ydown, hdx, hdy;

	unsigned int start = PDATA;

	nrows = (PRE0 & PRE0_VCNT_MASK) >> PRE0_VCNT_SHIFT;

	bpp = BPP[PRE0 & PRE0_BPP_MASK];
	offsetl = 2;

	if (bpp < 8)
		offsetl = 1;

	pixcount = 0;

	compsize = 30;

	SPRHI = nrows + 1;
	calcx = 0;

	if (TestInitVisual(1))
		return;
	xvert = XPOS1616;
	yvert = YPOS1616;

	if (TEXEL_FUN_NUMBER == 0) {
		//return;
		for (currentrow = 0; currentrow < (TEXTURE_HI_LIM); currentrow++) {
			int scipw, wcnt;

			BitReaderBig_AttachBuffer(&bitoper, start);
			offset = BitReaderBig_Read(&bitoper, offsetl << 3);

			//BITCALC=((offset+2)<<2)<<5;
			lastaddr = start + ((offset + 2) << 2);
			eor = 0;
			xcur = xvert;
			ycur = yvert;
			xvert += VDX1616;
			yvert += VDY1616;

			if (TEXTURE_HI_START) {
				TEXTURE_HI_START--;
				start = lastaddr;
				continue;
			}
			scipw = TEXTURE_WI_START;
			wcnt  = scipw;

			while (!eor) {//while not end of row

				const int header = BitReaderBig_Read(&bitoper, 8);
				type = (header >> 6) & 3;
				if ( (int)(bitoper.point + start) >= (lastaddr)) type = 0;
				pixcount = (header & 63) + 1;

				if (scipw) {
					if (type == 0) break;
					if (scipw >= (int)(pixcount)) {
						scipw -= (pixcount);
						if (HDX1616) xcur += HDX1616 * (pixcount);
						if (HDY1616) ycur += HDY1616 * (pixcount);
						if (type == 1)
							BitReaderBig_Skip(&bitoper, bpp * pixcount);
						else if (type == 3)
							BitReaderBig_Skip(&bitoper, bpp);
						continue;
					} else {
						if (HDX1616) xcur += HDX1616 * (scipw);
						if (HDY1616) ycur += HDY1616 * (scipw);
						pixcount -= scipw;
						if (type == 1)
							BitReaderBig_Skip(&bitoper, bpp * scipw);
						scipw = 0;
					}
				}
				//if(wcnt>=TEXTURE_WI_LIM)break;
				wcnt += (pixcount);
				if (wcnt > TEXTURE_WI_LIM) {
					pixcount -= (wcnt - TEXTURE_WI_LIM);
					//if(pixcount>>31)break;
				}
				switch (type) {
				case 0: //end of row
					eor = 1;
					break;
				case 1: //PACK_LITERAL
					TexelDraw_BitmapRow(LAMV, xcur, ycur, pixcount);
					if (HDX1616) xcur += HDX1616 * (pixcount);
					if (HDY1616) ycur += HDY1616 * (pixcount);

					break;
				case 2: //PACK_TRANSPARENT
					//	calcx+=(pixcount+1);
					if (HDX1616) xcur += HDX1616 * (pixcount);
					if (HDY1616) ycur += HDY1616 * (pixcount);

					break;
				case 3: //PACK_REPEAT
					CURPIX = PDEC(BitReaderBig_Read(&bitoper, bpp), &LAMV);
					if (CURPIX > 32300 && CURPIX < 33500 && (CURPIX > 32760 || CURPIX < 32750)) {
						if (speedfixes >= 0 && sdf == 0 && speedfixes <= 200001 && unknownflag11 == 0) speedfixes = 200000;
					}
					if (unknownflag11 > 0 && sdf == 0 && CURPIX < 30000 && CURPIX > 29000) speedfixes = -200000;
					if (!pproj.Transparent) {

						TexelDraw_Line(CURPIX, LAMV, xcur, ycur, (pixcount));

					}
					if (HDX1616) xcur += HDX1616 * (pixcount);
					if (HDY1616) ycur += HDY1616 * (pixcount);

					break;
				}       //type
				if (wcnt >= TEXTURE_WI_LIM) break;
			}               //eor

			start = lastaddr;

		}
	} else if (TEXEL_FUN_NUMBER == 1) {
		int drawHeight;

		unknownflag11 = 100000;

		drawHeight = VDY1616;
		if (CCBFLAGS & CCB_MARIA && drawHeight > (1 << 16))
			drawHeight = (1 << 16);

		for (currentrow = 0; currentrow < SPRHI; currentrow++) {

			BitReaderBig_AttachBuffer(&bitoper, start);
			offset = BitReaderBig_Read(&bitoper, offsetl << 3);

			BITCALC = ((offset + 2) << 2) << 5;
			lastaddr = start + ((offset + 2) << 2);

			eor = 0;

			xcur = xvert;
			ycur = yvert;
			xvert += VDX1616;
			yvert += VDY1616;

			while (!eor) {//while not end of row

				const int header = BitReaderBig_Read(&bitoper, 8);
				type = (header >> 6) & 3;
				if ( (int)(bitoper.point + start) >= (lastaddr)) type = 0;
				pixcount = (header & 63) + 1;

				switch (type) {
				case 0: //end of row
					eor = 1;
					break;
				case 1: //PACK_LITERAL
					while (pixcount) {
						pixcount--;
						CURPIX = PDEC(BitReaderBig_Read(&bitoper, bpp), &LAMV);

						if (!pproj.Transparent) {
							if (TexelDraw_Scale(CURPIX, LAMV, xcur >> 16, ycur >> 16, (xcur + (HDX1616 + VDX1616)) >> 16, (ycur + (HDY1616 + drawHeight)) >> 16))
								break;
						}
						xcur += HDX1616;
						ycur += HDY1616;

					}

					break;
				case 2: //PACK_TRANSPARENT
					//	calcx+=(pixcount+1);
					xcur += HDX1616 * (pixcount);
					ycur += HDY1616 * (pixcount);
					pixcount = 0;

					break;
				case 3: //PACK_REPEAT
					CURPIX = PDEC(BitReaderBig_Read(&bitoper, bpp), &LAMV);
					if (!pproj.Transparent) {

						if (TexelDraw_Scale(CURPIX, LAMV, xcur >> 16, ycur >> 16, (xcur + (HDX1616 * (pixcount)) + VDX1616) >> 16, (ycur + (HDY1616 * (pixcount)) + drawHeight) >> 16)) break;

					}
					xcur += HDX1616 * (pixcount);
					ycur += HDY1616 * (pixcount);
					pixcount = 0;
					break;
				}       //type
				if (pixcount) break;
			}               //eor


			start = lastaddr;


		}

	} else {
		if (speedfixes >= 0 && speedfixes <= 100001) speedfixes = 100000;
		for (currentrow = 0; currentrow < SPRHI; currentrow++) {

			BitReaderBig_AttachBuffer(&bitoper, start);
			offset = BitReaderBig_Read(&bitoper, offsetl << 3);

			BITCALC = ((offset + 2) << 2) << 5;
			lastaddr = start + ((offset + 2) << 2);

			eor = 0;

			xcur = xvert;
			ycur = yvert;
			hdx = HDX1616;
			hdy = HDY1616;

			xvert += VDX1616;
			yvert += VDY1616;
			HDX1616 += HDDX1616;
			HDY1616 += HDDY1616;


			xdown = xvert;
			ydown = yvert;

			while (!eor) {//while not end of row

				const int header = BitReaderBig_Read(&bitoper, 8);
				type = (header >> 6) & 3;
				if ( (int)(bitoper.point + start) >= (lastaddr)) type = 0;
				pixcount = (header & 63) + 1;

				switch (type) {
				case 0: //end of row
					eor = 1;
					break;
				case 1: //PACK_LITERAL

					while (pixcount) {
						CURPIX = PDEC(BitReaderBig_Read(&bitoper, bpp), &LAMV);
						pixcount--;
						//   if(speedfixes>=0&&speedfixes<=100001) speedfixes=300000;
						if (!pproj.Transparent) {
							if (TexelDraw_Arbitrary(CURPIX, LAMV, xcur, ycur, xcur + hdx, ycur + hdy, xdown + HDX1616, ydown + HDY1616, xdown, ydown))
								break;
						}
						xcur += hdx;
						ycur += hdy;
						xdown += HDX1616;
						ydown += HDY1616;
					}
					//pixcount=0;
					break;
				case 2: //PACK_TRANSPARENT
					if (speedfixes >= 0 && sdf > 0 /*&&speedfixes<=100001*/) speedfixes = 300000;
					//	calcx+=(pixcount+1);
					xcur += hdx * (pixcount);
					ycur += hdy * (pixcount);
					xdown += HDX1616 * (pixcount);
					ydown += HDY1616 * (pixcount);
					pixcount = 0;

					break;
				case 3:                                                                                                                                                                 //PACK_REPEAT
					CURPIX = PDEC(BitReaderBig_Read(&bitoper, bpp), &LAMV);
					if (speedfixes >= 0 && speedfixes < 200001 && ((CURPIX > 10000 && CURPIX < 11000) && sdf == 0 /*||(CURPIX>10500&&CURPIX<10650)*/)) speedfixes = 200000;         //(CURPIX>10450&&CURPIX<10470)
					if (!pproj.Transparent) {
						while (pixcount) {
							pixcount--;
							if (TexelDraw_Arbitrary(CURPIX, LAMV, xcur, ycur, xcur + hdx, ycur + hdy, xdown + HDX1616, ydown + HDY1616, xdown, ydown))
								break;
							xcur += hdx;
							ycur += hdy;
							xdown += HDX1616;
							ydown += HDY1616;
						}
					} else {
						xcur += hdx * pixcount;
						ycur += hdy * pixcount;
						xdown += HDX1616 * pixcount;
						ydown += HDY1616 * pixcount;
						pixcount = 0;
					}
					//pixcount=0;

					break;
				}
				;//type
				if (pixcount) break;
			}//eor

			start = lastaddr;

		}
	}
	SPRWI++;

	if (fixmode & FIX_BIT_GRAPHICS_STEP_Y) {
		YPOS1616 = ycur;
	} else {
		XPOS1616 = xcur;
	}
}

void  DrawLiteralCel_New(void)
{
	sf = 100000;
	int xcur = 0, ycur = 0, xvert, yvert, xdown, ydown, hdx, hdy;
	uint16_t CURPIX, LAMV;

	bpp = BPP[PRE0 & PRE0_BPP_MASK];
	offsetl = 2;
	if (bpp < 8)
		offsetl = 1;
	pixcount = 0;
	offset = (offsetl == 1) ? ((PRE1 & PRE1_WOFFSET8_MASK) >> PRE1_WOFFSET8_SHIFT) : ((PRE1 & PRE1_WOFFSET10_MASK) >> PRE1_WOFFSET10_SHIFT);


	SPRWI = 1 + (PRE1 & PRE1_TLHPCNT_MASK);
	SPRHI = ((PRE0 & PRE0_VCNT_MASK) >> PRE0_VCNT_SHIFT) + 1;

	if (TestInitVisual(0))
		return;
	xvert = XPOS1616;
	yvert = YPOS1616;

	switch (TEXEL_FUN_NUMBER) {
	case 0:
	{
		int i;

		//  if(speedfixes>=0&&speedfixes<=100001)   speedfixes=300000;
		sdf = 100000;
		//������ NFS
		SPRWI -= ((PRE0 >> 24) & 0xf);
		xvert += TEXTURE_HI_START * VDX1616;
		yvert += TEXTURE_HI_START * VDY1616;
		PDATA += ((offset + 2) << 2) * TEXTURE_HI_START;
		if (SPRWI > TEXTURE_WI_LIM) SPRWI = TEXTURE_WI_LIM;
		for (i = TEXTURE_HI_START; i < TEXTURE_HI_LIM; i++) {

			BitReaderBig_AttachBuffer(&bitoper, PDATA);
			BITCALC = ((offset + 2) << 2) << 5;
			xcur = xvert + TEXTURE_WI_START * HDX1616;
			ycur = yvert + TEXTURE_WI_START * HDY1616;
			BitReaderBig_Skip(&bitoper, bpp * (((PRE0 >> 24) & 0xf)));
			if (TEXTURE_WI_START)
				BitReaderBig_Skip(&bitoper, bpp * (TEXTURE_WI_START));

			xvert += VDX1616;
			yvert += VDY1616;

			TexelDraw_BitmapRow(LAMV, xcur, ycur, SPRWI - TEXTURE_WI_START);

			PDATA += (offset + 2) << 2;

		}
	}
	break;
	case 1:
	{
		int drawHeight;
		int i, j;

		SPRWI -= ((PRE0 >> 24) & 0xf);

		drawHeight = VDY1616;
		if (CCBFLAGS & CCB_MARIA && drawHeight > (1 << 16))
			drawHeight = (1 << 16);

		for (i = 0; i < SPRHI; i++) {

			BitReaderBig_AttachBuffer(&bitoper, PDATA);
			BITCALC = ((offset + 2) << 2) << 5;
			xcur = xvert;
			ycur = yvert;
			xvert += VDX1616;
			yvert += VDY1616;
			BitReaderBig_Skip(&bitoper, bpp * (((PRE0 >> 24) & 0xf)));


			for (j = 0; j < SPRWI; j++) {

				CURPIX = PDEC(BitReaderBig_Read(&bitoper, bpp), &LAMV);


				if (!pproj.Transparent) {
					if (TexelDraw_Scale(CURPIX, LAMV, xcur >> 16, ycur >> 16, (xcur + HDX1616 + VDX1616) >> 16, (ycur + HDY1616 + drawHeight) >> 16)) break;

				}
				xcur += HDX1616;
				ycur += HDY1616;

			}
			PDATA += (offset + 2) << 2;

		}
	}
	break;
	default:
	{
		int i, j;

		SPRWI -= ((PRE0 >> 24) & 0xf);
		for (i = 0; i < SPRHI; i++) {
			BitReaderBig_AttachBuffer(&bitoper, PDATA);
			BITCALC = ((offset + 2) << 2) << 5;

			xcur = xvert;
			ycur = yvert;
			hdx = HDX1616;
			hdy = HDY1616;

			xvert += VDX1616;
			yvert += VDY1616;
			HDX1616 += HDDX1616;
			HDY1616 += HDDY1616;

			BitReaderBig_Skip(&bitoper, bpp * (((PRE0 >> 24) & 0xf)));


			xdown = xvert;
			ydown = yvert;

			for (j = 0; j < SPRWI; j++) {

				CURPIX = PDEC(BitReaderBig_Read(&bitoper, bpp), &LAMV);

				if (!pproj.Transparent) {
					if (TexelDraw_Arbitrary(CURPIX, LAMV, xcur, ycur, xcur + hdx, ycur + hdy, xdown + HDX1616, ydown + HDY1616, xdown, ydown))
						break;
					if (speedfixes < 1 || (speedfixes >= 0 && speedfixes < 200001)) {
						if (CURPIX > 30000 && CURPIX < 40000) speedfixes = 0;
						else speedfixes = -100000;
					}
				}
				xcur += hdx;
				ycur += hdy;
				xdown += HDX1616;
				ydown += HDY1616;
			}
			PDATA += (((offset + 2) << 2) /*scipstr*/);


		}
	}
	break;
	}

	if (fixmode & FIX_BIT_GRAPHICS_STEP_Y) {
		YPOS1616 = ycur;
	} else {
		XPOS1616 = xcur;
	}
}

void  DrawLRCel_New(void)
{
	sf = 100000;
	int x, y, xcur = 0, ycur = 0, xvert, yvert, xdown, ydown, hdx, hdy;
	uint16_t CURPIX, LAMV = 0x49;


	bpp = BPP[PRE0 & PRE0_BPP_MASK];
	offsetl = 2;   if (bpp < 8) offsetl = 1;
	pixcount = 0;
	offset = (offsetl == 1) ? ((PRE1 & PRE1_WOFFSET8_MASK) >> PRE1_WOFFSET8_SHIFT) : ((PRE1 & PRE1_WOFFSET10_MASK) >> PRE1_WOFFSET10_SHIFT);
	offset += 2;
	offset <<= 2;

	SPRWI = 1 + (PRE1 & PRE1_TLHPCNT_MASK);
	SPRHI = (((PRE0 & PRE0_VCNT_MASK) >> PRE0_VCNT_SHIFT) << 1) + 2; //doom fix

	if (TestInitVisual(0)) return;
	xvert = XPOS1616;
	yvert = YPOS1616;

	switch (TEXEL_FUN_NUMBER) {
	unsigned pixel, framePixel;
	case 0:
		xvert += TEXTURE_HI_START * VDX1616;
		yvert += TEXTURE_HI_START * VDY1616;
		//if(SPRHI>TEXTURE_HI_LIM)SPRHI=TEXTURE_HI_LIM;
		if (SPRWI > TEXTURE_WI_LIM) SPRWI = TEXTURE_WI_LIM;
		for (y = TEXTURE_HI_START; y < TEXTURE_HI_LIM; y++) {
			xcur = xvert + TEXTURE_WI_START * HDX1616;
			ycur = yvert + TEXTURE_WI_START * HDY1616;
			xvert += VDX1616;
			yvert += VDY1616;

			int xp = xcur >> 16;
			int yp = ycur >> 16;
			const int hdx = HDX1616 >> 16;
			const int hdy = HDY1616 >> 16;
			for (x = TEXTURE_WI_START; x < SPRWI; x++, xp+=hdx, yp+=hdy) {
				CURPIX = readPixelLR(PDATA, x, y, offset);
				pproj.Transparent = ( ((CURPIX & 0x7fff) == 0x0) & pdec.tmask );
				if (!pproj.Transparent) {
					pixel = CURPIX;
					if (celNeedsFramePixel) framePixel = readFramebufferPixel(PIXSOURCE, xp, yp);
					if (celNeedsPPROC) pixel = PPROC(CURPIX, framePixel, LAMV);
					if (celNeedsPPROJ) pixel = PPROJ_OUTPUT(CURPIX, pixel, framePixel);
					writeFramebufferPixel(FBTARGET, xp, yp, pixel);
				}
			}

		}
		break;
	case 1:
	{
		int drawHeight;
		drawHeight = VDY1616;
		if (CCBFLAGS & CCB_MARIA && drawHeight > (1 << 16))
			drawHeight = (1 << 16);

		for (y = 0; y < SPRHI; y++) {
			xcur = xvert;
			ycur = yvert;
			xvert += VDX1616;
			yvert += VDY1616;


			for (x = 0; x < SPRWI; x++) {

				CURPIX = readPixelLR(PDATA, x, y, offset);
				pproj.Transparent = ( ((CURPIX & 0x7fff) == 0x0) & pdec.tmask );
				if (!pproj.Transparent) {

					if (TexelDraw_Scale(CURPIX, LAMV, xcur >> 16, ycur >> 16, (xcur + HDX1616 + VDX1616) >> 16, (ycur + HDY1616 + drawHeight) >> 16))
						break;

				}
				xcur += HDX1616;
				ycur += HDY1616;


			}

		}
	}
	break;
	default:
		for (y = 0; y < SPRHI; y++) {

			xcur = xvert;
			ycur = yvert;
			xvert += VDX1616;
			yvert += VDY1616;
			xdown = xvert;
			ydown = yvert;
			hdx = HDX1616;
			hdy = HDY1616;
			HDX1616 += HDDX1616;
			HDY1616 += HDDY1616;


			for (x = 0; x < SPRWI; x++) {
				CURPIX = readPixelLR(PDATA, x, y, offset);
				pproj.Transparent = ( ((CURPIX & 0x7fff) == 0x0) & pdec.tmask );
				if (!pproj.Transparent) {
					if (TexelDraw_Arbitrary(CURPIX, LAMV, xcur, ycur, xcur + hdx, ycur + hdy, xdown + HDX1616, ydown + HDY1616, xdown, ydown))
						break;
				}

				xcur += hdx;
				ycur += hdy;
				xdown += HDX1616;
				ydown += HDY1616;


			}


		}
		break;
	}

	if (fixmode & FIX_BIT_GRAPHICS_STEP_Y) {
		YPOS1616 = ycur;
	} else {
		XPOS1616 = xcur;
	}
}

unsigned int _madam_GetCELCycles(void)
{
	unsigned int val = CELCYCLES; // 1 word = 2 CELCYCLES, 1 hword= 1 CELCYCLE, 8 CELCYCLE=1 CPU SCYCLE

	CELCYCLES = 0;
	return val;
}


void _madam_Reset(void)
{
	unsigned i;

	for (i = 0; i < 2048; i++)
		mregs[i] = 0;
}


void _madam_SetMapping(unsigned int flag)
{
	MAPPING = flag;
}



#define ROAN_SHIFT 16
#define ROAN_TYPE int

#include <math.h>

static INLINE unsigned int TexelCCWTest(int64_t hdx, int64_t hdy, int64_t vdx, int64_t vdy)
{
	if (((hdx + vdx) * (hdy - vdy) + vdx * vdy - hdx * hdy) < 0)
		return CCB_ACCW;
	return CCB_ACW;
}

bool QuadCCWTest(int wdt)
{
	unsigned int tmp;

	if (((CCBFLAGS & CCB_ACCW)) && ((CCBFLAGS & CCB_ACW)))
		return false;

	tmp = TexelCCWTest(HDX1616, HDY1616, VDX1616, VDY1616);
	if (tmp != TexelCCWTest(HDX1616, HDY1616, VDX1616 + HDDX1616*wdt, VDY1616 + HDDY1616*wdt))
		return false;
	if (tmp != TexelCCWTest(HDX1616 + HDDX1616*SPRHI, HDY1616 + HDDY1616*SPRHI, VDX1616, VDY1616))
		return false;
	if (tmp != TexelCCWTest(HDX1616 + HDDX1616*SPRHI, HDY1616 + HDDY1616*SPRHI, VDX1616 + HDDX1616*SPRHI * wdt, VDY1616 + HDDY1616*SPRHI * wdt))
		return false;
	if (tmp == (CCBFLAGS & (CCB_ACCW | CCB_ACW)))
		return true;
	return false;
}

static INLINE int __abs(int val)
{
	if (val > 0)
		return val;
	return -val;
}

int TestInitVisual(int packed)
{
	int xpoints[4], ypoints[4];

	if ((!(CCBFLAGS & CCB_ACCW)) && (!(CCBFLAGS & CCB_ACW)))
		return -1;

	if (!packed) {
		xpoints[0] = XPOS1616 >> 16;
		xpoints[1] = (XPOS1616 + HDX1616 * SPRWI) >> 16;
		xpoints[2] = (XPOS1616 + VDX1616 * SPRHI) >> 16;
		xpoints[3] = (XPOS1616 + VDX1616 * SPRHI +
			      (HDX1616 + HDDX1616 * SPRHI) * SPRWI) >> 16;
		if (xpoints[0] < 0 && xpoints[1] < 0 && xpoints[2] < 0 && xpoints[3] < 0) return -1;
		if (xpoints[0] > CLIPXVAL && xpoints[1] > CLIPXVAL && xpoints[2] > CLIPXVAL && xpoints[3] > CLIPXVAL) return -1;


		ypoints[0] = YPOS1616 >> 16;
		ypoints[1] = (YPOS1616 + HDY1616 * SPRWI) >> 16;
		ypoints[2] = (YPOS1616 + VDY1616 * SPRHI) >> 16;
		ypoints[3] = (YPOS1616 + VDY1616 * SPRHI +
			      (HDY1616 + HDDY1616 * SPRHI) * SPRWI) >> 16;
		if (ypoints[0] < 0 && ypoints[1] < 0 && ypoints[2] < 0 && ypoints[3] < 0) return -1;
		if (ypoints[0] > CLIPYVAL && ypoints[1] > CLIPYVAL && ypoints[2] > CLIPYVAL && ypoints[3] > CLIPYVAL) return -1;
	} else {
		xpoints[0] = XPOS1616 >> 16;
		xpoints[1] = (XPOS1616 + VDX1616 * SPRHI) >> 16;
		if ( xpoints[0] < 0 && xpoints[1] < 0 && HDX1616 <= 0 && HDDX1616 <= 0 ) return -1;
		if (xpoints[0] > CLIPXVAL && xpoints[1] > CLIPXVAL && HDX1616 >= 0 && HDDX1616 >= 0 ) return -1;

		ypoints[0] = YPOS1616 >> 16;
		ypoints[1] = (YPOS1616 + VDY1616 * SPRHI) >> 16;
		if (ypoints[0] < 0 && ypoints[1] < 0 && HDY1616 <= 0 && HDDY1616 <= 0 ) return -1;
		if (ypoints[0] > CLIPYVAL && ypoints[1] > CLIPYVAL && HDY1616 >= 0 && HDDY1616 >= 0 ) return -1;
	}

	if (HDDX1616 == 0 && HDDY1616 == 0) {
		if (HDX1616 == 0 && VDY1616 == 0) {
			if ((HDY1616 < 0 && VDX1616 > 0) || (HDY1616 > 0 && VDX1616 < 0)) {
				if ((CCBFLAGS & CCB_ACW)) {
					if (__abs(HDY1616) == 0x10000 && __abs(VDX1616) == 0x10000 && !((YPOS1616 | XPOS1616) & 0xffff)) {
						return Init_Line_Map();
						//return 0;
					} else {
						Init_Scale_Map();
						return 0;
					}
				}
			} else {
				if ((CCBFLAGS & CCB_ACCW)) {
					if (__abs(HDY1616) == 0x10000 && __abs(VDX1616) == 0x10000 && !((YPOS1616 | XPOS1616) & 0xffff)) {
						return Init_Line_Map();
						//return 0;
					} else {
						Init_Scale_Map();
						return 0;
					}
				}

			}
			return -1;


		} else if (HDY1616 == 0 && VDX1616 == 0) {

			if ((HDX1616 < 0 && VDY1616 > 0) || (HDX1616 > 0 && VDY1616 < 0)) {
				if ((CCBFLAGS & CCB_ACCW)) {
					if (__abs(HDX1616) == 0x10000 &&    __abs(VDY1616) == 0x10000 && !((YPOS1616 | XPOS1616) & 0xffff)) {
						return Init_Line_Map();
						//return 0;
					} else {
						Init_Scale_Map();
						return 0;
					}
				}
			} else {
				if ((CCBFLAGS & CCB_ACW)) {
					if (__abs(HDX1616) == 0x10000 &&    __abs(VDY1616) == 0x10000 && !((YPOS1616 | XPOS1616) & 0xffff)) {
						return Init_Line_Map();
						//return 0;
					} else {
						Init_Scale_Map();
						return 0;
					}
				}
			}
			return -1;


		}
	}

	if (QuadCCWTest((!packed) ? SPRWI : 2048)) return -1;
	Init_Arbitrary_Map();


	return 0;

}

int Init_Line_Map(void)
{
	TEXEL_FUN_NUMBER = 0;
	TEXTURE_WI_START = 0;
	TEXTURE_HI_START = 0;
	TEXTURE_HI_LIM = SPRHI;
	if (HDX1616 < 0)
		XPOS1616 -= 0x8000;
	else if (VDX1616 < 0)
		XPOS1616 -= 0x8000;

	if (HDY1616 < 0)
		YPOS1616 -= 0x8000;
	else if (VDY1616 < 0)
		YPOS1616 -= 0x8000;

	if (VDX1616 < 0) {
		if ((((XPOS1616)-((SPRHI - 1) << 16)) >> 16) < 0)
			TEXTURE_HI_LIM = (XPOS1616 >> 16) + 1;
		if (TEXTURE_HI_LIM > SPRHI) TEXTURE_HI_LIM = SPRHI;
	} else if (VDX1616 > 0) {
		if (((XPOS1616 + (SPRHI << 16)) >> 16) > CLIPXVAL)
			TEXTURE_HI_LIM = CLIPXVAL - (XPOS1616 >> 16) + 1;
	}
	if (VDY1616 < 0) {
		if ((((YPOS1616)-((SPRHI - 1) << 16)) >> 16) < 0)
			TEXTURE_HI_LIM = (YPOS1616 >> 16) + 1;
		if (TEXTURE_HI_LIM > SPRHI) TEXTURE_HI_LIM = SPRHI;
	} else if (VDY1616 > 0) {
		if (((YPOS1616 + (SPRHI << 16)) >> 16) > CLIPYVAL)
			TEXTURE_HI_LIM = CLIPYVAL - (YPOS1616 >> 16) + 1;
	}

	if (HDX1616 < 0)
		TEXTURE_WI_LIM = (XPOS1616 >> 16) + 1;
	else if (HDX1616 > 0)
		TEXTURE_WI_LIM = CLIPXVAL - (XPOS1616 >> 16) + 1;

	if (HDY1616 < 0)
		TEXTURE_WI_LIM = (YPOS1616 >> 16) + 1;
	else if (HDY1616 > 0)
		TEXTURE_WI_LIM = CLIPYVAL - (YPOS1616 >> 16) + 1;


	if (XPOS1616 < 0) {
		if (HDX1616 < 0) return -1;
		else if (HDX1616 > 0)
			TEXTURE_WI_START = -(XPOS1616 >> 16);

		if (VDX1616 < 0) return -1;
		else if (VDX1616 > 0)
			TEXTURE_HI_START = -(XPOS1616 >> 16);
	} else if ((XPOS1616 >> 16) > CLIPXVAL) {
		if (HDX1616 > 0) return -1;
		else if (HDX1616 < 0)
			TEXTURE_WI_START = (XPOS1616 >> 16) - CLIPXVAL;

		if (VDX1616 > 0) return -1;
		else if (VDX1616 < 0)
			TEXTURE_HI_START = (XPOS1616 >> 16) - CLIPXVAL;

	}
	if (YPOS1616 < 0) {
		if (HDY1616 < 0) return -1;
		else if (HDY1616 > 0)
			TEXTURE_WI_START = -(YPOS1616 >> 16);

		if (VDY1616 < 0) return -1;
		else if (VDY1616 > 0)
			TEXTURE_HI_START = -(YPOS1616 >> 16);
	} else if ((YPOS1616 >> 16) > CLIPYVAL) {
		if (HDY1616 > 0) return -1;
		else if (HDY1616 < 0)
			TEXTURE_WI_START = (YPOS1616 >> 16) - CLIPYVAL;

		if (VDY1616 > 0) return -1;
		else if (VDY1616 < 0)
			TEXTURE_HI_START = (YPOS1616 >> 16) - CLIPYVAL;
	}
	//if(TEXTURE_WI_START<((PRE0>>24)&0xf))
	//        TEXTURE_WI_START=((PRE0>>24)&0xf);
	//TEXTURE_WI_START+=(PRE0>>24)&0xf;
	//if(TEXTURE_WI_START<0)TEXTURE_WI_START=0;
	//if(TEXTURE_HI_START<0)TEXTURE_HI_START=0;
	//if(TEXTURE_HI_LIM>SPRHI)TEXTURE_HI_LIM=SPRHI;
	if (TEXTURE_WI_LIM <= 0) return -1;
	return 0;
}

void Init_Scale_Map(void)
{
	int deltax, deltay;

	TEXEL_FUN_NUMBER = 1;
	if (HDX1616 < 0)
		XPOS1616 -= 0x8000;
	else if (VDX1616 < 0)
		XPOS1616 -= 0x8000;

	if (HDY1616 < 0)
		YPOS1616 -= 0x8000;
	else if (VDY1616 < 0)
		YPOS1616 -= 0x8000;

	deltax = HDX1616 + VDX1616;
	deltay = HDY1616 + VDY1616;
	if (deltax < 0) TEXEL_INCX = -1;
	else TEXEL_INCX = 1;
	if (deltay < 0) TEXEL_INCY = -1;
	else TEXEL_INCY = 1;

	TEXTURE_WI_START = 0;
	TEXTURE_HI_START = 0;
}

void Init_Arbitrary_Map(void)
{
	TEXEL_FUN_NUMBER = 2;
	TEXTURE_WI_START = 0;
	TEXTURE_HI_START = 0;
}

void TexelDraw_BitmapRow(uint16_t LAMV, int xcur, int ycur, int cnt)
{
	int x;
	unsigned pixel, framePixel = 0;

	int xp = xcur >> 16;
	int yp = ycur >> 16;
	const int hdx = HDX1616 >> 16;
	const int hdy = HDY1616 >> 16;
	for (x = 0; x < cnt; x++, xp += hdx, yp += hdy) {
		uint16_t CURPIX = PDEC(BitReaderBig_Read(&bitoper, bpp), &LAMV);
		if (!pproj.Transparent) {
			pixel = CURPIX;
			if (celNeedsFramePixel) framePixel = readFramebufferPixel(PIXSOURCE, xp, yp);
			if (celNeedsPPROC) pixel = PPROC(CURPIX, framePixel, LAMV);
			if (celNeedsPPROJ) pixel = PPROJ_OUTPUT(CURPIX, pixel, framePixel);
			writeFramebufferPixel(FBTARGET, xp, yp, pixel);
		}
	}
}

void TexelDraw_Line(uint16_t CURPIX, uint16_t LAMV, int xcur, int ycur, int cnt)
{
	int x;
	unsigned int pixel = CURPIX;
	unsigned int nextFramePixel = 0;
	unsigned int currFramePixel = 0xffffffff;

	int xp = xcur >> 16;
	int yp = ycur >> 16;
	const int hdx = HDX1616 >> 16;
	const int hdy = HDY1616 >> 16;
	for (x = 0; x < cnt; x++, xp += hdx, yp += hdy) {
		if (celNeedsFramePixel) nextFramePixel = readFramebufferPixel(PIXSOURCE, xp, yp);
		if (nextFramePixel != currFramePixel) {
			currFramePixel = nextFramePixel;
			if (celNeedsPPROC) pixel = PPROC(CURPIX, nextFramePixel, LAMV);
			if (celNeedsPPROJ) pixel = PPROJ_OUTPUT(CURPIX, pixel, nextFramePixel);
		}
		writeFramebufferPixel(FBTARGET, xp, yp, pixel);
	}
}

int  TexelDraw_Scale(uint16_t CURPIX, uint16_t LAMV, int xcur, int ycur, int deltax, int deltay)
{
	int x, y;
	unsigned int pixel = CURPIX;
	unsigned int nextFramePixel = 0;
	unsigned int currFramePixel = 0xffffffff;

	if ((HDX1616 < 0) && (deltax) < 0 && xcur < 0)
		return -1;
	else if ((HDY1616 < 0) && (deltay) < 0 && ycur < 0 )
		return -1;
	else if ((HDX1616 > 0) && (deltax) > (CLIPXVAL) && (xcur) > (CLIPXVAL))
		return -1;
	else if ((HDY1616 > 0) && ((deltay)) > (CLIPYVAL) && (ycur) > (CLIPYVAL))
		return -1;

	if (xcur == deltax)
		return 0;

	for (y = ycur; y != deltay; y += TEXEL_INCY)
		for (x = xcur; x != deltax; x += TEXEL_INCX)
			if (TESTCLIP(x,y)) {
				if (celNeedsFramePixel) nextFramePixel = readFramebufferPixel(PIXSOURCE, x, y);
				if (nextFramePixel != currFramePixel) {
					currFramePixel = nextFramePixel;
					if (celNeedsPPROC) pixel = PPROC(CURPIX, nextFramePixel, LAMV);
					if (celNeedsPPROJ) pixel = PPROJ_OUTPUT(CURPIX, pixel, nextFramePixel);
				}
				writeFramebufferPixel(FBTARGET, x, y, pixel);
			}

	return 0;
}

int TexelCCWTestSmp(int hdx, int hdy, int vdx, int vdy)
{
	if (((hdx + vdx) * (hdy - vdy) + vdx * vdy - hdx * hdy) < 0)
		return CCB_ACCW;
	return CCB_ACW;
}

int  TexelDraw_Arbitrary(uint16_t CURPIX, uint16_t LAMV,
			 int xA, int yA, int xB, int yB, int xC, int yC, int xD, int yD)
{
	int miny, maxy, x, xpoints[4], y, maxyt, maxxt, maxx;
	int updowns[4], ytmp;
	unsigned int pixel = CURPIX;
	unsigned int nextFramePixel = 0;
	unsigned int currFramePixel = 0xffffffff;

	xA >>= 16;
	xB >>= 16;
	xC >>= 16;
	xD >>= 16;
	yA >>= 16;
	yB >>= 16;
	yC >>= 16;
	yD >>= 16;

	if ((xA) == (xB) && (xB) == (xC) && (xC) == (xD)) return 0;

	maxxt = CLIPXVAL + 1;
	maxyt = CLIPYVAL + 1;

	if (HDX1616 < 0 && HDDX1616 < 0) {
		if ((xA < 0) && (xB < 0) && (xC < 0) && (xD < 0))
			return -1;
	}
	if (HDX1616 > 0 && HDDX1616 > 0) {
		if (((xA) >= maxxt) && ((xB) >= maxxt) && ((xC) >= maxxt) && ((xD) >= maxxt))
			return -1;
	}
	if (HDY1616 < 0 && HDDY1616 < 0) {
		if ((yA < 0) && (yB < 0) && (yC < 0) && (yD < 0))
			return -1;
	}
	if (HDY1616 > 0 && HDDY1616 > 0) {
		if (((yA) >= maxyt) && ((yB) >= maxyt) && ((yC) >= maxyt) && ((yD) >= maxyt))
			return -1;
	}

	miny = maxy = yA;
	if (miny > yB) miny = yB;
	if (miny > yC) miny = yC;
	if (miny > yD) miny = yD;
	if (maxy < yB) maxy = yB;
	if (maxy < yC) maxy = yC;
	if (maxy < yD) maxy = yD;

	y = (miny);
	if (y < 0) y = 0;
	if (maxy < maxyt) maxyt = maxy;


	for (; y < maxyt; y++) {
		int cnt_cross = 0;
		if (y < (yB) && y >= (yA)) {
			xpoints[cnt_cross] = (int)((quickDivide(((xB - xA) * (y - yA)), (yB - yA)) + xA));
			updowns[cnt_cross++] = 1;
		} else if (y >= (yB) && y < (yA)) {
			xpoints[cnt_cross] = (int)((quickDivide(((xA - xB) * (y - yB)), (yA - yB)) + xB));
			updowns[cnt_cross++] = 0;
		}

		if (y < (yC) && y >= (yB)) {
			xpoints[cnt_cross] = (int)((quickDivide(((xC - xB) * (y - yB)), (yC - yB)) + xB));
			updowns[cnt_cross++] = 1;
		} else if (y >= (yC) && y < (yB)) {
			xpoints[cnt_cross] = (int)((quickDivide(((xB - xC) * (y - yC)), (yB - yC)) + xC));
			updowns[cnt_cross++] = 0;
		}

		if (y < (yD) && y >= (yC)) {
			xpoints[cnt_cross] = (int)((quickDivide(((xD - xC) * (y - yC)), (yD - yC)) + xC));
			updowns[cnt_cross++] = 1;
		} else if (y >= (yD) && y < (yC)) {
			xpoints[cnt_cross] = (int)((quickDivide(((xC - xD) * (y - yD)), (yC - yD)) + xD));
			updowns[cnt_cross++] = 0;
		}

		if (cnt_cross & 1) {
			if (y < (yA) && y >= (yD)) {
				xpoints[cnt_cross] = (int)((quickDivide(((xA - xD) * (y - yD)), (yA - yD)) + xD));
				updowns[cnt_cross] = 1;
			} else if (y >= (yA) && y < (yD)) {
				xpoints[cnt_cross] = (int)((quickDivide(((xD - xA) * (y - yA)), (yD - yA)) + xA));
				updowns[cnt_cross] = 0;
			}
		}

		if (cnt_cross != 0) {

			if (xpoints[0] > xpoints[1]) {
				xpoints[1] += xpoints[0];
				xpoints[0] = xpoints[1] - xpoints[0];
				xpoints[1] = xpoints[1] - xpoints[0];

				ytmp = updowns[0];
				updowns[0] = updowns[1];
				updowns[1] = ytmp;
			}
			if (cnt_cross > 2) {
				if ( ((CCBFLAGS & CCB_ACW) && updowns[2] == 0) ||
				     ((CCBFLAGS & CCB_ACCW) && updowns[2] == 1)) {
					x = xpoints[2];
					if (x < 0) x = 0;
					maxx = xpoints[3];
					if (maxx > maxxt) maxx = maxxt;
					for (; x < maxx; x++) {
						if (celNeedsFramePixel) nextFramePixel = readFramebufferPixel(PIXSOURCE, x, y);
						if (nextFramePixel != currFramePixel) {
							currFramePixel = nextFramePixel;
							if (celNeedsPPROC) pixel = PPROC(CURPIX, nextFramePixel, LAMV);
							if (celNeedsPPROJ) pixel = PPROJ_OUTPUT(CURPIX, pixel, nextFramePixel);
						}
						writeFramebufferPixel(FBTARGET, x, y, pixel);
					}
				}

			}

			if ( ((CCBFLAGS & CCB_ACW) && updowns[0] == 0) ||
			     ((CCBFLAGS & CCB_ACCW) && updowns[0] == 1)) {
				x = xpoints[0];
				if (x < 0) x = 0;
				maxx = xpoints[1];
				if (maxx > maxxt) maxx = maxxt;

				for (; x < maxx; x++) {
					if (celNeedsFramePixel) nextFramePixel = readFramebufferPixel(PIXSOURCE, x, y);
					if (nextFramePixel != currFramePixel) {
						currFramePixel = nextFramePixel;
						if (celNeedsPPROC) pixel = PPROC(CURPIX, nextFramePixel, LAMV);
						if (celNeedsPPROJ) pixel = PPROJ_OUTPUT(CURPIX, pixel, nextFramePixel);
					}
					writeFramebufferPixel(FBTARGET, x, y, pixel);
				}
			}
		}
	}
	return 0;
}
