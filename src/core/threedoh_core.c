#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <direct.h>
#endif

#include "threedoh_core.h"
#include "freedocore.h"
#include "_3do_sys.h"
#include "frame.h"
#include "fs.h"
#include "input.h"
#include "sound.h"
#include "vdlp.h"
#include "quarz.h"
#include "Clio.h"
#include "Madam.h"
#include "DSP.h"
#include "arm.h"

char biosFile[128];
int onsector = 0;

struct threedoh_core {
    struct VDLFrame frame;
    int started;
    int fs_started;
    int iso_started;
    int video_mode;
    int active_video_standard;
    int visible_width;
    int strict_bus_faults;
    int strict_madam_runaway_faults;
    int strict_dsp_resources;
};

#define THREEDOH_NVRAM_SIZE (32u * 1024u)
#define THREEDOH_NVRAM_PATH_MAX 1024
#define THREEDOH_NVRAM_RECORD_TYPE 1
#define THREEDOH_NVRAM_LINKED_MEM_VERSION 2
#define THREEDOH_NVRAM_SYNC_BYTE 'Z'
#define THREEDOH_NVRAM_SYNC_LEN 5
#define THREEDOH_NVRAM_COMMENT_LEN 32
#define THREEDOH_NVRAM_LABEL_LEN 32
#define THREEDOH_NVRAM_ROOT_AVATARS 8
#define THREEDOH_NVRAM_VOLUME_UNIQUE_ID 0xffffffffu
#define THREEDOH_NVRAM_ROOT_UNIQUE_ID 0xfffffffeu
#define THREEDOH_NVRAM_FINGERPRINT_ANCHOR 0x855a02b6u
#define THREEDOH_NVRAM_FINGERPRINT_FREE 0x7aa565bdu

#pragma pack(push,1)
typedef struct {
    uint8_t recordType;
    uint8_t syncBytes[THREEDOH_NVRAM_SYNC_LEN];
    uint8_t recordVersion;
    uint8_t flags;
    uint8_t comment[THREEDOH_NVRAM_COMMENT_LEN];
    uint8_t label[THREEDOH_NVRAM_LABEL_LEN];
    uint32_t id;
    uint32_t blockSize;
    uint32_t blockCount;
    uint32_t rootDirId;
    uint32_t rootDirBlocks;
    uint32_t rootDirBlockSize;
    uint32_t lastRootDirCopy;
    uint32_t rootDirCopies[THREEDOH_NVRAM_ROOT_AVATARS];
} NvRamDiscLabel;

typedef struct {
    uint32_t fingerprint;
    uint32_t flinkoffset;
    uint32_t blinkoffset;
    uint32_t blockcount;
    uint32_t headerblockcount;
} NvRamLinkedMemBlock;
#pragma pack(pop)

extern int isexit;
extern void *Getp_NVRAM(void);

static char g_nvram_path[THREEDOH_NVRAM_PATH_MAX];

static uint32_t ReverseBytes(uint32_t value)
{
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
           (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}


static int nvram_join_path(char *out, size_t out_size, const char *a, const char *b)
{
    size_t la;
    size_t lb;
    int need_sep;
    if (!out || out_size == 0 || !a || !b)
        return 0;
    la = strlen(a);
    lb = strlen(b);
    need_sep = (la > 0 && a[la - 1] != '/' && a[la - 1] != '\\');
    if (la + (need_sep ? 1u : 0u) + lb + 1u > out_size)
        return 0;
    memcpy(out, a, la);
    if (need_sep)
        out[la++] = '/';
    memcpy(out + la, b, lb);
    out[la + lb] = '\0';
    return 1;
}

static int nvram_mkdir_if_needed(const char *path)
{
    if (!path || !*path)
        return 0;
#if defined(_WIN32)
    if (_mkdir(path) == 0 || errno == EEXIST)
        return 1;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return 1;
#endif
    return 0;
}

static const char *nvram_basename(const char *path)
{
    const char *base = path;
    const char *p;
    if (!path)
        return "3do";
    for (p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    return *base ? base : "3do";
}

static void nvram_sanitize_game_name(const char *path, char *out, size_t out_size)
{
    const char *base = nvram_basename(path);
    const char *dot = strrchr(base, '.');
    size_t stop = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    size_t o = 0;
    size_t i;
    if (!out || out_size == 0)
        return;
    for (i = 0; i < stop && o + 1 < out_size; i++) {
        unsigned char c = (unsigned char)base[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            out[o++] = (char)c;
        } else if (c == ' ' || c == '.' || c == '(' || c == ')' || c == '[' || c == ']') {
            if (o > 0 && out[o - 1] != '_')
                out[o++] = '_';
        } else {
            if (o > 0 && out[o - 1] != '_')
                out[o++] = '_';
        }
    }
    while (o > 0 && out[o - 1] == '_')
        o--;
    if (o == 0) {
        const char fallback[] = "3do";
        for (i = 0; fallback[i] && o + 1 < out_size; i++)
            out[o++] = fallback[i];
    }
    out[o] = '\0';
}

static int nvram_configure_path(const char *game_path)
{
    const char *home = getenv("HOME");
    char root[THREEDOH_NVRAM_PATH_MAX];
    char dir[THREEDOH_NVRAM_PATH_MAX];
    char name[256];

#if defined(_WIN32)
    if (!home || !*home)
        home = getenv("USERPROFILE");
#endif
    if (!home || !*home) {
        g_nvram_path[0] = '\0';
        return 0;
    }

    if (!nvram_join_path(root, sizeof(root), home, ".3doh") ||
        !nvram_join_path(dir, sizeof(dir), root, "nvram")) {
        g_nvram_path[0] = '\0';
        return 0;
    }
    if (!nvram_mkdir_if_needed(root) || !nvram_mkdir_if_needed(dir)) {
        g_nvram_path[0] = '\0';
        return 0;
    }

    nvram_sanitize_game_name(game_path, name, sizeof(name));
    if (strlen(name) + 4u >= sizeof(name)) {
        g_nvram_path[0] = '\0';
        return 0;
    }
    strcat(name, ".nvr");
    if (!nvram_join_path(g_nvram_path, sizeof(g_nvram_path), dir, name)) {
        g_nvram_path[0] = '\0';
        return 0;
    }
    return 1;
}

static int nvram_is_initialized(const void *pnvram)
{
    const NvRamDiscLabel *label = (const NvRamDiscLabel *)pnvram;
    int i;
    if (!pnvram)
        return 0;
    if (label->recordType != THREEDOH_NVRAM_RECORD_TYPE)
        return 0;
    if (label->recordVersion != THREEDOH_NVRAM_LINKED_MEM_VERSION)
        return 0;
    for (i = 0; i < THREEDOH_NVRAM_SYNC_LEN; i++) {
        if (label->syncBytes[i] != (uint8_t)THREEDOH_NVRAM_SYNC_BYTE)
            return 0;
    }
    return 1;
}

static void nvram_format(void *pnvram)
{
    NvRamDiscLabel *label;
    NvRamLinkedMemBlock *anchor;
    NvRamLinkedMemBlock *free_block;
    if (!pnvram)
        return;

    memset(pnvram, 0, THREEDOH_NVRAM_SIZE);
    label = (NvRamDiscLabel *)pnvram;
    anchor = (NvRamLinkedMemBlock *)((uint8_t *)pnvram + sizeof(NvRamDiscLabel));
    free_block = (NvRamLinkedMemBlock *)((uint8_t *)anchor + sizeof(NvRamLinkedMemBlock));

    label->recordType = THREEDOH_NVRAM_RECORD_TYPE;
    memset(label->syncBytes, THREEDOH_NVRAM_SYNC_BYTE, sizeof(label->syncBytes));
    label->recordVersion = THREEDOH_NVRAM_LINKED_MEM_VERSION;
    memcpy(label->comment, "3doh formatted", sizeof("3doh formatted") - 1);
    memcpy(label->label, "nvram", sizeof("nvram") - 1);
    label->id = ReverseBytes(THREEDOH_NVRAM_VOLUME_UNIQUE_ID);
    label->blockSize = ReverseBytes(1);
    label->blockCount = ReverseBytes(THREEDOH_NVRAM_SIZE);
    label->rootDirId = ReverseBytes(THREEDOH_NVRAM_ROOT_UNIQUE_ID);
    label->rootDirBlocks = ReverseBytes(0);
    label->rootDirBlockSize = ReverseBytes(1);
    label->lastRootDirCopy = ReverseBytes(0);
    label->rootDirCopies[0] = ReverseBytes(sizeof(NvRamDiscLabel));

    anchor->fingerprint = ReverseBytes(THREEDOH_NVRAM_FINGERPRINT_ANCHOR);
    anchor->flinkoffset = ReverseBytes(sizeof(NvRamDiscLabel) + sizeof(NvRamLinkedMemBlock));
    anchor->blinkoffset = ReverseBytes(sizeof(NvRamDiscLabel) + sizeof(NvRamLinkedMemBlock));
    anchor->blockcount = ReverseBytes(sizeof(NvRamLinkedMemBlock));
    anchor->headerblockcount = ReverseBytes(sizeof(NvRamLinkedMemBlock));

    free_block->fingerprint = ReverseBytes(THREEDOH_NVRAM_FINGERPRINT_FREE);
    free_block->flinkoffset = ReverseBytes(sizeof(NvRamDiscLabel));
    free_block->blinkoffset = ReverseBytes(sizeof(NvRamDiscLabel));
    free_block->blockcount = ReverseBytes(THREEDOH_NVRAM_SIZE - sizeof(NvRamDiscLabel) - sizeof(NvRamLinkedMemBlock));
    free_block->headerblockcount = ReverseBytes(sizeof(NvRamLinkedMemBlock));
}

static int nvram_load_file(void *pnvram)
{
    FILE *fp;
    size_t n;
    if (!pnvram || !g_nvram_path[0])
        return 0;
    fp = fopen(g_nvram_path, "rb");
    if (!fp)
        return 0;
    n = fread(pnvram, 1, THREEDOH_NVRAM_SIZE, fp);
    fclose(fp);
    return n == THREEDOH_NVRAM_SIZE;
}

static int nvram_save_file(const void *pnvram)
{
    FILE *fp;
    size_t n;
    if (!pnvram || !g_nvram_path[0])
        return 0;
    fp = fopen(g_nvram_path, "wb");
    if (!fp)
        return 0;
    n = fwrite(pnvram, 1, THREEDOH_NVRAM_SIZE, fp);
    fclose(fp);
    return n == THREEDOH_NVRAM_SIZE;
}

void readNvRam(void *pnvram)
{
    if (!pnvram)
        return;
    if (!nvram_load_file(pnvram) || !nvram_is_initialized(pnvram)) {
        nvram_format(pnvram);
        nvram_save_file(pnvram);
    }
}

void writeNvRam(void)
{
    void *pnvram = Getp_NVRAM();
    if (pnvram)
        nvram_save_file(pnvram);
}

size_t threedoh_core_size(void)
{
    return sizeof(threedoh_core);
}

void threedoh_core_construct(threedoh_core *core)
{
    if (core) {
        memset(core, 0, sizeof(*core));
        core->video_mode = THREEDOH_VIDEO_AUTO;
        core->active_video_standard = THREEDOH_VIDEO_NTSC;
        core->visible_width = THREEDOH_SCREEN_WIDTH;
        core->strict_bus_faults = 1;
        core->strict_madam_runaway_faults = 0;
        core->strict_dsp_resources = 1;
    }
}


static int ascii_contains_token(const unsigned char *data, size_t size, const char *token)
{
    size_t token_len;
    size_t i;
    if (!data || !size || !token || !*token)
        return 0;
    token_len = strlen(token);
    if (token_len > size)
        return 0;
    for (i = 0; i + token_len <= size; i++) {
        size_t j;
        for (j = 0; j < token_len; j++) {
            unsigned char c = data[i + j];
            if (c >= 'A' && c <= 'Z')
                c = (unsigned char)(c - 'A' + 'a');
            if (c != (unsigned char)token[j])
                break;
        }
        if (j == token_len)
            return 1;
    }
    return 0;
}

static int path_contains_token(const char *path, const char *token)
{
    return path ? ascii_contains_token((const unsigned char *)path, strlen(path), token) : 0;
}

#ifndef THREEDOH_WASM
static int file_contains_token(const char *path, const char *token)
{
    FILE *fp;
    unsigned char buffer[4096];
    size_t n;
    size_t scanned = 0;
    if (!path || !*path)
        return 0;
    fp = fopen(path, "rb");
    if (!fp)
        return 0;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0 && scanned < (4U * 1024U * 1024U)) {
        if (ascii_contains_token(buffer, n, token)) {
            fclose(fp);
            return 1;
        }
        scanned += n;
    }
    fclose(fp);
    return 0;
}
#else
static int file_contains_token(const char *path, const char *token)
{
    (void)path;
    (void)token;
    return 0;
}
#endif

static int detect_video_standard(const char *bios_path, const char *iso_path)
{
    int iso_path_pal_hint;
    int iso_path_ntsc_hint;
    int iso_file_pal_hint;
    int iso_file_ntsc_hint;
    int bios_pal_hint;
    int bios_ntsc_hint;

    iso_path_pal_hint = path_contains_token(iso_path, "pal") ||
                        path_contains_token(iso_path, "europe") ||
                        path_contains_token(iso_path, "euro");
    iso_path_ntsc_hint = path_contains_token(iso_path, "ntsc") ||
                         path_contains_token(iso_path, "usa") ||
                         path_contains_token(iso_path, "japan") ||
                         path_contains_token(iso_path, "jpn");
    if (iso_path_pal_hint && !iso_path_ntsc_hint)
        return THREEDOH_VIDEO_PAL;
    if (iso_path_ntsc_hint && !iso_path_pal_hint)
        return THREEDOH_VIDEO_NTSC;

    iso_file_pal_hint = file_contains_token(iso_path, "pal ") ||
                        file_contains_token(iso_path, "pal-") ||
                        file_contains_token(iso_path, "pal_") ||
                        file_contains_token(iso_path, "europe");
    iso_file_ntsc_hint = file_contains_token(iso_path, "ntsc") ||
                         file_contains_token(iso_path, "japan") ||
                         file_contains_token(iso_path, "usa");
    if (iso_file_pal_hint && !iso_file_ntsc_hint)
        return THREEDOH_VIDEO_PAL;
    if (iso_file_ntsc_hint && !iso_file_pal_hint)
        return THREEDOH_VIDEO_NTSC;

    bios_pal_hint = path_contains_token(bios_path, "pal") ||
                    path_contains_token(bios_path, "europe") ||
                    path_contains_token(bios_path, "euro") ||
                    file_contains_token(bios_path, "pal ") ||
                    file_contains_token(bios_path, "pal-") ||
                    file_contains_token(bios_path, "pal_") ||
                    file_contains_token(bios_path, "europe");
    bios_ntsc_hint = path_contains_token(bios_path, "ntsc") ||
                     path_contains_token(bios_path, "usa") ||
                     path_contains_token(bios_path, "japan") ||
                     path_contains_token(bios_path, "jpn") ||
                     file_contains_token(bios_path, "ntsc") ||
                     file_contains_token(bios_path, "japan") ||
                     file_contains_token(bios_path, "usa");
    if (bios_pal_hint && !bios_ntsc_hint)
        return THREEDOH_VIDEO_PAL;
    return THREEDOH_VIDEO_NTSC;
}

static int resolve_video_standard(threedoh_core *core, const char *bios_path, const char *iso_path)
{
    int mode = core ? core->video_mode : THREEDOH_VIDEO_AUTO;
    if (mode == THREEDOH_VIDEO_PAL || mode == THREEDOH_VIDEO_NTSC)
        return mode;
    return detect_video_standard(bios_path, iso_path);
}

static int visible_height_for_standard(int standard)
{
    return standard == THREEDOH_VIDEO_PAL ? THREEDOH_PAL1_SCREEN_HEIGHT : THREEDOH_SCREEN_HEIGHT;
}

static void apply_video_standard(threedoh_core *core, int standard)
{
    if (!core)
        return;
    if (standard != THREEDOH_VIDEO_PAL)
        standard = THREEDOH_VIDEO_NTSC;
    core->active_video_standard = standard;
    core->visible_width = THREEDOH_SCREEN_WIDTH;
    _qrz_SetVideoStandard(standard == THREEDOH_VIDEO_PAL);
    _clio_SetVideoStandard(standard == THREEDOH_VIDEO_PAL);
    _vdl_SetVisibleHeight((uint32_t)visible_height_for_standard(standard));
}

void threedoh_core_set_video_standard_mode(threedoh_core *core, int mode)
{
    if (!core)
        return;
    if (mode != THREEDOH_VIDEO_AUTO && mode != THREEDOH_VIDEO_NTSC && mode != THREEDOH_VIDEO_PAL)
        mode = THREEDOH_VIDEO_AUTO;
    core->video_mode = mode;
    if (mode == THREEDOH_VIDEO_NTSC || mode == THREEDOH_VIDEO_PAL)
        apply_video_standard(core, mode);
}

int threedoh_core_apply_video_standard_mode(threedoh_core *core, const char *bios_path, const char *iso_path)
{
    if (!core)
        return THREEDOH_VIDEO_NTSC;
    apply_video_standard(core, resolve_video_standard(core, bios_path, iso_path));
    return core->active_video_standard;
}

int threedoh_core_video_standard_mode(const threedoh_core *core)
{
    return core ? core->video_mode : THREEDOH_VIDEO_AUTO;
}

int threedoh_core_active_video_standard(const threedoh_core *core)
{
    return core ? core->active_video_standard : THREEDOH_VIDEO_NTSC;
}

int threedoh_core_frame_rate_hz(const threedoh_core *core)
{
    return threedoh_core_active_video_standard(core) == THREEDOH_VIDEO_PAL ? 50 : 60;
}

const char *threedoh_core_video_standard_name(const threedoh_core *core)
{
    if (threedoh_core_active_video_standard(core) != THREEDOH_VIDEO_PAL)
        return "NTSC 60 Hz / 320x240";
    return threedoh_core_visible_width(core) > THREEDOH_SCREEN_WIDTH ?
           "PAL2 50 Hz / 384x288" : "PAL1 50 Hz / 320x288";
}

int threedoh_core_visible_width(const threedoh_core *core)
{
    return core && core->visible_width > 0 ? core->visible_width : THREEDOH_SCREEN_WIDTH;
}

int threedoh_core_visible_height(const threedoh_core *core)
{
    return visible_height_for_standard(threedoh_core_active_video_standard(core));
}

int threedoh_core_max_visible_width(void)
{
    return THREEDOH_MAX_SCREEN_WIDTH;
}

int threedoh_core_max_visible_height(void)
{
    return THREEDOH_MAX_SCREEN_HEIGHT;
}

void threedoh_core_set_strict_bus_faults(threedoh_core *core, int enabled)
{
    if (!core)
        return;
    core->strict_bus_faults = enabled ? 1 : 0;
    _arm_SetStrictBusFaults(core->strict_bus_faults != 0);
}

int threedoh_core_strict_bus_faults(const threedoh_core *core)
{
    return core ? core->strict_bus_faults : 1;
}

uint32_t threedoh_core_last_fault_address(void)
{
    return _arm_LastFaultAddress();
}

uint32_t threedoh_core_last_fault_pc(void)
{
    return _arm_LastFaultPC();
}

uint32_t threedoh_core_last_fault_type(void)
{
    return _arm_LastFaultType();
}

uint32_t threedoh_core_arm_current_pc(void)
{
    return _arm_CurrentPC();
}

uint32_t threedoh_core_arm_current_cpsr(void)
{
    return _arm_CurrentCPSR();
}

uint32_t threedoh_core_arm_highram_read_count(void) { return _arm_HighRamReadCount(); }
uint32_t threedoh_core_arm_highram_write_count(void) { return _arm_HighRamWriteCount(); }
uint32_t threedoh_core_arm_highram_first_read(void) { return _arm_HighRamFirstRead(); }
uint32_t threedoh_core_arm_highram_first_write(void) { return _arm_HighRamFirstWrite(); }
uint32_t threedoh_core_arm_highram_last_read(void) { return _arm_HighRamLastRead(); }
uint32_t threedoh_core_arm_highram_last_write(void) { return _arm_HighRamLastWrite(); }

uint32_t threedoh_core_arm_fiq_entry_count(void)
{
    return _arm_FiqEntryCount();
}

uint32_t threedoh_core_arm_unaligned_prefetch_count(void)
{
    return _arm_UnalignedPrefetchCount();
}

uint32_t threedoh_core_arm_unaligned_prefetch_last(void)
{
    return _arm_UnalignedPrefetchLast();
}

uint32_t threedoh_core_arm_unaligned_prefetch_fetch(void)
{
    return _arm_UnalignedPrefetchFetch();
}

uint32_t threedoh_core_arm_mirrored_prefetch_count(void)
{
    return _arm_MirroredPrefetchCount();
}

uint32_t threedoh_core_arm_mirrored_prefetch_last(void)
{
    return _arm_MirroredPrefetchLast();
}

uint32_t threedoh_core_arm_mirrored_prefetch_fetch(void)
{
    return _arm_MirroredPrefetchFetch();
}

uint32_t threedoh_core_cp15_control(void)
{
    return _arm_CP15Control();
}

uint32_t threedoh_core_cp15_ops(void)
{
    return _arm_CP15Ops();
}

uint32_t threedoh_core_cp15_cache_flushes(void)
{
    return _arm_CP15CacheFlushes();
}

uint32_t threedoh_core_cp15_writebuffer_flushes(void)
{
    return _arm_CP15WriteBufferFlushes();
}

void threedoh_core_set_strict_madam_runaway_faults(threedoh_core *core, int enabled)
{
    if (core)
        core->strict_madam_runaway_faults = enabled ? 1 : 0;
    _madam_SetStrictRunawayFaults(enabled != 0);
}

int threedoh_core_strict_madam_runaway_faults(void)
{
    return _madam_GetStrictRunawayFaults() ? 1 : 0;
}


void threedoh_core_set_strict_dsp_resources(threedoh_core *core, int enabled)
{
    if (core)
        core->strict_dsp_resources = enabled ? 1 : 0;
    _dsp_SetStrictResourceFaults(enabled != 0);
}

int threedoh_core_strict_dsp_resources(void)
{
    return _dsp_GetStrictResourceFaults() ? 1 : 0;
}

uint32_t threedoh_core_dsp_resource_fault_count(void)
{
    return _dsp_GetResourceFaultCount();
}

uint32_t threedoh_core_dsp_resource_mirror_fault_count(void)
{
    return _dsp_GetResourceMirrorFaultCount();
}

uint32_t threedoh_core_dsp_last_resource_fault_address(void)
{
    return _dsp_GetLastResourceFaultAddress();
}

uint32_t threedoh_core_dsp_last_resource_fault_detail(void)
{
    return _dsp_GetLastResourceFaultDetail();
}

uint32_t threedoh_core_clio_fiq_generate_count(void) { return _clio_GetFiqGenerateCount(); }
uint32_t threedoh_core_clio_fiq_need_count(void) { return _clio_GetFiqNeedCount(); }
uint32_t threedoh_core_clio_last_fiq_reason1(void) { return _clio_GetLastFiqReason1(); }
uint32_t threedoh_core_clio_last_fiq_reason2(void) { return _clio_GetLastFiqReason2(); }
uint32_t threedoh_core_clio_irq0_pending(void) { return _clio_GetIrq0Pending(); }
uint32_t threedoh_core_clio_irq0_mask(void) { return _clio_GetIrq0Mask(); }
uint32_t threedoh_core_clio_irq1_pending(void) { return _clio_GetIrq1Pending(); }
uint32_t threedoh_core_clio_irq1_mask(void) { return _clio_GetIrq1Mask(); }
uint32_t threedoh_core_clio_eififo_read_count(void) { return _clio_GetEififoReadCount(); }
uint32_t threedoh_core_clio_eififo_empty_read_count(void) { return _clio_GetEififoEmptyReadCount(); }
uint32_t threedoh_core_clio_eififo_reload_count(void) { return _clio_GetEififoReloadCount(); }
uint32_t threedoh_core_clio_eofifo_write_count(void) { return _clio_GetEofifoWriteCount(); }
uint32_t threedoh_core_clio_eofifo_disabled_write_count(void) { return _clio_GetEofifoDisabledWriteCount(); }
uint32_t threedoh_core_clio_eofifo_full_count(void) { return _clio_GetEofifoFullCount(); }
uint32_t threedoh_core_clio_last_fifo_event(void) { return _clio_GetLastFifoEvent(); }
uint32_t threedoh_core_clio_last_eififo_empty_channel(void) { return _clio_GetLastEififoEmptyChannel(); }
uint32_t threedoh_core_clio_last_eififo_reload_channel(void) { return _clio_GetLastEififoReloadChannel(); }
uint32_t threedoh_core_clio_eififo_empty_channel_count(uint32_t channel) { return _clio_GetEififoEmptyChannelCount(channel); }
uint32_t threedoh_core_clio_eififo_reload_channel_count(uint32_t channel) { return _clio_GetEififoReloadChannelCount(channel); }
uint32_t threedoh_core_clio_fifo_level_reassert_count(void) { return _clio_GetFifoLevelReassertCount(); }
uint32_t threedoh_core_clio_fifo_level_reassert_mask(void) { return _clio_GetFifoLevelReassertMask(); }
uint32_t threedoh_core_clio_dspp_control_write_count(void) { return _clio_GetDSPPControlWriteCount(); }
uint32_t threedoh_core_clio_dspp_control_last_value(void) { return _clio_GetDSPPControlLastValue(); }
uint32_t threedoh_core_clio_dspp_control_non_gw_count(void) { return _clio_GetDSPPControlNonGWCount(); }
uint32_t threedoh_core_clio_dspp_reset_write_count(void) { return _clio_GetDSPPResetWriteCount(); }
uint32_t threedoh_core_clio_dspp_reset_last_value(void) { return _clio_GetDSPPResetLastValue(); }
uint32_t threedoh_core_clio_fifo_reload_dma_block_count(void) { return _clio_GetFifoReloadDMABlockCount(); }
uint32_t threedoh_core_clio_fifo_last_reload_dma_block_channel(void) { return _clio_GetFifoLastReloadDMABlockChannel(); }
uint32_t threedoh_core_clio_dspp_nmem_read_count(void) { return _clio_GetDSPPNMemReadCount(); }
uint32_t threedoh_core_clio_dspp_nmem_last_read_address(void) { return _clio_GetDSPPNMemLastReadAddress(); }
uint32_t threedoh_core_clio_xbus_dma_pulse_count(void) { return _clio_GetXbusDmaPulseCount(); }
uint32_t threedoh_core_clio_xbus_dma_last_len(void) { return _clio_GetXbusDmaLastLen(); }
uint32_t threedoh_core_clio_xbus_dma_last_addr(void) { return _clio_GetXbusDmaLastAddr(); }
uint32_t threedoh_core_clio_xbus_dma_timer_accum(void) { return _clio_GetXbusDmaTimerAccum(); }
uint32_t threedoh_core_clio_xbus_dma_timer_window(void) { return _clio_GetXbusDmaTimerWindow(); }
uint32_t threedoh_core_clio_xbus_timer120_adjust_count(void) { return _clio_GetXbusTimer120AdjustCount(); }
uint32_t threedoh_core_clio_xbus_timer120_last_in(void) { return _clio_GetXbusTimer120LastIn(); }
uint32_t threedoh_core_clio_xbus_timer120_last_out(void) { return _clio_GetXbusTimer120LastOut(); }
uint32_t threedoh_core_dsp_run_start_count(void) { return _dsp_GetRunStartCount(); }
uint32_t threedoh_core_dsp_run_stop_count(void) { return _dsp_GetRunStopCount(); }
uint32_t threedoh_core_dsp_reset_count(void) { return _dsp_GetResetCount(); }
uint32_t threedoh_core_dsp_int_write_count(void) { return _dsp_GetIntWriteCount(); }
uint32_t threedoh_core_dsp_last_int_value(void) { return _dsp_GetLastIntValue(); }
uint32_t threedoh_core_dsp_arm_sema_write_count(void) { return _dsp_GetArmSemaWriteCount(); }
uint32_t threedoh_core_dsp_arm_sema_read_count(void) { return _dsp_GetArmSemaReadCount(); }
uint32_t threedoh_core_dsp_dsp_sema_write_count(void) { return _dsp_GetDspSemaWriteCount(); }
uint32_t threedoh_core_dsp_dsp_sema_ack_count(void) { return _dsp_GetDspSemaAckCount(); }
uint32_t threedoh_core_dsp_cpu_supply_write_count(void) { return _dsp_GetCpuSupplyWriteCount(); }
uint32_t threedoh_core_dsp_cpu_supply_read_count(void) { return _dsp_GetCpuSupplyReadCount(); }
uint32_t threedoh_core_dsp_cpu_supply_random_read_count(void) { return _dsp_GetCpuSupplyRandomReadCount(); }
uint32_t threedoh_core_dsp_last_cpu_supply_channel(void) { return _dsp_GetLastCpuSupplyChannel(); }
uint32_t threedoh_core_dsp_current_pc(void) { return _dsp_GetCurrentPC(); }
uint32_t threedoh_core_dsp_counter_value(void) { return _dsp_GetCounterValue(); }
uint32_t threedoh_core_dsp_reload_value(void) { return _dsp_GetReloadValue(); }
uint32_t threedoh_core_dsp_current_status(void) { return _dsp_GetCurrentStatus(); }
uint32_t threedoh_core_dsp_audio_tick_count(void) { return _dsp_GetAudioTickCount(); }
uint32_t threedoh_core_dsp_counter_reload_count(void) { return _dsp_GetCounterReloadCount(); }
uint32_t threedoh_core_dsp_program_frame_count(void) { return _dsp_GetProgramFrameCount(); }
uint32_t threedoh_core_dsp_sleep_count(void) { return _dsp_GetSleepCount(); }
uint32_t threedoh_core_dsp_deferred_tick_count(void) { return _dsp_GetDeferredTickCount(); }
uint32_t threedoh_core_dsp_multi_reload_count(void) { return _dsp_GetMultiReloadCount(); }
uint32_t threedoh_core_dsp_audlock_write_count(void) { return _dsp_GetAudlockWriteCount(); }
uint32_t threedoh_core_dsp_audlock_reset_count(void) { return _dsp_GetAudlockResetCount(); }
uint32_t threedoh_core_dsp_last_audio_status_value(void) { return _dsp_GetLastAudioStatusValue(); }

uint32_t threedoh_core_madam_soft_clip_count(void)
{
    return _madam_GetSoftClipCount();
}

static void set_bios_path(const char *path)
{
    if (!path)
        path = "bios.bin";
    strncpy(biosFile, path, sizeof(biosFile) - 1);
    biosFile[sizeof(biosFile) - 1] = '\0';
}

int threedoh_core_start(threedoh_core *core, const char *bios_path, const char *iso_path)
{
    if (!core || !iso_path)
        return 0;
    if (core->started)
        return 1;

    memset(&core->frame, 0, sizeof(core->frame));
    _arm_SetStrictBusFaults(core->strict_bus_faults != 0);
    _dsp_SetStrictResourceFaults(core->strict_dsp_resources != 0);
    threedoh_core_apply_video_standard_mode(core, bios_path, iso_path);
    set_bios_path(bios_path);

    if (!fsInit())
        return 0;
    core->fs_started = 1;

    if (!fsOpenIso((char *)iso_path))
        return 0;
    core->iso_started = 1;

    nvram_configure_path(iso_path);

    isexit = 0;
    if (!_3do_Init())
        return 0;
    _madam_SetStrictRunawayFaults(core->strict_madam_runaway_faults != 0);

    core->started = 1;
    return 1;
}



static void clear_output_frame(void *destination_pixels, int width, int height)
{
#if BPP_TYPE == 32
    uint8_t *p = (uint8_t *)destination_pixels;
    int pixels = width * height;
    int i;
    for (i = 0; i < pixels; i++) {
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 255;
    }
#else
    memset(destination_pixels, 0, (size_t)width * (size_t)height * THREEDOH_PIXEL_BYTES);
#endif
}

static int frame_recorded_bitmap_height(const struct VDLFrame *frame, int visible_height)
{
    int y;
    int last = -1;
    if (!frame || visible_height <= 0)
        return 0;
    for (y = 0; y < visible_height; y++) {
        if (frame->lines[y].xHasBitmapLine)
            last = y;
    }
    return last >= 0 ? last + 1 : 0;
}

static int frame_line_width_from_vdl_dma(uint32_t clutdma)
{
    static const int width_for_modulo[8] = { 320, 384, 512, 640, 1024, 320, 320, 320 };
    int width = width_for_modulo[(clutdma >> 23) & 7U];
    if (width > THREEDOH_MAX_SCREEN_WIDTH)
        width = THREEDOH_MAX_SCREEN_WIDTH;
    if (width < THREEDOH_SCREEN_WIDTH)
        width = THREEDOH_SCREEN_WIDTH;
    return width;
}

static int frame_recorded_bitmap_width(const struct VDLFrame *frame, int visible_height)
{
    int y;
    int width = THREEDOH_SCREEN_WIDTH;
    if (!frame || visible_height <= 0)
        return width;
    for (y = 0; y < visible_height; y++) {
        if (frame->lines[y].xHasBitmapLine) {
            int line_width = frame_line_width_from_vdl_dma(frame->lines[y].xCLUTDMA);
            if (line_width > width)
                width = line_width;
        }
    }
    return width;
}

static int frame_visible_width_for_standard(const struct VDLFrame *frame, int standard, int visible_height)
{
    int recorded;
    if (standard != THREEDOH_VIDEO_PAL)
        return THREEDOH_SCREEN_WIDTH;
    recorded = frame_recorded_bitmap_width(frame, visible_height);
    return recorded > THREEDOH_SCREEN_WIDTH ? recorded : THREEDOH_SCREEN_WIDTH;
}

static int frame_copy_height_for_standard(const struct VDLFrame *frame, int standard, int visible_height)
{
    int recorded = frame_recorded_bitmap_height(frame, visible_height);

    if (standard == THREEDOH_VIDEO_PAL && visible_height == THREEDOH_PAL1_SCREEN_HEIGHT) {
        /* PAL1 is a 320x288 display, but many 3DO titles/BIOS paths still
         * build a 240-line NTSC-style VDL list.  Real PAL presentation does
         * not anchor that picture at y=0 with all extra blanking at the
         * bottom; center the 240-line active picture in the 288-line field
         * unless the VDL actually supplies PAL-height bitmap lines.
         */
        if (recorded > 0 && recorded <= THREEDOH_SCREEN_HEIGHT)
            return THREEDOH_SCREEN_HEIGHT;
    }

    return visible_height;
}

static int frame_output_y_for_standard(int standard, int visible_height, int copy_height)
{
    if (standard == THREEDOH_VIDEO_PAL && visible_height > copy_height)
        return (visible_height - copy_height) / 2;
    return 0;
}

int threedoh_core_frame(threedoh_core *core, void *destination_pixels,
                        uint_fast32_t width, uint_fast32_t height)
{
    if (!core || !core->started || !destination_pixels || isexit)
        return 0;

    const int visible_height = threedoh_core_visible_height(core);
    const int vdl_lines = THREEDOH_VDL_FIRST_VISIBLE_LINE + visible_height;

    if (width < THREEDOH_SCREEN_WIDTH || height < (uint_fast32_t)visible_height)
        return 0;

    soundBeginFrame();
    if (core->strict_bus_faults)
        _arm_ClearFault();
    _3do_Frame(&core->frame, true);
    if (core->strict_bus_faults && _arm_LastFaultType() != 0)
        return 0;
    memset(&core->frame, 0, sizeof(core->frame));
    _vdl_SetVisibleHeight((uint32_t)visible_height);
    for (uint_fast16_t line = 0; line < (uint_fast16_t)vdl_lines; line++)
        _vdl_DoLineNew(line, &core->frame);
    {
        const int standard = threedoh_core_active_video_standard(core);
        const int visible_width = frame_visible_width_for_standard(&core->frame, standard, visible_height);
        const int copy_height = frame_copy_height_for_standard(&core->frame, standard, visible_height);
        const int output_y = frame_output_y_for_standard(standard, visible_height, copy_height);
        uint8_t *dest = (uint8_t *)destination_pixels;
        const uint_fast32_t row_bytes = width * THREEDOH_PIXEL_BYTES;

        if (width < (uint_fast32_t)visible_width)
            return 0;

        core->visible_width = visible_width;
        clear_output_frame(destination_pixels, (int)width, visible_height);
        core->frame.srcw = (unsigned int)visible_width;
        core->frame.srch = (unsigned int)copy_height;
        dest += (size_t)output_y * (size_t)row_bytes;
        Get_Frame_Bitmap_Pitched(&core->frame, dest, (uint_fast32_t)visible_width,
                                 (uint_fast32_t)copy_height, width);
    }
    return 1;
}


#define THREEDOH_STATE_MAGIC 0x33444f53U /* "3DOS" */
#define THREEDOH_STATE_VERSION 1U

typedef struct threedoh_state_header {
    uint32_t magic;
    uint32_t version;
    uint32_t payload_size;
    uint32_t reserved;
} threedoh_state_header;

uint32_t threedoh_core_state_size(threedoh_core *core)
{
    if (!core || !core->started)
        return 0;
    return (uint32_t)(sizeof(threedoh_state_header) + _3do_SaveSize());
}

int threedoh_core_save_state(threedoh_core *core, void *buffer, uint32_t buffer_size)
{
    threedoh_state_header *header;
    uint32_t payload_size;

    if (!core || !core->started || !buffer)
        return 0;

    payload_size = _3do_SaveSize();
    if (buffer_size < sizeof(*header) + payload_size)
        return 0;

    header = (threedoh_state_header *)buffer;
    header->magic = THREEDOH_STATE_MAGIC;
    header->version = THREEDOH_STATE_VERSION;
    header->payload_size = payload_size;
    header->reserved = 0;
    _3do_Save((unsigned char *)buffer + sizeof(*header));
    return 1;
}

int threedoh_core_load_state(threedoh_core *core, const void *buffer, uint32_t buffer_size)
{
    const threedoh_state_header *header;

    if (!core || !core->started || !buffer || buffer_size < sizeof(*header))
        return 0;

    header = (const threedoh_state_header *)buffer;
    if (header->magic != THREEDOH_STATE_MAGIC ||
        header->version != THREEDOH_STATE_VERSION ||
        header->payload_size != _3do_SaveSize() ||
        header->payload_size > buffer_size - sizeof(*header))
        return 0;

    return _3do_LoadSized((void *)((const unsigned char *)buffer + sizeof(*header)),
                          header->payload_size) ? 1 : 0;
}


#ifndef THREEDOH_WASM
int threedoh_core_save_state_file(threedoh_core *core, const char *path)
{
    FILE *fp;
    unsigned char *buffer;
    uint32_t size;
    int ok;

    if (!path || !*path)
        return 0;
    size = threedoh_core_state_size(core);
    if (!size)
        return 0;
    buffer = (unsigned char *)malloc(size);
    if (!buffer)
        return 0;
    ok = threedoh_core_save_state(core, buffer, size);
    if (ok) {
        fp = fopen(path, "wb");
        if (!fp)
            ok = 0;
        else {
            ok = fwrite(buffer, 1, size, fp) == (size_t)size;
            fclose(fp);
        }
    }
    free(buffer);
    return ok;
}

int threedoh_core_load_state_file(threedoh_core *core, const char *path)
{
    FILE *fp;
    unsigned char *buffer;
    long size;
    int ok = 0;

    if (!path || !*path)
        return 0;
    fp = fopen(path, "rb");
    if (!fp)
        return 0;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return 0;
    }
    rewind(fp);
    buffer = (unsigned char *)malloc((size_t)size);
    if (buffer) {
        if (fread(buffer, 1, (size_t)size, fp) == (size_t)size)
            ok = threedoh_core_load_state(core, buffer, (uint32_t)size);
        free(buffer);
    }
    fclose(fp);
    return ok;
}
#else
int threedoh_core_save_state_file(threedoh_core *core, const char *path)
{
    (void)core;
    (void)path;
    return 0;
}

int threedoh_core_load_state_file(threedoh_core *core, const char *path)
{
    (void)core;
    (void)path;
    return 0;
}
#endif

int threedoh_core_soft_reset(threedoh_core *core)
{
    if (!core || !core->started)
        return 0;

    _3do_Destroy();
    isexit = 0;
    memset(&core->frame, 0, sizeof(core->frame));
    apply_video_standard(core, core->active_video_standard);
    soundBeginFrame();

    if (!_3do_Init()) {
        core->started = 0;
        return 0;
    }
    return 1;
}

void threedoh_core_stop(threedoh_core *core)
{
    if (!core)
        return;

    if (core->started) {
        _3do_Destroy();
        core->started = 0;
    }
    if (core->iso_started) {
        fsCloseIso();
        core->iso_started = 0;
    }
    if (core->fs_started) {
        fsClose();
        core->fs_started = 0;
    }
    isexit = 0;
}

int threedoh_core_is_started(const threedoh_core *core)
{
    return core ? core->started : 0;
}
