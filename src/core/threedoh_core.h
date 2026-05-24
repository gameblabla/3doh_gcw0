#ifndef THREEDOH_CORE_H
#define THREEDOH_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifndef THREEDOH_SCREEN_WIDTH
#define THREEDOH_SCREEN_WIDTH 320
#endif
#ifndef THREEDOH_SCREEN_HEIGHT
#define THREEDOH_SCREEN_HEIGHT 240
#endif
#ifndef THREEDOH_PAL1_SCREEN_HEIGHT
#define THREEDOH_PAL1_SCREEN_HEIGHT 288
#endif
#ifndef THREEDOH_MAX_SCREEN_HEIGHT
#define THREEDOH_MAX_SCREEN_HEIGHT 288
#endif

#if BPP_TYPE == 32
#define THREEDOH_PIXEL_BYTES 4
#else
#define THREEDOH_PIXEL_BYTES 2
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct threedoh_core threedoh_core;

enum threedoh_video_standard {
    THREEDOH_VIDEO_AUTO = 0,
    THREEDOH_VIDEO_NTSC = 1,
    THREEDOH_VIDEO_PAL = 2
};

size_t threedoh_core_size(void);
void threedoh_core_construct(threedoh_core *core);
void threedoh_core_set_video_standard_mode(threedoh_core *core, int mode);
int threedoh_core_video_standard_mode(const threedoh_core *core);
int threedoh_core_apply_video_standard_mode(threedoh_core *core, const char *bios_path, const char *iso_path);
int threedoh_core_active_video_standard(const threedoh_core *core);
int threedoh_core_frame_rate_hz(const threedoh_core *core);
const char *threedoh_core_video_standard_name(const threedoh_core *core);
int threedoh_core_visible_width(const threedoh_core *core);
int threedoh_core_visible_height(const threedoh_core *core);
int threedoh_core_max_visible_width(void);
int threedoh_core_max_visible_height(void);
void threedoh_core_set_strict_bus_faults(threedoh_core *core, int enabled);
int threedoh_core_strict_bus_faults(const threedoh_core *core);
uint32_t threedoh_core_last_fault_address(void);
uint32_t threedoh_core_last_fault_pc(void);
uint32_t threedoh_core_last_fault_type(void);
uint32_t threedoh_core_arm_current_pc(void);
uint32_t threedoh_core_arm_current_cpsr(void);
uint32_t threedoh_core_arm_highram_read_count(void);
uint32_t threedoh_core_arm_highram_write_count(void);
uint32_t threedoh_core_arm_highram_first_read(void);
uint32_t threedoh_core_arm_highram_first_write(void);
uint32_t threedoh_core_arm_highram_last_read(void);
uint32_t threedoh_core_arm_highram_last_write(void);
uint32_t threedoh_core_arm_fiq_entry_count(void);
uint32_t threedoh_core_arm_unaligned_prefetch_count(void);
uint32_t threedoh_core_arm_unaligned_prefetch_last(void);
uint32_t threedoh_core_arm_unaligned_prefetch_fetch(void);
uint32_t threedoh_core_arm_mirrored_prefetch_count(void);
uint32_t threedoh_core_arm_mirrored_prefetch_last(void);
uint32_t threedoh_core_arm_mirrored_prefetch_fetch(void);
uint32_t threedoh_core_cp15_control(void);
uint32_t threedoh_core_cp15_ops(void);
uint32_t threedoh_core_cp15_cache_flushes(void);
uint32_t threedoh_core_cp15_writebuffer_flushes(void);
void threedoh_core_set_strict_madam_runaway_faults(threedoh_core *core, int enabled);
int threedoh_core_strict_madam_runaway_faults(void);
void threedoh_core_set_strict_dsp_resources(threedoh_core *core, int enabled);
int threedoh_core_strict_dsp_resources(void);
uint32_t threedoh_core_dsp_resource_fault_count(void);
uint32_t threedoh_core_dsp_resource_mirror_fault_count(void);
uint32_t threedoh_core_dsp_last_resource_fault_address(void);
uint32_t threedoh_core_dsp_last_resource_fault_detail(void);
uint32_t threedoh_core_clio_fiq_generate_count(void);
uint32_t threedoh_core_clio_fiq_need_count(void);
uint32_t threedoh_core_clio_last_fiq_reason1(void);
uint32_t threedoh_core_clio_last_fiq_reason2(void);
uint32_t threedoh_core_clio_irq0_pending(void);
uint32_t threedoh_core_clio_irq0_mask(void);
uint32_t threedoh_core_clio_irq1_pending(void);
uint32_t threedoh_core_clio_irq1_mask(void);
uint32_t threedoh_core_clio_eififo_read_count(void);
uint32_t threedoh_core_clio_eififo_empty_read_count(void);
uint32_t threedoh_core_clio_eififo_reload_count(void);
uint32_t threedoh_core_clio_eofifo_write_count(void);
uint32_t threedoh_core_clio_eofifo_disabled_write_count(void);
uint32_t threedoh_core_clio_eofifo_full_count(void);
uint32_t threedoh_core_clio_last_fifo_event(void);
uint32_t threedoh_core_clio_last_eififo_empty_channel(void);
uint32_t threedoh_core_clio_last_eififo_reload_channel(void);
uint32_t threedoh_core_clio_eififo_empty_channel_count(uint32_t channel);
uint32_t threedoh_core_clio_eififo_reload_channel_count(uint32_t channel);
uint32_t threedoh_core_clio_fifo_level_reassert_count(void);
uint32_t threedoh_core_clio_fifo_level_reassert_mask(void);
uint32_t threedoh_core_clio_dspp_control_write_count(void);
uint32_t threedoh_core_clio_dspp_control_last_value(void);
uint32_t threedoh_core_clio_dspp_control_non_gw_count(void);
uint32_t threedoh_core_clio_dspp_reset_write_count(void);
uint32_t threedoh_core_clio_dspp_reset_last_value(void);
uint32_t threedoh_core_clio_fifo_reload_dma_block_count(void);
uint32_t threedoh_core_clio_fifo_last_reload_dma_block_channel(void);
uint32_t threedoh_core_clio_dspp_nmem_read_count(void);
uint32_t threedoh_core_clio_dspp_nmem_last_read_address(void);
uint32_t threedoh_core_clio_xbus_dma_pulse_count(void);
uint32_t threedoh_core_clio_xbus_dma_last_len(void);
uint32_t threedoh_core_clio_xbus_dma_last_addr(void);
uint32_t threedoh_core_clio_xbus_dma_timer_accum(void);
uint32_t threedoh_core_clio_xbus_dma_timer_window(void);
uint32_t threedoh_core_clio_xbus_timer120_adjust_count(void);
uint32_t threedoh_core_clio_xbus_timer120_last_in(void);
uint32_t threedoh_core_clio_xbus_timer120_last_out(void);
uint32_t threedoh_core_dsp_run_start_count(void);
uint32_t threedoh_core_dsp_run_stop_count(void);
uint32_t threedoh_core_dsp_reset_count(void);
uint32_t threedoh_core_dsp_int_write_count(void);
uint32_t threedoh_core_dsp_last_int_value(void);
uint32_t threedoh_core_dsp_arm_sema_write_count(void);
uint32_t threedoh_core_dsp_arm_sema_read_count(void);
uint32_t threedoh_core_dsp_dsp_sema_write_count(void);
uint32_t threedoh_core_dsp_dsp_sema_ack_count(void);
uint32_t threedoh_core_dsp_cpu_supply_write_count(void);
uint32_t threedoh_core_dsp_cpu_supply_read_count(void);
uint32_t threedoh_core_dsp_cpu_supply_random_read_count(void);
uint32_t threedoh_core_dsp_last_cpu_supply_channel(void);
uint32_t threedoh_core_dsp_current_pc(void);
uint32_t threedoh_core_dsp_counter_value(void);
uint32_t threedoh_core_dsp_reload_value(void);
uint32_t threedoh_core_dsp_current_status(void);
uint32_t threedoh_core_dsp_audio_tick_count(void);
uint32_t threedoh_core_dsp_counter_reload_count(void);
uint32_t threedoh_core_dsp_program_frame_count(void);
uint32_t threedoh_core_dsp_sleep_count(void);
uint32_t threedoh_core_dsp_deferred_tick_count(void);
uint32_t threedoh_core_dsp_multi_reload_count(void);
uint32_t threedoh_core_dsp_audlock_write_count(void);
uint32_t threedoh_core_dsp_audlock_reset_count(void);
uint32_t threedoh_core_dsp_last_audio_status_value(void);
uint32_t threedoh_core_madam_soft_clip_count(void);
int threedoh_core_start(threedoh_core *core, const char *bios_path, const char *iso_path);
int threedoh_core_frame(threedoh_core *core, void *destination_pixels,
                        uint_fast32_t width, uint_fast32_t height);
int threedoh_core_soft_reset(threedoh_core *core);
uint32_t threedoh_core_state_size(threedoh_core *core);
int threedoh_core_save_state(threedoh_core *core, void *buffer, uint32_t buffer_size);
int threedoh_core_load_state(threedoh_core *core, const void *buffer, uint32_t buffer_size);
int threedoh_core_save_state_file(threedoh_core *core, const char *path);
int threedoh_core_load_state_file(threedoh_core *core, const char *path);
void threedoh_core_stop(threedoh_core *core);
int threedoh_core_is_started(const threedoh_core *core);

extern char biosFile[128];

#ifdef __cplusplus
}
#endif

#endif
