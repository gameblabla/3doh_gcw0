#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/threedoh_core.h"
#include "input.h"
#include "sound.h"
#include "arm.h"

static threedoh_core *g_core;
static uint8_t g_framebuffer[THREEDOH_MAX_SCREEN_WIDTH * THREEDOH_MAX_SCREEN_HEIGHT * THREEDOH_PIXEL_BYTES];
static uint8_t g_present_framebuffer[THREEDOH_MAX_SCREEN_WIDTH * THREEDOH_MAX_SCREEN_HEIGHT * THREEDOH_PIXEL_BYTES];
static int g_video_mode = THREEDOH_VIDEO_AUTO;
static int g_started;
static int g_last_error;

int threedoh_width(void) { return g_core ? threedoh_core_visible_width(g_core) : THREEDOH_SCREEN_WIDTH; }
int threedoh_height(void) { return g_core ? threedoh_core_visible_height(g_core) : THREEDOH_SCREEN_HEIGHT; }
uint8_t *threedoh_framebuffer_ptr(void) { return g_present_framebuffer; }
int threedoh_last_error(void) { return g_last_error; }
uint32_t threedoh_last_fault_address(void) { return threedoh_core_last_fault_address(); }
uint32_t threedoh_last_fault_pc(void) { return threedoh_core_last_fault_pc(); }
uint32_t threedoh_last_fault_type(void) { return threedoh_core_last_fault_type(); }
uint32_t threedoh_arm_current_pc(void) { return threedoh_core_arm_current_pc(); }
uint32_t threedoh_arm_current_cpsr(void) { return threedoh_core_arm_current_cpsr(); }
uint32_t threedoh_arm_fiq_entry_count(void) { return threedoh_core_arm_fiq_entry_count(); }

uint32_t threedoh_arm_highram_read_count(void) { return threedoh_core_arm_highram_read_count(); }
uint32_t threedoh_arm_highram_write_count(void) { return threedoh_core_arm_highram_write_count(); }
uint32_t threedoh_arm_highram_first_read(void) { return threedoh_core_arm_highram_first_read(); }
uint32_t threedoh_arm_highram_first_write(void) { return threedoh_core_arm_highram_first_write(); }
uint32_t threedoh_arm_highram_last_read(void) { return threedoh_core_arm_highram_last_read(); }
uint32_t threedoh_arm_highram_last_write(void) { return threedoh_core_arm_highram_last_write(); }
uint32_t threedoh_cp15_control(void) { return _arm_CP15Control(); }
uint32_t threedoh_cp15_ops(void) { return _arm_CP15Ops(); }
uint32_t threedoh_cp15_cache_flushes(void) { return _arm_CP15CacheFlushes(); }
uint32_t threedoh_cp15_writebuffer_flushes(void) { return _arm_CP15WriteBufferFlushes(); }
uint32_t threedoh_madam_soft_clip_count(void) { return threedoh_core_madam_soft_clip_count(); }
uint32_t threedoh_dsp_resource_fault_count(void) { return threedoh_core_dsp_resource_fault_count(); }
uint32_t threedoh_dsp_resource_mirror_fault_count(void) { return threedoh_core_dsp_resource_mirror_fault_count(); }
uint32_t threedoh_dsp_last_resource_fault_address(void) { return threedoh_core_dsp_last_resource_fault_address(); }
uint32_t threedoh_dsp_last_resource_fault_detail(void) { return threedoh_core_dsp_last_resource_fault_detail(); }
uint32_t threedoh_clio_fiq_generate_count(void) { return threedoh_core_clio_fiq_generate_count(); }
uint32_t threedoh_clio_fiq_need_count(void) { return threedoh_core_clio_fiq_need_count(); }
uint32_t threedoh_clio_last_fiq_reason1(void) { return threedoh_core_clio_last_fiq_reason1(); }
uint32_t threedoh_clio_last_fiq_reason2(void) { return threedoh_core_clio_last_fiq_reason2(); }
uint32_t threedoh_clio_irq0_pending(void) { return threedoh_core_clio_irq0_pending(); }
uint32_t threedoh_clio_irq0_mask(void) { return threedoh_core_clio_irq0_mask(); }
uint32_t threedoh_clio_irq1_pending(void) { return threedoh_core_clio_irq1_pending(); }
uint32_t threedoh_clio_irq1_mask(void) { return threedoh_core_clio_irq1_mask(); }
uint32_t threedoh_clio_eififo_read_count(void) { return threedoh_core_clio_eififo_read_count(); }
uint32_t threedoh_clio_eififo_empty_read_count(void) { return threedoh_core_clio_eififo_empty_read_count(); }
uint32_t threedoh_clio_eififo_reload_count(void) { return threedoh_core_clio_eififo_reload_count(); }
uint32_t threedoh_clio_eofifo_write_count(void) { return threedoh_core_clio_eofifo_write_count(); }
uint32_t threedoh_clio_eofifo_disabled_write_count(void) { return threedoh_core_clio_eofifo_disabled_write_count(); }
uint32_t threedoh_clio_eofifo_full_count(void) { return threedoh_core_clio_eofifo_full_count(); }
uint32_t threedoh_clio_last_fifo_event(void) { return threedoh_core_clio_last_fifo_event(); }
uint32_t threedoh_clio_last_eififo_empty_channel(void) { return threedoh_core_clio_last_eififo_empty_channel(); }
uint32_t threedoh_clio_last_eififo_reload_channel(void) { return threedoh_core_clio_last_eififo_reload_channel(); }
uint32_t threedoh_clio_eififo_empty_channel_count(uint32_t channel) { return threedoh_core_clio_eififo_empty_channel_count(channel); }
uint32_t threedoh_clio_eififo_reload_channel_count(uint32_t channel) { return threedoh_core_clio_eififo_reload_channel_count(channel); }
uint32_t threedoh_clio_fifo_level_reassert_count(void) { return threedoh_core_clio_fifo_level_reassert_count(); }
uint32_t threedoh_clio_fifo_level_reassert_mask(void) { return threedoh_core_clio_fifo_level_reassert_mask(); }

uint32_t threedoh_clio_dspp_control_write_count(void) { return threedoh_core_clio_dspp_control_write_count(); }
uint32_t threedoh_clio_dspp_control_last_value(void) { return threedoh_core_clio_dspp_control_last_value(); }
uint32_t threedoh_clio_dspp_control_non_gw_count(void) { return threedoh_core_clio_dspp_control_non_gw_count(); }
uint32_t threedoh_clio_dspp_reset_write_count(void) { return threedoh_core_clio_dspp_reset_write_count(); }
uint32_t threedoh_clio_dspp_reset_last_value(void) { return threedoh_core_clio_dspp_reset_last_value(); }
uint32_t threedoh_clio_fifo_reload_dma_block_count(void) { return threedoh_core_clio_fifo_reload_dma_block_count(); }
uint32_t threedoh_clio_fifo_last_reload_dma_block_channel(void) { return threedoh_core_clio_fifo_last_reload_dma_block_channel(); }
uint32_t threedoh_clio_dspp_nmem_read_count(void) { return threedoh_core_clio_dspp_nmem_read_count(); }
uint32_t threedoh_clio_dspp_nmem_last_read_address(void) { return threedoh_core_clio_dspp_nmem_last_read_address(); }
uint32_t threedoh_clio_xbus_dma_pulse_count(void) { return threedoh_core_clio_xbus_dma_pulse_count(); }
uint32_t threedoh_clio_xbus_dma_last_len(void) { return threedoh_core_clio_xbus_dma_last_len(); }
uint32_t threedoh_clio_xbus_dma_last_addr(void) { return threedoh_core_clio_xbus_dma_last_addr(); }
uint32_t threedoh_clio_xbus_dma_timer_accum(void) { return threedoh_core_clio_xbus_dma_timer_accum(); }
uint32_t threedoh_clio_xbus_dma_timer_window(void) { return threedoh_core_clio_xbus_dma_timer_window(); }
uint32_t threedoh_clio_xbus_timer120_adjust_count(void) { return threedoh_core_clio_xbus_timer120_adjust_count(); }
uint32_t threedoh_clio_xbus_timer120_last_in(void) { return threedoh_core_clio_xbus_timer120_last_in(); }
uint32_t threedoh_clio_xbus_timer120_last_out(void) { return threedoh_core_clio_xbus_timer120_last_out(); }
uint32_t threedoh_dsp_run_start_count(void) { return threedoh_core_dsp_run_start_count(); }
uint32_t threedoh_dsp_run_stop_count(void) { return threedoh_core_dsp_run_stop_count(); }
uint32_t threedoh_dsp_reset_count(void) { return threedoh_core_dsp_reset_count(); }
uint32_t threedoh_dsp_int_write_count(void) { return threedoh_core_dsp_int_write_count(); }
uint32_t threedoh_dsp_last_int_value(void) { return threedoh_core_dsp_last_int_value(); }
uint32_t threedoh_dsp_arm_sema_write_count(void) { return threedoh_core_dsp_arm_sema_write_count(); }
uint32_t threedoh_dsp_arm_sema_read_count(void) { return threedoh_core_dsp_arm_sema_read_count(); }
uint32_t threedoh_dsp_dsp_sema_write_count(void) { return threedoh_core_dsp_dsp_sema_write_count(); }
uint32_t threedoh_dsp_dsp_sema_ack_count(void) { return threedoh_core_dsp_dsp_sema_ack_count(); }
uint32_t threedoh_dsp_cpu_supply_write_count(void) { return threedoh_core_dsp_cpu_supply_write_count(); }
uint32_t threedoh_dsp_cpu_supply_read_count(void) { return threedoh_core_dsp_cpu_supply_read_count(); }
uint32_t threedoh_dsp_cpu_supply_random_read_count(void) { return threedoh_core_dsp_cpu_supply_random_read_count(); }
uint32_t threedoh_dsp_last_cpu_supply_channel(void) { return threedoh_core_dsp_last_cpu_supply_channel(); }
uint32_t threedoh_dsp_current_pc(void) { return threedoh_core_dsp_current_pc(); }
uint32_t threedoh_dsp_counter_value(void) { return threedoh_core_dsp_counter_value(); }
uint32_t threedoh_dsp_reload_value(void) { return threedoh_core_dsp_reload_value(); }
uint32_t threedoh_dsp_current_status(void) { return threedoh_core_dsp_current_status(); }
uint32_t threedoh_dsp_audio_tick_count(void) { return threedoh_core_dsp_audio_tick_count(); }
uint32_t threedoh_dsp_counter_reload_count(void) { return threedoh_core_dsp_counter_reload_count(); }
uint32_t threedoh_dsp_program_frame_count(void) { return threedoh_core_dsp_program_frame_count(); }
uint32_t threedoh_dsp_sleep_count(void) { return threedoh_core_dsp_sleep_count(); }
uint32_t threedoh_dsp_deferred_tick_count(void) { return threedoh_core_dsp_deferred_tick_count(); }
uint32_t threedoh_dsp_multi_reload_count(void) { return threedoh_core_dsp_multi_reload_count(); }
uint32_t threedoh_dsp_audlock_write_count(void) { return threedoh_core_dsp_audlock_write_count(); }
uint32_t threedoh_dsp_audlock_reset_count(void) { return threedoh_core_dsp_audlock_reset_count(); }
uint32_t threedoh_dsp_last_audio_status_value(void) { return threedoh_core_dsp_last_audio_status_value(); }
int threedoh_strict_dsp_resources(void) { return threedoh_core_strict_dsp_resources(); }
void threedoh_set_strict_dsp_resources(int enabled)
{
    if (g_core)
        threedoh_core_set_strict_dsp_resources(g_core, enabled != 0);
}
int threedoh_is_started(void) { return g_started; }
int threedoh_frame_rate_hz(void) { return g_core ? threedoh_core_frame_rate_hz(g_core) : 60; }

void threedoh_set_video_standard_mode(int mode)
{
    if (mode != THREEDOH_VIDEO_AUTO && mode != THREEDOH_VIDEO_NTSC && mode != THREEDOH_VIDEO_PAL)
        mode = THREEDOH_VIDEO_AUTO;
    g_video_mode = mode;
    if (g_core)
        threedoh_core_set_video_standard_mode(g_core, mode);
}


int threedoh_start(void)
{
    if (g_started)
        return 0;

    memset(g_framebuffer, 0, sizeof(g_framebuffer));

    if (!g_core) {
        g_core = (threedoh_core *)calloc(1, threedoh_core_size());
        if (!g_core) {
            g_last_error = 10;
            return g_last_error;
        }
        threedoh_core_construct(g_core);
        threedoh_core_set_video_standard_mode(g_core, g_video_mode);
    }

    if (!soundInit()) {
        g_last_error = 3;
        return g_last_error;
    }
    if (!inputInit()) {
        g_last_error = 4;
        return g_last_error;
    }
    if (!threedoh_core_start(g_core, "browser:bios.bin", "browser:game.iso")) {
        g_last_error = 5;
        soundClose();
        inputClose();
        return g_last_error;
    }

    g_started = 1;
    g_last_error = 0;
    return 0;
}

int threedoh_frame(void)
{
    int y;
    int w;
    int h;
    if (!g_started)
        return 0;
    if (!threedoh_core_frame(g_core, g_framebuffer,
                             THREEDOH_MAX_SCREEN_WIDTH, THREEDOH_MAX_SCREEN_HEIGHT))
        return 0;
    w = threedoh_core_visible_width(g_core);
    h = threedoh_core_visible_height(g_core);
    for (y = 0; y < h; y++)
        memcpy(g_present_framebuffer + (size_t)y * (size_t)w * THREEDOH_PIXEL_BYTES,
               g_framebuffer + (size_t)y * THREEDOH_MAX_SCREEN_WIDTH * THREEDOH_PIXEL_BYTES,
               (size_t)w * THREEDOH_PIXEL_BYTES);
    return 1;
}

int threedoh_soft_reset(void)
{
    if (!g_started) {
        g_last_error = 6;
        return g_last_error;
    }
    memset(g_framebuffer, 0, sizeof(g_framebuffer));
    if (!threedoh_core_soft_reset(g_core)) {
        g_started = 0;
        g_last_error = 5;
        return g_last_error;
    }
    g_last_error = 0;
    return 0;
}

void threedoh_shutdown(void)
{
    if (!g_started)
        return;
    threedoh_core_stop(g_core);
    inputClose();
    soundClose();
    g_started = 0;
    g_last_error = 0;
}
