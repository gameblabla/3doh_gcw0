#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "core/threedoh_core.h"
#include "input.h"
#include "input_script.h"
#include "sound.h"
#include "freedo/Clio.h"
#include "freedo/arm.h"
extern void* Getp_RAMS(void);

extern int threedoh_audio_sample_count(void);
extern uint32_t *threedoh_audio_sample_ptr(void);
extern int threedoh_audio_sample_rate(void);


static FILE *audio_dump_fp = NULL;
static uint32_t audio_dump_data_bytes = 0;
static uint32_t audio_dump_sample_rate = 44100;

static void write_wav_le16(FILE *fp, unsigned int v)
{
    fputc((int)(v & 0xff), fp);
    fputc((int)((v >> 8) & 0xff), fp);
}

static void write_wav_le32(FILE *fp, unsigned int v)
{
    fputc((int)(v & 0xff), fp);
    fputc((int)((v >> 8) & 0xff), fp);
    fputc((int)((v >> 16) & 0xff), fp);
    fputc((int)((v >> 24) & 0xff), fp);
}

static void maybe_begin_audio_dump(void)
{
    const char *path = getenv("THREEDOH_HEADLESS_DUMP_WAV");
    if (!path || !*path)
        return;
    audio_dump_fp = fopen(path, "wb+");
    if (!audio_dump_fp) {
        fprintf(stderr, "could not write wav: %s\n", path);
        return;
    }
    audio_dump_sample_rate = (uint32_t)threedoh_audio_sample_rate();
    fwrite("RIFF", 1, 4, audio_dump_fp);
    write_wav_le32(audio_dump_fp, 0);
    fwrite("WAVEfmt ", 1, 8, audio_dump_fp);
    write_wav_le32(audio_dump_fp, 16);
    write_wav_le16(audio_dump_fp, 1);
    write_wav_le16(audio_dump_fp, 2);
    write_wav_le32(audio_dump_fp, audio_dump_sample_rate);
    write_wav_le32(audio_dump_fp, audio_dump_sample_rate * 4);
    write_wav_le16(audio_dump_fp, 4);
    write_wav_le16(audio_dump_fp, 16);
    fwrite("data", 1, 4, audio_dump_fp);
    write_wav_le32(audio_dump_fp, 0);
}

static void maybe_append_audio_frame(void)
{
    int count;
    uint32_t *samples;
    int i;
    if (!audio_dump_fp)
        return;
    count = threedoh_audio_sample_count();
    samples = threedoh_audio_sample_ptr();
    for (i = 0; i < count; i++) {
        uint32_t v = samples[i];
        write_wav_le16(audio_dump_fp, v & 0xffffu);
        write_wav_le16(audio_dump_fp, (v >> 16) & 0xffffu);
        audio_dump_data_bytes += 4;
    }
}

static void maybe_end_audio_dump(void)
{
    if (!audio_dump_fp)
        return;
    fseek(audio_dump_fp, 4, SEEK_SET);
    write_wav_le32(audio_dump_fp, 36u + audio_dump_data_bytes);
    fseek(audio_dump_fp, 40, SEEK_SET);
    write_wav_le32(audio_dump_fp, audio_dump_data_bytes);
    fclose(audio_dump_fp);
    audio_dump_fp = NULL;
}

static void print_diag(const char *prefix, unsigned long frame)
{
    printf("%s frame=%lu fault=%u pc=%08x addr=%08x arm_pc=%08x cpsr=%08x r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r9=%08x r10=%08x r11=%08x r12=%08x r14=%08x arm_fiq=%u unalign_pf=%u/%08x/%08x mirror_pf=%u/%08x/%08x highram_r=%u/%08x/%08x highram_w=%u/%08x/%08x "
           "dspres=%u dspmir=%u dspaddr=%08x dspdetail=%08x "
           "dsp_run=%u/%u dsp_reset=%u dsp_int=%u dsp_intval=%04x dsp_pc=%03x dsp_status=%08x dsp_cnt=%u dsp_prld=%u dsp_ticks=%u dsp_reload=%u dsp_frames=%u dsp_sleep=%u dsp_defer=%u dsp_multi=%u audlock=%u/%u/%04x "
           "sema_arm_w=%u sema_arm_r=%u sema_dsp_w=%u sema_dsp_ack=%u "
           "cpus_w=%u cpus_r=%u cpus_rand=%u cpus_ch=%u "
           "fiq_gen=%u fiq_need=%u fiq_last=%08x/%08x irq0=%08x/%08x irq1=%08x/%08x "
           "eif_rd=%u eif_empty=%u eif_reload=%u eof_wr=%u eof_dis=%u eof_full=%u fifo_last=%08x "
           "eif_last_empty=%u eif_last_reload=%u eif_empty_ch0=%u/%u ch1=%u/%u ch2=%u/%u ch4=%u/%u ch5=%u/%u "
           "madamclip=%u fifo0=%08x/%d/%08x/%d fifo1=%08x/%d/%08x/%d fifo2=%08x/%d/%08x/%d fifo4=%08x/%d/%08x/%d fifo5=%08x/%d/%08x/%d level_reassert=%u/%08x dspp_ctl=%u/%08x nongw=%u dspp_rst=%u/%08x reload_dma_block=%u/%08x nmem_r=%u/%08x xbusdma=%u/%08x/%u xbuswin=%u/%u timer120=%u/%u/%u\n",
           prefix ? prefix : "diag", frame,
           threedoh_core_last_fault_type(), threedoh_core_last_fault_pc(), threedoh_core_last_fault_address(),
           threedoh_core_arm_current_pc(), threedoh_core_arm_current_cpsr(),
           arm.USER[0], arm.USER[1], arm.USER[2], arm.USER[3], arm.USER[4],
           arm.USER[9], arm.USER[10], arm.USER[11], arm.USER[12], arm.USER[14],
           threedoh_core_arm_fiq_entry_count(),
           threedoh_core_arm_unaligned_prefetch_count(),
           threedoh_core_arm_unaligned_prefetch_last(),
           threedoh_core_arm_unaligned_prefetch_fetch(),
           threedoh_core_arm_mirrored_prefetch_count(),
           threedoh_core_arm_mirrored_prefetch_last(),
           threedoh_core_arm_mirrored_prefetch_fetch(),
           threedoh_core_arm_highram_read_count(), threedoh_core_arm_highram_first_read(), threedoh_core_arm_highram_last_read(),
           threedoh_core_arm_highram_write_count(), threedoh_core_arm_highram_first_write(), threedoh_core_arm_highram_last_write(),
           threedoh_core_dsp_resource_fault_count(), threedoh_core_dsp_resource_mirror_fault_count(),
           threedoh_core_dsp_last_resource_fault_address(), threedoh_core_dsp_last_resource_fault_detail(),
           threedoh_core_dsp_run_start_count(), threedoh_core_dsp_run_stop_count(),
           threedoh_core_dsp_reset_count(), threedoh_core_dsp_int_write_count(), threedoh_core_dsp_last_int_value(),
           threedoh_core_dsp_current_pc(), threedoh_core_dsp_current_status(),
           threedoh_core_dsp_counter_value(), threedoh_core_dsp_reload_value(),
           threedoh_core_dsp_audio_tick_count(), threedoh_core_dsp_counter_reload_count(),
           threedoh_core_dsp_program_frame_count(), threedoh_core_dsp_sleep_count(),
           threedoh_core_dsp_deferred_tick_count(), threedoh_core_dsp_multi_reload_count(),
           threedoh_core_dsp_audlock_write_count(), threedoh_core_dsp_audlock_reset_count(),
           threedoh_core_dsp_last_audio_status_value(),
           threedoh_core_dsp_arm_sema_write_count(), threedoh_core_dsp_arm_sema_read_count(),
           threedoh_core_dsp_dsp_sema_write_count(), threedoh_core_dsp_dsp_sema_ack_count(),
           threedoh_core_dsp_cpu_supply_write_count(), threedoh_core_dsp_cpu_supply_read_count(),
           threedoh_core_dsp_cpu_supply_random_read_count(), threedoh_core_dsp_last_cpu_supply_channel(),
           threedoh_core_clio_fiq_generate_count(), threedoh_core_clio_fiq_need_count(),
           threedoh_core_clio_last_fiq_reason1(), threedoh_core_clio_last_fiq_reason2(),
           threedoh_core_clio_irq0_pending(), threedoh_core_clio_irq0_mask(),
           threedoh_core_clio_irq1_pending(), threedoh_core_clio_irq1_mask(),
           threedoh_core_clio_eififo_read_count(), threedoh_core_clio_eififo_empty_read_count(),
           threedoh_core_clio_eififo_reload_count(), threedoh_core_clio_eofifo_write_count(),
           threedoh_core_clio_eofifo_disabled_write_count(), threedoh_core_clio_eofifo_full_count(),
           threedoh_core_clio_last_fifo_event(),
           threedoh_core_clio_last_eififo_empty_channel(), threedoh_core_clio_last_eififo_reload_channel(),
           threedoh_core_clio_eififo_empty_channel_count(0), threedoh_core_clio_eififo_reload_channel_count(0),
           threedoh_core_clio_eififo_empty_channel_count(1), threedoh_core_clio_eififo_reload_channel_count(1),
           threedoh_core_clio_eififo_empty_channel_count(2), threedoh_core_clio_eififo_reload_channel_count(2),
           threedoh_core_clio_eififo_empty_channel_count(4), threedoh_core_clio_eififo_reload_channel_count(4),
           threedoh_core_clio_eififo_empty_channel_count(5), threedoh_core_clio_eififo_reload_channel_count(5),
           threedoh_core_madam_soft_clip_count(),
           _clio_FIFOStruct(0x400), (int)_clio_FIFOStruct(0x404), _clio_FIFOStruct(0x408), (int)_clio_FIFOStruct(0x40c),
           _clio_FIFOStruct(0x410), (int)_clio_FIFOStruct(0x414), _clio_FIFOStruct(0x418), (int)_clio_FIFOStruct(0x41c),
           _clio_FIFOStruct(0x420), (int)_clio_FIFOStruct(0x424), _clio_FIFOStruct(0x428), (int)_clio_FIFOStruct(0x42c),
           _clio_FIFOStruct(0x440), (int)_clio_FIFOStruct(0x444), _clio_FIFOStruct(0x448), (int)_clio_FIFOStruct(0x44c),
           _clio_FIFOStruct(0x450), (int)_clio_FIFOStruct(0x454), _clio_FIFOStruct(0x458), (int)_clio_FIFOStruct(0x45c),
           _clio_GetFifoLevelReassertCount(), _clio_GetFifoLevelReassertMask(),
           threedoh_core_clio_dspp_control_write_count(), threedoh_core_clio_dspp_control_last_value(),
           threedoh_core_clio_dspp_control_non_gw_count(), threedoh_core_clio_dspp_reset_write_count(),
           threedoh_core_clio_dspp_reset_last_value(),
           threedoh_core_clio_fifo_reload_dma_block_count(),
           threedoh_core_clio_fifo_last_reload_dma_block_channel(),
           threedoh_core_clio_dspp_nmem_read_count(),
           threedoh_core_clio_dspp_nmem_last_read_address(),
           threedoh_core_clio_xbus_dma_pulse_count(),
           threedoh_core_clio_xbus_dma_last_addr(),
           threedoh_core_clio_xbus_dma_last_len(),
           threedoh_core_clio_xbus_dma_timer_accum(),
           threedoh_core_clio_xbus_dma_timer_window(),
           threedoh_core_clio_xbus_timer120_adjust_count(),
           threedoh_core_clio_xbus_timer120_last_in(),
           threedoh_core_clio_xbus_timer120_last_out());
}

static void maybe_dump_ram(void)
{
    const char *dump_path = getenv("THREEDOH_HEADLESS_DUMP_RAM");
    if (dump_path && *dump_path) {
        FILE *dfp = fopen(dump_path, "wb");
        if (dfp) {
            fwrite(Getp_RAMS(), 1, 3u * 1024u * 1024u, dfp);
            fclose(dfp);
        }
    }
}

static void write_le16(FILE *fp, unsigned int v)
{
    fputc((int)(v & 0xff), fp);
    fputc((int)((v >> 8) & 0xff), fp);
}

static void write_le32(FILE *fp, unsigned int v)
{
    fputc((int)(v & 0xff), fp);
    fputc((int)((v >> 8) & 0xff), fp);
    fputc((int)((v >> 16) & 0xff), fp);
    fputc((int)((v >> 24) & 0xff), fp);
}

static int save_bmp(const char *path, const unsigned char *pixels,
                    int width, int height, int pitch, int pixel_bytes)
{
    FILE *fp;
    unsigned int row_bytes;
    unsigned int file_size;
    int y;

    if (!path || !*path || !pixels || width <= 0 || height <= 0 || pitch <= 0)
        return 0;
    fp = fopen(path, "wb");
    if (!fp)
        return 0;

    row_bytes = (unsigned int)((width * 3 + 3) & ~3);
    file_size = 54U + row_bytes * (unsigned int)height;
    fputc('B', fp); fputc('M', fp);
    write_le32(fp, file_size);
    write_le16(fp, 0); write_le16(fp, 0);
    write_le32(fp, 54);
    write_le32(fp, 40);
    write_le32(fp, (unsigned int)width);
    write_le32(fp, (unsigned int)height);
    write_le16(fp, 1);
    write_le16(fp, 24);
    write_le32(fp, 0);
    write_le32(fp, row_bytes * (unsigned int)height);
    write_le32(fp, 2835); write_le32(fp, 2835);
    write_le32(fp, 0); write_le32(fp, 0);

    for (y = height - 1; y >= 0; y--) {
        const unsigned char *row = pixels + y * pitch;
        int x;
        for (x = 0; x < width; x++) {
            unsigned char bgr[3];
            if (pixel_bytes == 4) {
                const unsigned char *px = row + x * 4;
                bgr[0] = px[2]; bgr[1] = px[1]; bgr[2] = px[0];
            } else {
                uint16_t v = (uint16_t)row[x * 2] | ((uint16_t)row[x * 2 + 1] << 8);
                bgr[2] = (unsigned char)(((v >> 11) & 0x1f) * 255 / 31);
                bgr[1] = (unsigned char)(((v >> 5) & 0x3f) * 255 / 63);
                bgr[0] = (unsigned char)((v & 0x1f) * 255 / 31);
            }
            fwrite(bgr, 1, 3, fp);
        }
        for (x = width * 3; x < (int)row_bytes; x++)
            fputc(0, fp);
    }
    return fclose(fp) == 0;
}

static void maybe_dump_frame(const unsigned char *framebuffer, const threedoh_core *core)
{
    const char *path = getenv("THREEDOH_HEADLESS_DUMP_BMP");
    if (path && *path) {
        if (!save_bmp(path, framebuffer,
                      threedoh_core_visible_width(core),
                      threedoh_core_visible_height(core),
                      threedoh_core_max_visible_width() * THREEDOH_PIXEL_BYTES,
                      THREEDOH_PIXEL_BYTES))
            fprintf(stderr, "could not write bmp: %s\n", path);
    }
}

static int parse_video_mode_name(const char *s)
{
    if (!s || !*s)
        return THREEDOH_VIDEO_AUTO;
    if (!strcmp(s, "pal") || !strcmp(s, "pal1") || !strcmp(s, "pal2") ||
        !strcmp(s, "PAL") || !strcmp(s, "PAL1") || !strcmp(s, "PAL2"))
        return THREEDOH_VIDEO_PAL;
    if (!strcmp(s, "ntsc") || !strcmp(s, "NTSC"))
        return THREEDOH_VIDEO_NTSC;
    return THREEDOH_VIDEO_AUTO;
}

static long parse_long(const char *s, long fallback)
{
    char *end = NULL;
    long v;
    if (!s || !*s)
        return fallback;
    v = strtol(s, &end, 10);
    if (!end || *end || v < 0)
        return fallback;
    return v;
}

int main(int argc, char **argv)
{
    threedoh_core *core;
    unsigned char *framebuffer;
    threedoh_input_script script;
    const char *script_error = NULL;
    const char *bios;
    const char *disc;
    const char *script_path;
    long frames;
    long report_every;
    long frame;
    int rc = 0;

    if (argc < 5) {
        fprintf(stderr, "usage: %s bios.bin disc.iso script.input|- frames [report_every]\n", argv[0]);
        return 2;
    }

    bios = argv[1];
    disc = argv[2];
    script_path = argv[3];
    frames = parse_long(argv[4], 0);
    report_every = (argc >= 6) ? parse_long(argv[5], 300) : 300;
    if (frames <= 0)
        frames = 1;
    if (report_every <= 0)
        report_every = frames + 1;

    framebuffer = (unsigned char *)calloc(1, (size_t)threedoh_core_max_visible_width() *
                                          (size_t)threedoh_core_max_visible_height() *
                                          THREEDOH_PIXEL_BYTES);
    core = (threedoh_core *)calloc(1, threedoh_core_size());
    if (!framebuffer || !core) {
        fprintf(stderr, "allocation failed\n");
        free(framebuffer);
        free(core);
        return 2;
    }

    threedoh_input_script_init(&script);
    if (script_path && strcmp(script_path, "-") != 0) {
        if (!threedoh_input_script_parse_file(&script, script_path, &script_error)) {
            fprintf(stderr, "input-script parse failed: %s\n", script_error ? script_error : "unknown error");
            free(framebuffer);
            free(core);
            return 2;
        }
    }

    if (!soundInit() || !inputInit()) {
        fprintf(stderr, "headless backend init failed\n");
        free(framebuffer);
        free(core);
        return 2;
    }

    threedoh_core_construct(core);
    threedoh_core_set_video_standard_mode(core, parse_video_mode_name(getenv("THREEDOH_HEADLESS_VIDEO")));
    threedoh_core_set_strict_dsp_resources(core, 1);

    if (!threedoh_core_start(core, bios, disc)) {
        fprintf(stderr, "core start failed\n");
        rc = 1;
        goto out;
    }

    {
        const char *load_state_path = getenv("THREEDOH_HEADLESS_LOAD_STATE");
        if (load_state_path && *load_state_path) {
            if (!threedoh_core_load_state_file(core, load_state_path)) {
                fprintf(stderr, "could not load state: %s\n", load_state_path);
                rc = 1;
                goto out;
            }
        }
    }


    maybe_begin_audio_dump();
    threedoh_input_script_compile(&script, threedoh_core_frame_rate_hz(core));
    printf("headless: disc=%s frames=%ld video=%s %dx%d %dHz script=%d strict_dsp=%d\n",
           disc, frames, threedoh_core_video_standard_name(core),
           threedoh_core_visible_width(core), threedoh_core_visible_height(core),
           threedoh_core_frame_rate_hz(core), threedoh_input_script_count(&script),
           threedoh_core_strict_dsp_resources());

    for (frame = 0; frame < frames; frame++) {
        threedoh_input_script_apply(&script, (unsigned long)frame);
        if (!threedoh_core_frame(core, framebuffer,
                                 threedoh_core_max_visible_width(),
                                 threedoh_core_max_visible_height())) {
            print_diag("stop", (unsigned long)frame);
            maybe_dump_ram();
            rc = 1;
            goto out;
        }
        maybe_append_audio_frame();
        if ((frame % report_every) == 0)
            print_diag("frame", (unsigned long)frame);
    }

    print_diag("done", (unsigned long)frames);
    maybe_dump_ram();
    maybe_dump_frame(framebuffer, core);

out:
    threedoh_core_stop(core);
    inputClose();
    maybe_end_audio_dump();
    soundClose();
    free(framebuffer);
    free(core);
    return rc;
}
