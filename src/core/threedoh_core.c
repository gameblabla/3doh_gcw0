#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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
    int strict_bus_faults;
    int strict_madam_runaway_faults;
    int strict_dsp_resources;
};

typedef struct {
    char recordType;
    char syncBytes[5];
    char recordVersion;
    char flags;
    char comment[32];
    char label[32];
    uint32_t id;
    uint32_t blockSize;
    uint32_t blockCount;
    uint32_t rootDirId;
    uint32_t rootDirBlocks;
    uint32_t rootDirBlockSize;
    uint32_t lastRootDirCopy;
    uint32_t rootDirCopies[8];
} NvRamStr;

extern int isexit;

static uint32_t ReverseBytes(uint32_t value)
{
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
           (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}

void readNvRam(void *pnvram)
{
    uint_fast8_t x;
    NvRamStr *nvramStruct = (NvRamStr *)pnvram;

    nvramStruct->recordType = 0x01;
    for (x = 0; x < 5; x++)
        nvramStruct->syncBytes[x] = (char)'Z';
    nvramStruct->recordVersion = 0x02;
    nvramStruct->flags = 0x00;
    for (x = 0; x < 32; x++)
        nvramStruct->comment[x] = 0;

    nvramStruct->label[0] = (char)'n';
    nvramStruct->label[1] = (char)'v';
    nvramStruct->label[2] = (char)'r';
    nvramStruct->label[3] = (char)'a';
    nvramStruct->label[4] = (char)'m';
    for (x = 5; x < 32; x++)
        nvramStruct->label[x] = 0;

    nvramStruct->id = ReverseBytes(0xFFFFFFFF);
    nvramStruct->blockSize = ReverseBytes(0x00000001);
    nvramStruct->blockCount = ReverseBytes(0x00008000);
    nvramStruct->rootDirId = ReverseBytes(0xFFFFFFFE);
    nvramStruct->rootDirBlocks = ReverseBytes(0x00000000);
    nvramStruct->rootDirBlockSize = ReverseBytes(0x00000001);
    nvramStruct->lastRootDirCopy = ReverseBytes(0x00000000);
    nvramStruct->rootDirCopies[0] = ReverseBytes(0x00000084);
    for (x = 1; x < 8; x++)
        nvramStruct->rootDirCopies[x] = 0;
}

void writeNvRam(void)
{
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
    int pal_hint = 0;
    int ntsc_hint = 0;

    pal_hint = path_contains_token(bios_path, "pal") ||
               path_contains_token(bios_path, "europe") ||
               path_contains_token(bios_path, "euro") ||
               path_contains_token(iso_path, "pal") ||
               path_contains_token(iso_path, "europe") ||
               path_contains_token(iso_path, "euro") ||
               file_contains_token(bios_path, "pal ") ||
               file_contains_token(bios_path, "pal-") ||
               file_contains_token(bios_path, "pal_") ||
               file_contains_token(bios_path, "europe") ||
               file_contains_token(iso_path, "pal ") ||
               file_contains_token(iso_path, "pal-") ||
               file_contains_token(iso_path, "pal_") ||
               file_contains_token(iso_path, "europe");

    ntsc_hint = path_contains_token(bios_path, "ntsc") ||
                path_contains_token(bios_path, "usa") ||
                path_contains_token(bios_path, "japan") ||
                path_contains_token(bios_path, "jpn") ||
                path_contains_token(iso_path, "ntsc") ||
                path_contains_token(iso_path, "usa") ||
                path_contains_token(iso_path, "japan") ||
                path_contains_token(iso_path, "jpn") ||
                file_contains_token(bios_path, "ntsc") ||
                file_contains_token(bios_path, "japan") ||
                file_contains_token(bios_path, "usa") ||
                file_contains_token(iso_path, "ntsc") ||
                file_contains_token(iso_path, "japan") ||
                file_contains_token(iso_path, "usa");

    if (pal_hint && !ntsc_hint)
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
    return threedoh_core_active_video_standard(core) == THREEDOH_VIDEO_PAL ? "PAL1 50 Hz / 320x288" : "NTSC 60 Hz / 320x240";
}

int threedoh_core_visible_width(const threedoh_core *core)
{
    (void)core;
    return THREEDOH_SCREEN_WIDTH;
}

int threedoh_core_visible_height(const threedoh_core *core)
{
    return visible_height_for_standard(threedoh_core_active_video_standard(core));
}

int threedoh_core_max_visible_width(void)
{
    return THREEDOH_SCREEN_WIDTH;
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
        const int copy_height = frame_copy_height_for_standard(&core->frame, standard, visible_height);
        const int output_y = frame_output_y_for_standard(standard, visible_height, copy_height);
        uint8_t *dest = (uint8_t *)destination_pixels;
        const uint_fast32_t row_bytes = THREEDOH_SCREEN_WIDTH * THREEDOH_PIXEL_BYTES;

        clear_output_frame(destination_pixels, THREEDOH_SCREEN_WIDTH, visible_height);
        core->frame.srcw = THREEDOH_SCREEN_WIDTH;
        core->frame.srch = (unsigned int)copy_height;
        dest += (size_t)output_y * (size_t)row_bytes;
        Get_Frame_Bitmap(&core->frame, dest, THREEDOH_SCREEN_WIDTH, (uint_fast32_t)copy_height);
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
