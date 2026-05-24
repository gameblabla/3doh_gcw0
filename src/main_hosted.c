#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif
#ifdef THREEDOH_PLATFORM_SDL3
#include <SDL3/SDL_main.h>
#endif

#include "core/threedoh_core.h"
#include "platform/threedoh_platform.h"
#include "input.h"
#include "input_script.h"
#include "sound.h"

static void default_bios_path(char *out, size_t out_size)
{
#if defined(_WIN32)
    snprintf(out, out_size, "bios.bin");
#else
    const char *home = getenv("HOME");
    if (home && *home)
        snprintf(out, out_size, "%s/.3doh/bios.bin", home);
    else
        snprintf(out, out_size, "bios.bin");
#endif
}

static int make_dir(const char *dir)
{
#if defined(_WIN32)
    return _mkdir(dir) == 0 || errno == EEXIST;
#else
    return mkdir(dir, 0755) == 0 || errno == EEXIST;
#endif
}

static int ensure_unix_home_dir(void)
{
#if !defined(_WIN32)
    char dir[512];
    const char *home = getenv("HOME");
    if (!home || !*home)
        return 1;
    snprintf(dir, sizeof(dir), "%s/.3doh", home);
    if (access(dir, F_OK) == -1) {
        if (mkdir(dir, 0755) == -1) {
            perror("mkdir ~/.3doh");
            return 0;
        }
        fprintf(stderr, "Created %s. Put bios.bin there, or pass an explicit BIOS path.\n", dir);
    }
#endif
    return 1;
}

static int file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;
    fclose(fp);
    return 1;
}

static const char *basename_ptr(const char *path)
{
    const char *a;
    const char *b;
    if (!path || !*path)
        return "game";
    a = strrchr(path, '/');
    b = strrchr(path, '\\');
    if (a && b)
        return (a > b) ? a + 1 : b + 1;
    if (a)
        return a + 1;
    if (b)
        return b + 1;
    return path;
}

static void sanitize_name(const char *in, char *out, size_t out_size)
{
    size_t i;
    size_t j = 0;
    if (!out || !out_size)
        return;
    if (!in)
        in = "game";
    for (i = 0; in[i] && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '.' || c == '-' || c == '_')
            out[j++] = (char)c;
        else
            out[j++] = '_';
    }
    if (j == 0 && out_size > 1) {
        out[j++] = 'g';
        out[j++] = 'a';
        out[j++] = 'm';
        out[j++] = 'e';
    }
    out[j] = '\0';
}

static int state_dir(char *out, size_t out_size)
{
#if defined(_WIN32)
    const char *base = getenv("APPDATA");
    if (base && *base) {
        char root[512];
        snprintf(root, sizeof(root), "%s\\3doh", base);
        make_dir(root);
        snprintf(out, out_size, "%s\\states", root);
        return make_dir(out);
    }
    snprintf(out, out_size, "states");
    return make_dir(out);
#else
    const char *home = getenv("HOME");
    if (home && *home) {
        char root[512];
        snprintf(root, sizeof(root), "%s/.3doh", home);
        make_dir(root);
        snprintf(out, out_size, "%s/states", root);
        return make_dir(out);
    }
    snprintf(out, out_size, "states");
    return make_dir(out);
#endif
}

static int state_slot_path(const char *iso_path, int slot, char *out, size_t out_size)
{
    char dir[512];
    char name[256];
    if (slot < 1)
        slot = 1;
    if (!state_dir(dir, sizeof(dir)))
        return 0;
    sanitize_name(basename_ptr(iso_path), name, sizeof(name));
#if defined(_WIN32)
    snprintf(out, out_size, "%s\\%s.slot%d.3dohstate", dir, name, slot);
#else
    snprintf(out, out_size, "%s/%s.slot%d.3dohstate", dir, name, slot);
#endif
    return 1;
}


static int parse_video_mode_arg(const char *arg, int *mode)
{
    if (!arg || !mode)
        return 0;
    if (strcmp(arg, "--pal") == 0 || strcmp(arg, "-pal") == 0 ||
        strcmp(arg, "--pal1") == 0 || strcmp(arg, "-pal1") == 0 ||
        strcmp(arg, "--force-pal") == 0) {
        *mode = THREEDOH_VIDEO_PAL;
        return 1;
    }
    if (strcmp(arg, "--ntsc") == 0 || strcmp(arg, "-ntsc") == 0) {
        *mode = THREEDOH_VIDEO_NTSC;
        return 1;
    }
    if (strcmp(arg, "--auto") == 0 || strcmp(arg, "-auto") == 0) {
        *mode = THREEDOH_VIDEO_AUTO;
        return 1;
    }
    return 0;
}

static int parse_fault_mode_arg(const char *arg, int *strict_faults)
{
    if (!arg || !strict_faults)
        return 0;
    if (strcmp(arg, "--strict-bus") == 0 || strcmp(arg, "--strict-faults") == 0 ||
        strcmp(arg, "--accurate-faults") == 0) {
        *strict_faults = 1;
        return 1;
    }
    if (strcmp(arg, "--compat-bus") == 0 || strcmp(arg, "--no-strict-bus") == 0 ||
        strcmp(arg, "--legacy-faults") == 0) {
        *strict_faults = 0;
        return 1;
    }
    return 0;
}


static int parse_dsp_fault_mode_arg(const char *arg, int *strict_dsp)
{
    if (!arg || !strict_dsp)
        return 0;
    if (strcmp(arg, "--strict-dsp") == 0 || strcmp(arg, "--strict-dspp") == 0 ||
        strcmp(arg, "--strict-dsp-resources") == 0) {
        *strict_dsp = 1;
        return 1;
    }
    if (strcmp(arg, "--compat-dsp") == 0 || strcmp(arg, "--compat-dspp") == 0 ||
        strcmp(arg, "--no-strict-dsp") == 0 || strcmp(arg, "--no-strict-dsp-resources") == 0) {
        *strict_dsp = 0;
        return 1;
    }
    return 0;
}

static int parse_madam_fault_mode_arg(const char *arg, int *strict_madam)
{
    if (!arg || !strict_madam)
        return 0;
    if (strcmp(arg, "--strict-madam") == 0 || strcmp(arg, "--strict-madam-runaway") == 0) {
        *strict_madam = 1;
        return 1;
    }
    if (strcmp(arg, "--compat-madam") == 0 || strcmp(arg, "--no-strict-madam") == 0) {
        *strict_madam = 0;
        return 1;
    }
    return 0;
}

static const char *video_mode_label(int mode)
{
    switch (mode) {
    case THREEDOH_VIDEO_PAL: return "PAL 50 Hz";
    case THREEDOH_VIDEO_NTSC: return "NTSC 60 Hz";
    default: return "Auto";
    }
}

static void host_sleep_ms(unsigned int ms)
{
#if defined(_WIN32)
    Sleep(ms);
#else
    usleep(ms * 1000U);
#endif
}

static void set_statusf(threedoh_platform *platform, const char *fmt, const char *arg, int slot)
{
    char buffer[512];
    if (slot >= 0)
        snprintf(buffer, sizeof(buffer), fmt, arg ? arg : "", slot);
    else
        snprintf(buffer, sizeof(buffer), fmt, arg ? arg : "");
    threedoh_platform_set_status(platform, buffer);
}


static int option_value(int argc, char **argv, int *index, const char *name,
                        const char *arg, const char **out)
{
    size_t len = strlen(name);
    if (strcmp(arg, name) == 0) {
        if (*index + 1 >= argc) {
            fprintf(stderr, "%s requires a value.\n", name);
            return -1;
        }
        *out = argv[++(*index)];
        return 1;
    }
    if (strncmp(arg, name, len) == 0 && arg[len] == '=') {
        *out = arg + len + 1;
        return 1;
    }
    return 0;
}

static int parse_long_value(const char *text, long *out)
{
    char *end = NULL;
    long v;
    if (!text || !*text || !out)
        return 0;
    errno = 0;
    v = strtol(text, &end, 10);
    if (errno || !end || *end || v < 0)
        return 0;
    *out = v;
    return 1;
}

static int parse_double_value(const char *text, double *out)
{
    char *end = NULL;
    double v;
    if (!text || !*text || !out)
        return 0;
    errno = 0;
    v = strtod(text, &end);
    if (errno || !end || *end || v < 0.0)
        return 0;
    *out = v;
    return 1;
}

static const char *fault_type_label(uint32_t type)
{
    switch (type) {
    case 1: return "unmapped read";
    case 2: return "unmapped write";
    case 3: return "unaligned device read";
    case 4: return "unaligned device write";
    case 5: return "DSP/CLIO bounds";
    case 6: return "unaligned block transfer";
    case 7: return "unaligned prefetch";
    case 8: return "DSP runaway/no SLEEP";
    case 9: return "MADAM cel runaway";
    case 10: return "ARM CPU runaway";
    case 11: return "MMU translation fault";
    case 12: return "MMU permission fault";
    case 13: return "undefined CP15/coprocessor operation";
    case 14: return "DSPP resource/window violation";
    default: return "unknown strict fault";
    }
}

int main(int argc, char **argv)
{
    char iso_path[1024];
    char previous_iso_path[1024];
    char bios_path[512];
    threedoh_platform *platform = NULL;
    threedoh_core *core = NULL;
    unsigned char *framebuffer = NULL;
    int paused = 0;
    int rc = 1;
    int video_mode = THREEDOH_VIDEO_AUTO;
    int strict_faults = 1;
    int strict_madam = 0;
    int strict_dsp = 1;
    int positional = 0;
    threedoh_input_script input_script;
    const char *script_error = NULL;
    long stop_after_frames_arg = -1;
    double stop_after_seconds = -1.0;
    long stop_after_frames = -1;
    unsigned long emu_frame = 0;
    int i;

    iso_path[0] = 0;
    bios_path[0] = 0;
    threedoh_input_script_init(&input_script);

    for (i = 1; i < argc; i++) {
        const char *value = NULL;
        int opt;
        if (parse_video_mode_arg(argv[i], &video_mode))
            continue;
        if (parse_fault_mode_arg(argv[i], &strict_faults))
            continue;
        if (parse_dsp_fault_mode_arg(argv[i], &strict_dsp))
            continue;
        if (parse_madam_fault_mode_arg(argv[i], &strict_madam))
            continue;
        opt = option_value(argc, argv, &i, "--input", argv[i], &value);
        if (opt < 0) return 1;
        if (opt || (opt = option_value(argc, argv, &i, "--script", argv[i], &value)) != 0) {
            if (opt < 0) return 1;
            if (!threedoh_input_script_parse_inline(&input_script, value, &script_error)) {
                fprintf(stderr, "Input script parse error: %s\n", script_error ? script_error : "unknown error");
                return 1;
            }
            continue;
        }
        opt = option_value(argc, argv, &i, "--input-script", argv[i], &value);
        if (opt < 0) return 1;
        if (opt || (opt = option_value(argc, argv, &i, "--script-file", argv[i], &value)) != 0) {
            if (opt < 0) return 1;
            if (!threedoh_input_script_parse_file(&input_script, value, &script_error)) {
                fprintf(stderr, "Input script file error: %s\n", script_error ? script_error : "unknown error");
                return 1;
            }
            continue;
        }
        opt = option_value(argc, argv, &i, "--input-hold-ms", argv[i], &value);
        if (opt < 0) return 1;
        if (opt) {
            long hold;
            if (!parse_long_value(value, &hold) || hold <= 0 || hold > 5000) {
                fprintf(stderr, "Bad --input-hold-ms value: %s\n", value);
                return 1;
            }
            threedoh_input_script_set_hold_ms(&input_script, (int)hold);
            continue;
        }
        opt = option_value(argc, argv, &i, "--stop-after-frames", argv[i], &value);
        if (opt < 0) return 1;
        if (opt) {
            if (!parse_long_value(value, &stop_after_frames_arg)) {
                fprintf(stderr, "Bad --stop-after-frames value: %s\n", value);
                return 1;
            }
            continue;
        }
        opt = option_value(argc, argv, &i, "--stop-after", argv[i], &value);
        if (opt < 0) return 1;
        if (opt) {
            if (!parse_double_value(value, &stop_after_seconds)) {
                fprintf(stderr, "Bad --stop-after value: %s\n", value);
                return 1;
            }
            continue;
        }
        if (positional == 0) {
            snprintf(iso_path, sizeof(iso_path), "%s", argv[i]);
            positional++;
        } else if (positional == 1) {
            snprintf(bios_path, sizeof(bios_path), "%s", argv[i]);
            positional++;
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (!iso_path[0]) {
        fprintf(stderr, "Usage: %s <game.iso|game.cue> [bios.bin] [--auto|--ntsc|--pal|--pal1] [--strict-bus|--compat-bus] [--strict-dsp|--compat-dsp] [--strict-madam|--compat-madam]\n", argv[0]);
        fprintf(stderr, "       [--input \"1550f:P,1930f:DOWN,1980f:A\"] [--input-script file] [--input-hold-ms ms]\n");
        fprintf(stderr, "       [--stop-after seconds | --stop-after-frames frames]\n");
        return 1;
    }

    if (!bios_path[0])
        default_bios_path(bios_path, sizeof(bios_path));

    if (!ensure_unix_home_dir())
        return 1;
    if (!file_exists(bios_path)) {
        fprintf(stderr, "BIOS not found: %s\n", bios_path);
        return 1;
    }
    if (!file_exists(iso_path)) {
        fprintf(stderr, "ISO/CUE not found: %s\n", iso_path);
        return 1;
    }

    platform = threedoh_platform_create();
    if (!platform) {
        fprintf(stderr, "Unable to allocate platform backend.\n");
        goto out;
    }

    if (!threedoh_platform_init(platform, "3DOh", threedoh_core_max_visible_width(),
                                threedoh_core_max_visible_height(), THREEDOH_PIXEL_BYTES)) {
        fprintf(stderr, "Unable to initialize platform backend.\n");
        goto out;
    }

    if (!soundInit()) {
        fprintf(stderr, "Unable to initialize audio backend.\n");
        goto out;
    }
    if (!inputInit()) {
        fprintf(stderr, "Unable to initialize input backend.\n");
        goto out;
    }

    core = (threedoh_core *)calloc(1, threedoh_core_size());
    framebuffer = (unsigned char *)calloc(threedoh_core_max_visible_width() * threedoh_core_max_visible_height(),
                                          THREEDOH_PIXEL_BYTES);
    if (!core || !framebuffer) {
        fprintf(stderr, "Out of memory.\n");
        goto out;
    }

    threedoh_core_construct(core);
    threedoh_core_set_video_standard_mode(core, video_mode);
    threedoh_core_set_strict_bus_faults(core, strict_faults);
    threedoh_core_set_strict_dsp_resources(core, strict_dsp);
    threedoh_core_set_strict_madam_runaway_faults(core, strict_madam);
    if (!threedoh_core_start(core, bios_path, iso_path)) {
        fprintf(stderr, "Unable to start emulator with BIOS '%s' and disc '%s'.\n",
                bios_path, iso_path);
        goto out;
    }
    threedoh_platform_set_runtime_state(platform, 1, paused, iso_path);
    threedoh_platform_set_frame_rate(platform, threedoh_core_frame_rate_hz(core));
    threedoh_platform_set_video_standard(platform, threedoh_core_video_standard_mode(core),
                                         threedoh_core_active_video_standard(core),
                                         threedoh_core_frame_rate_hz(core));
    threedoh_input_script_compile(&input_script, threedoh_core_frame_rate_hz(core));
    if (stop_after_frames_arg >= 0)
        stop_after_frames = stop_after_frames_arg;
    else if (stop_after_seconds >= 0.0)
        stop_after_frames = (long)(stop_after_seconds * (double)threedoh_core_frame_rate_hz(core) + 0.5);
    if (threedoh_input_script_is_enabled(&input_script))
        fprintf(stderr, "Loaded input script with %d scheduled button edges at %d Hz.\n",
                threedoh_input_script_count(&input_script), threedoh_core_frame_rate_hz(core));
    {
        char startup_status[512];
        snprintf(startup_status, sizeof(startup_status), "Running %s at %s (%s, %s bus faults, DSPP %s, MADAM %s). F1/Esc opens menu.",
                 basename_ptr(iso_path), threedoh_core_video_standard_name(core), video_mode_label(video_mode),
                 threedoh_core_strict_bus_faults(core) ? "strict" : "compat",
                 threedoh_core_strict_dsp_resources() ? "strict" : "compat",
                 threedoh_core_strict_madam_runaway_faults() ? "strict" : "soft-clip");
        threedoh_platform_set_status(platform, startup_status);
    }

    while (!threedoh_platform_should_quit(platform) && !isexit) {
        threedoh_platform_command command;

        threedoh_platform_poll(platform);
        while (threedoh_platform_take_command(platform, &command)) {
            if (command.flags & THREEDOH_PLATFORM_CMD_TOGGLE_FULLSCREEN)
                threedoh_platform_toggle_fullscreen(platform);

            if (command.flags & THREEDOH_PLATFORM_CMD_SET_VIDEO_STANDARD) {
                int requested_mode = command.slot;
                if (requested_mode != THREEDOH_VIDEO_AUTO &&
                    requested_mode != THREEDOH_VIDEO_NTSC &&
                    requested_mode != THREEDOH_VIDEO_PAL)
                    requested_mode = THREEDOH_VIDEO_AUTO;
                video_mode = requested_mode;
                threedoh_core_set_video_standard_mode(core, video_mode);
                threedoh_core_apply_video_standard_mode(core, bios_path, iso_path);
                threedoh_platform_set_frame_rate(platform, threedoh_core_frame_rate_hz(core));
                threedoh_platform_set_video_standard(platform, threedoh_core_video_standard_mode(core),
                                                     threedoh_core_active_video_standard(core),
                                                     threedoh_core_frame_rate_hz(core));
                {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Video standard: %s (%s).",
                             threedoh_core_video_standard_name(core), video_mode_label(video_mode));
                    threedoh_platform_set_status(platform, msg);
                }
            }

            if (command.flags & THREEDOH_PLATFORM_CMD_TOGGLE_PAUSE) {
                paused = !paused;
                threedoh_platform_set_runtime_state(platform, 1, paused, iso_path);
                threedoh_platform_set_status(platform, paused ? "Emulation paused." : "Emulation resumed.");
            }

            if (command.flags & THREEDOH_PLATFORM_CMD_PAUSE) {
                paused = 1;
                threedoh_platform_set_runtime_state(platform, 1, paused, iso_path);
            }

            if (command.flags & THREEDOH_PLATFORM_CMD_RESUME) {
                paused = 0;
                threedoh_platform_set_runtime_state(platform, 1, paused, iso_path);
            }

            if (command.flags & THREEDOH_PLATFORM_CMD_SOFT_RESET) {
                if (threedoh_core_soft_reset(core)) {
                    memset(framebuffer, 0, threedoh_core_max_visible_width() * threedoh_core_max_visible_height() * THREEDOH_PIXEL_BYTES);
                    paused = 0;
                    emu_frame = 0;
                    input_script.cursor = 0;
                    threedoh_platform_set_runtime_state(platform, 1, paused, iso_path);
                    threedoh_platform_set_frame_rate(platform, threedoh_core_frame_rate_hz(core));
                    threedoh_platform_set_video_standard(platform, threedoh_core_video_standard_mode(core), threedoh_core_active_video_standard(core), threedoh_core_frame_rate_hz(core));
                    threedoh_platform_set_status(platform, "Soft reset complete.");
                } else {
                    threedoh_platform_set_status(platform, "Soft reset failed.");
                }
            }

            if (command.flags & THREEDOH_PLATFORM_CMD_SAVE_STATE) {
                char path[1024];
                int slot = command.slot > 0 ? command.slot : 1;
                if (state_slot_path(iso_path, slot, path, sizeof(path)) &&
                    threedoh_core_save_state_file(core, path))
                    set_statusf(platform, "Saved state slot %s%d.", "", slot);
                else
                    set_statusf(platform, "Could not save state slot %s%d.", "", slot);
            }

            if (command.flags & THREEDOH_PLATFORM_CMD_LOAD_STATE) {
                char path[1024];
                int slot = command.slot > 0 ? command.slot : 1;
                if (state_slot_path(iso_path, slot, path, sizeof(path)) &&
                    threedoh_core_load_state_file(core, path))
                    set_statusf(platform, "Loaded state slot %s%d.", "", slot);
                else
                    set_statusf(platform, "No valid state in slot %s%d.", "", slot);
            }

            if ((command.flags & THREEDOH_PLATFORM_CMD_LOAD_DISC) && command.path[0]) {
                if (!file_exists(command.path)) {
                    set_statusf(platform, "Disc file not found: %s", command.path, -1);
                } else {
                    int was_paused = paused;
                    snprintf(previous_iso_path, sizeof(previous_iso_path), "%s", iso_path);
                    threedoh_core_stop(core);
                    memset(framebuffer, 0, threedoh_core_max_visible_width() * threedoh_core_max_visible_height() * THREEDOH_PIXEL_BYTES);
                    if (threedoh_core_start(core, bios_path, command.path)) {
                        snprintf(iso_path, sizeof(iso_path), "%s", command.path);
                        paused = was_paused;
                        emu_frame = 0;
                        input_script.cursor = 0;
                        threedoh_platform_set_runtime_state(platform, 1, paused, iso_path);
                        threedoh_platform_set_frame_rate(platform, threedoh_core_frame_rate_hz(core));
                        threedoh_platform_set_video_standard(platform, threedoh_core_video_standard_mode(core),
                                                             threedoh_core_active_video_standard(core),
                                                             threedoh_core_frame_rate_hz(core));
                        {
                            char msg[512];
                            snprintf(msg, sizeof(msg), paused ? "Loaded new disc paused: %s at %s" : "Loaded new disc: %s at %s",
                                     basename_ptr(iso_path), threedoh_core_video_standard_name(core));
                            threedoh_platform_set_status(platform, msg);
                        }
                    } else {
                        threedoh_platform_set_status(platform, "Failed to load new disc; attempting to restore previous disc.");
                        if (threedoh_core_start(core, bios_path, previous_iso_path)) {
                            snprintf(iso_path, sizeof(iso_path), "%s", previous_iso_path);
                            threedoh_platform_set_runtime_state(platform, 1, paused, iso_path);
                            threedoh_platform_set_frame_rate(platform, threedoh_core_frame_rate_hz(core));
                            threedoh_platform_set_video_standard(platform, threedoh_core_video_standard_mode(core), threedoh_core_active_video_standard(core), threedoh_core_frame_rate_hz(core));
                        } else {
                            threedoh_platform_set_status(platform, "Failed to restore previous disc. Quit and restart.");
                            isexit = 1;
                        }
                    }
                }
            }

            if (command.flags & THREEDOH_PLATFORM_CMD_QUIT) {
                isexit = 1;
                break;
            }
        }

        if (!paused) {
            threedoh_input_script_apply(&input_script, emu_frame);
            if (!threedoh_core_frame(core, framebuffer,
                                     threedoh_core_max_visible_width(), threedoh_core_max_visible_height())) {
                uint32_t fault = threedoh_core_last_fault_type();
                if (fault) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Strict fault: %s at PC=%08x address=%08x cp15=%08x ops=%u cflush=%u wbflush=%u madamclip=%u dspres=%u dspmir=%u dspaddr=%08x dspdetail=%08x.",
                             fault_type_label(fault),
                             (unsigned)threedoh_core_last_fault_pc(),
                             (unsigned)threedoh_core_last_fault_address(),
                             (unsigned)threedoh_core_cp15_control(),
                             (unsigned)threedoh_core_cp15_ops(),
                             (unsigned)threedoh_core_cp15_cache_flushes(),
                             (unsigned)threedoh_core_cp15_writebuffer_flushes(),
                             (unsigned)threedoh_core_madam_soft_clip_count(),
                             (unsigned)threedoh_core_dsp_resource_fault_count(),
                             (unsigned)threedoh_core_dsp_resource_mirror_fault_count(),
                             (unsigned)threedoh_core_dsp_last_resource_fault_address(),
                             (unsigned)threedoh_core_dsp_last_resource_fault_detail());
                    fprintf(stderr, "%s\n", msg);
                    threedoh_platform_set_status(platform, msg);
                }
                break;
            }
            emu_frame++;
            if (stop_after_frames >= 0 && (long)emu_frame >= stop_after_frames) {
                fprintf(stderr, "Reached requested stop frame %lu.\n", emu_frame);
                isexit = 1;
            }
        }
        threedoh_platform_present(platform, framebuffer,
                                  threedoh_core_visible_width(core), threedoh_core_visible_height(core),
                                  threedoh_core_max_visible_width() * THREEDOH_PIXEL_BYTES);
        threedoh_platform_wait_for_next_frame(platform);
    }

    rc = 0;

out:
    if (core) {
        threedoh_core_stop(core);
        free(core);
    }
    inputClose();
    soundClose();
    if (platform)
        threedoh_platform_destroy(platform);
    free(framebuffer);
    return rc;
}
