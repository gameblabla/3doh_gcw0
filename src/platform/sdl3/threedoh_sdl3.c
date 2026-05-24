#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#include "platform/threedoh_platform.h"
#include "platform/common/input_packet.h"
#include "platform/common/autocrop.h"
#include "input.h"
#include "sound.h"

#define UI_SLOTS 5
#define UI_STATUS_LEN 256
#define UI_PATH_LEN 1024
#define UI_STATUS_TIMEOUT_NS UINT64_C(1500000000)
#define INPUT_EDGE_QUEUE_LEN 256

struct threedoh_platform {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int width;
    int height;
    int pixel_bytes;
    int quit;
    bool fullscreen;

    bool menu_open;
    bool menu_paused_by_ui;
    int section;
    int selected;
    int remap_button;
    bool integer_scale;
    bool linear_filter;
    bool auto_crop;
    bool running;
    bool paused;
    bool input_recording;
    bool muted;
    bool frame_limiter;
    bool vsync;
    int target_hz;
    int video_mode;
    int active_video_standard;
    uint64_t next_frame_ns;
    bool limiter_primed;
    char disc_path[UI_PATH_LEN];
    char status[UI_STATUS_LEN];
    uint64_t status_until_ns;
    threedoh_autocrop_state crop_state;
    threedoh_crop_rect window_crop;
    bool window_crop_valid;

    SDL_Scancode key_map[THREEDOH_BUTTON_COUNT];
    int joy_map[THREEDOH_BUTTON_COUNT];
    threedoh_platform_command pending;
    threedoh_platform_input_edge input_edges[INPUT_EDGE_QUEUE_LEN];
    int input_edge_head;
    int input_edge_tail;

    volatile int dialog_ready;
    volatile int dialog_active;
    char dialog_path[UI_PATH_LEN];
};

static inputState g_input_state[6];
static unsigned char g_input_packet[16];
int isexit = 0;

static SDL_AudioStream *g_audio_stream;
static int g_audio_running;
static int g_audio_muted;
static threedoh_platform *g_active_platform;

static const int g_buttons[] = {
    THREEDOH_BUTTON_UP, THREEDOH_BUTTON_DOWN, THREEDOH_BUTTON_LEFT,
    THREEDOH_BUTTON_RIGHT, THREEDOH_BUTTON_A, THREEDOH_BUTTON_B,
    THREEDOH_BUTTON_C, THREEDOH_BUTTON_X, THREEDOH_BUTTON_L,
    THREEDOH_BUTTON_R, THREEDOH_BUTTON_P
};

static const char *button_name(int button)
{
    switch (button) {
    case THREEDOH_BUTTON_UP: return "D-pad Up";
    case THREEDOH_BUTTON_DOWN: return "D-pad Down";
    case THREEDOH_BUTTON_LEFT: return "D-pad Left";
    case THREEDOH_BUTTON_RIGHT: return "D-pad Right";
    case THREEDOH_BUTTON_A: return "A";
    case THREEDOH_BUTTON_B: return "B";
    case THREEDOH_BUTTON_C: return "C";
    case THREEDOH_BUTTON_X: return "X";
    case THREEDOH_BUTTON_L: return "L";
    case THREEDOH_BUTTON_R: return "R";
    case THREEDOH_BUTTON_P: return "P / Start";
    default: return "Unknown";
    }
}

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || !dst_size)
        return;
    if (!src)
        src = "";
    snprintf(dst, dst_size, "%s", src);
}

static void set_status(threedoh_platform *p, const char *message)
{
    if (!p)
        return;
    safe_copy(p->status, sizeof(p->status), message);
    p->status_until_ns = p->status[0] ? SDL_GetTicksNS() + UI_STATUS_TIMEOUT_NS : 0;
}

static void set_statusf(threedoh_platform *p, const char *fmt, ...)
{
    va_list ap;
    if (!p || !fmt)
        return;
    va_start(ap, fmt);
    vsnprintf(p->status, sizeof(p->status), fmt, ap);
    va_end(ap);
    p->status_until_ns = p->status[0] ? SDL_GetTicksNS() + UI_STATUS_TIMEOUT_NS : 0;
}

static int status_is_visible(threedoh_platform *p)
{
    if (!p || !p->status[0])
        return 0;
    if (p->status_until_ns && SDL_GetTicksNS() >= p->status_until_ns) {
        p->status[0] = 0;
        p->status_until_ns = 0;
        return 0;
    }
    return 1;
}

static const char *basename_ptr(const char *path)
{
    const char *a;
    const char *b;
    if (!path || !*path)
        return "none";
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

static int make_dir(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

static void config_path(char *out, size_t out_size)
{
#if defined(_WIN32)
    const char *base = getenv("APPDATA");
    if (base && *base) {
        char dir[UI_PATH_LEN];
        snprintf(dir, sizeof(dir), "%s\\3doh", base);
        make_dir(dir);
        snprintf(out, out_size, "%s\\sdl3_controls.cfg", dir);
        return;
    }
#endif
    {
        const char *home = getenv("HOME");
        if (home && *home) {
            char dir[UI_PATH_LEN];
            snprintf(dir, sizeof(dir), "%s/.3doh", home);
            make_dir(dir);
            snprintf(out, out_size, "%s/sdl3_controls.cfg", dir);
        } else {
            snprintf(out, out_size, "sdl3_controls.cfg");
        }
    }
}

static void set_defaults(threedoh_platform *p)
{
    int i;
    for (i = 0; i < THREEDOH_BUTTON_COUNT; i++) {
        p->key_map[i] = SDL_SCANCODE_UNKNOWN;
        p->joy_map[i] = -1;
    }
    p->key_map[THREEDOH_BUTTON_UP] = SDL_SCANCODE_UP;
    p->key_map[THREEDOH_BUTTON_DOWN] = SDL_SCANCODE_DOWN;
    p->key_map[THREEDOH_BUTTON_LEFT] = SDL_SCANCODE_LEFT;
    p->key_map[THREEDOH_BUTTON_RIGHT] = SDL_SCANCODE_RIGHT;
    p->key_map[THREEDOH_BUTTON_A] = SDL_SCANCODE_LCTRL;
    p->key_map[THREEDOH_BUTTON_B] = SDL_SCANCODE_LALT;
    p->key_map[THREEDOH_BUTTON_C] = SDL_SCANCODE_LSHIFT;
    p->key_map[THREEDOH_BUTTON_X] = SDL_SCANCODE_SPACE;
    p->key_map[THREEDOH_BUTTON_L] = SDL_SCANCODE_TAB;
    p->key_map[THREEDOH_BUTTON_R] = SDL_SCANCODE_BACKSPACE;
    p->key_map[THREEDOH_BUTTON_P] = SDL_SCANCODE_RETURN;

    p->joy_map[THREEDOH_BUTTON_A] = 0;
    p->joy_map[THREEDOH_BUTTON_B] = 1;
    p->joy_map[THREEDOH_BUTTON_C] = 2;
    p->joy_map[THREEDOH_BUTTON_X] = 3;
    p->joy_map[THREEDOH_BUTTON_L] = 4;
    p->joy_map[THREEDOH_BUTTON_R] = 5;
    p->joy_map[THREEDOH_BUTTON_P] = 7;
}

static void save_bindings(threedoh_platform *p)
{
    char path[UI_PATH_LEN];
    FILE *fp;
    int i;
    if (!p)
        return;
    config_path(path, sizeof(path));
    fp = fopen(path, "w");
    if (!fp) {
        set_status(p, "Could not save SDL3 control map.");
        return;
    }
    fprintf(fp, "# 3DOh SDL3 control bindings: button scancode joystick_button\n");
    for (i = 0; i < (int)(sizeof(g_buttons) / sizeof(g_buttons[0])); i++) {
        int b = g_buttons[i];
        fprintf(fp, "%d %d %d\n", b, (int)p->key_map[b], p->joy_map[b]);
    }
    fclose(fp);
    set_status(p, "Control map saved.");
}

static void load_bindings(threedoh_platform *p)
{
    char path[UI_PATH_LEN];
    FILE *fp;
    int button, key, joy;
    if (!p)
        return;
    set_defaults(p);
    config_path(path, sizeof(path));
    fp = fopen(path, "r");
    if (!fp)
        return;
    while (fscanf(fp, "%d %d %d", &button, &key, &joy) == 3) {
        if (button >= 0 && button < THREEDOH_BUTTON_COUNT) {
            p->key_map[button] = (SDL_Scancode)key;
            p->joy_map[button] = joy;
        }
    }
    fclose(fp);
}

static int button_from_scancode(threedoh_platform *p, SDL_Scancode scancode)
{
    int i;
    if (!p)
        return -1;
    for (i = 0; i < (int)(sizeof(g_buttons) / sizeof(g_buttons[0])); i++) {
        int b = g_buttons[i];
        if (p->key_map[b] == scancode)
            return b;
    }
    return -1;
}

static int button_from_joy_button(threedoh_platform *p, int joy_button)
{
    int i;
    if (!p)
        return -1;
    for (i = 0; i < (int)(sizeof(g_buttons) / sizeof(g_buttons[0])); i++) {
        int b = g_buttons[i];
        if (p->joy_map[b] == joy_button)
            return b;
    }
    return -1;
}

static void queue_command(threedoh_platform *p, int flags, int slot, const char *path)
{
    if (!p)
        return;
    p->pending.flags |= flags;
    p->pending.slot = slot;
    if (path)
        safe_copy(p->pending.path, sizeof(p->pending.path), path);
}

static void queue_input_edge(threedoh_platform *p, int button, int pressed)
{
    int next_tail;
    if (!p || !p->input_recording || button < 0 || button >= THREEDOH_BUTTON_EXIT)
        return;
    next_tail = (p->input_edge_tail + 1) % INPUT_EDGE_QUEUE_LEN;
    if (next_tail == p->input_edge_head)
        p->input_edge_head = (p->input_edge_head + 1) % INPUT_EDGE_QUEUE_LEN;
    p->input_edges[p->input_edge_tail].button = button;
    p->input_edges[p->input_edge_tail].pressed = pressed ? 1 : 0;
    p->input_edge_tail = next_tail;
}

static void set_button_state(threedoh_platform *p, int button, int pressed)
{
    int mask;
    int was_down;
    if (button < 0 || button >= THREEDOH_BUTTON_EXIT)
        return;
    mask = threedoh_input_mask_for_button(button);
    if (!mask)
        return;
    was_down = (g_input_state[0].buttons & mask) ? 1 : 0;
    threedoh_input_set_button(g_input_state, 0, button, pressed);
    if (was_down != (pressed ? 1 : 0))
        queue_input_edge(p, button, pressed);
}

static void release_all_game_buttons(threedoh_platform *p)
{
    int i;
    for (i = 0; i < (int)(sizeof(g_buttons) / sizeof(g_buttons[0])); i++)
        set_button_state(p, g_buttons[i], 0);
}

threedoh_platform *threedoh_platform_create(void)
{
    threedoh_platform *p = (threedoh_platform *)calloc(1, sizeof(threedoh_platform));
    if (p) {
        p->section = 0;
        p->selected = 0;
        p->remap_button = -1;
        p->integer_scale = true;
        p->linear_filter = false;
        p->auto_crop = false;
        p->frame_limiter = true;
        p->vsync = true;
        p->target_hz = 60;
        p->video_mode = THREEDOH_VIDEO_AUTO;
        p->active_video_standard = THREEDOH_VIDEO_NTSC;
        safe_copy(p->status, sizeof(p->status), "F1 or Esc opens the SDL3 menu.");
        p->status_until_ns = 0;
        set_defaults(p);
    }
    return p;
}

int threedoh_platform_init(threedoh_platform *platform, const char *title,
                           int width, int height, int pixel_bytes)
{
    if (!platform)
        return 0;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL3 init failed: %s\n", SDL_GetError());
        return 0;
    }

    platform->width = width;
    platform->height = height;
    platform->pixel_bytes = pixel_bytes;

    platform->window = SDL_CreateWindow(title ? title : "3DOh", width * 3, height * 3,
                                        SDL_WINDOW_RESIZABLE);
    if (!platform->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }

    platform->renderer = SDL_CreateRenderer(platform->window, NULL);
    if (!platform->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 0;
    }

#if BPP_TYPE == 32
    platform->texture = SDL_CreateTexture(platform->renderer, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STREAMING, width, height);
#else
    platform->texture = SDL_CreateTexture(platform->renderer, SDL_PIXELFORMAT_RGB565,
                                          SDL_TEXTUREACCESS_STREAMING, width, height);
#endif
    if (!platform->texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_SetTextureScaleMode(platform->texture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderDrawBlendMode(platform->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(platform->renderer, 1);
    if (platform->status[0] && !platform->status_until_ns)
        platform->status_until_ns = SDL_GetTicksNS() + UI_STATUS_TIMEOUT_NS;
    load_bindings(platform);
    g_active_platform = platform;
    return 1;
}

void threedoh_platform_destroy(threedoh_platform *platform)
{
    if (!platform)
        return;
    if (g_active_platform == platform)
        g_active_platform = NULL;
    if (platform->texture)
        SDL_DestroyTexture(platform->texture);
    if (platform->renderer)
        SDL_DestroyRenderer(platform->renderer);
    if (platform->window)
        SDL_DestroyWindow(platform->window);
    SDL_Quit();
    free(platform);
}

static void queue_dialog_result(threedoh_platform *p)
{
    if (!p || !p->dialog_ready)
        return;
    p->dialog_ready = 0;
    p->dialog_active = 0;
    if (p->dialog_path[0]) {
        queue_command(p, THREEDOH_PLATFORM_CMD_LOAD_DISC, 0, p->dialog_path);
        set_status(p, "Loading selected disc image.");
    } else {
        set_status(p, "Open-disc dialog cancelled.");
    }
}

static void open_disc_dialog_callback(void *userdata, const char * const *files, int filter)
{
    threedoh_platform *p = (threedoh_platform *)userdata;
    (void)filter;
    if (!p)
        return;
    if (files && files[0])
        safe_copy(p->dialog_path, sizeof(p->dialog_path), files[0]);
    else
        p->dialog_path[0] = 0;
    p->dialog_ready = 1;
}

static void open_disc_dialog(threedoh_platform *p)
{
    static const SDL_DialogFileFilter filters[] = {
        { "3DO disc images", "iso;cue" },
        { "All files", "*" }
    };
    if (!p || p->dialog_active)
        return;
    p->dialog_path[0] = 0;
    p->dialog_ready = 0;
    p->dialog_active = 1;
    set_status(p, "Choose a new ISO/CUE, or drop one onto the window.");
    SDL_ShowOpenFileDialog(open_disc_dialog_callback, p, p->window,
                           filters, (int)(sizeof(filters) / sizeof(filters[0])), NULL, false);
}

static int section_count(void)
{
    return 4;
}

static const char *section_name(int section)
{
    switch (section) {
    case 0: return "Games";
    case 1: return "Save States";
    case 2: return "Controls";
    case 3: return "System";
    default: return "";
    }
}

static int item_count_for_section(int section)
{
    switch (section) {
    case 0: return 5;
    case 1: return UI_SLOTS * 2;
    case 2: return (int)(sizeof(g_buttons) / sizeof(g_buttons[0])) + 2;
    case 3: return 10;
    default: return 0;
    }
}


static const char *video_mode_label(int mode)
{
    switch (mode) {
    case THREEDOH_VIDEO_NTSC: return "NTSC 60 Hz";
    case THREEDOH_VIDEO_PAL: return "PAL 50 Hz";
    default: return "Auto";
    }
}

static const char *active_video_label(int standard)
{
    return standard == THREEDOH_VIDEO_PAL ? "PAL 50 Hz" : "NTSC 60 Hz";
}

static int next_video_mode(int mode)
{
    if (mode == THREEDOH_VIDEO_AUTO)
        return THREEDOH_VIDEO_NTSC;
    if (mode == THREEDOH_VIDEO_NTSC)
        return THREEDOH_VIDEO_PAL;
    return THREEDOH_VIDEO_AUTO;
}

static void item_label(threedoh_platform *p, int section, int item, char *out, size_t out_size)
{
    if (!out || !out_size)
        return;
    out[0] = 0;
    switch (section) {
    case 0:
        switch (item) {
        case 0: snprintf(out, out_size, "%s", p->input_recording ? "Pause disabled while recording" : (p->paused ? "Resume emulation" : "Pause emulation")); break;
        case 1: snprintf(out, out_size, "Load new game..."); break;
        case 2: snprintf(out, out_size, "%s", p->input_recording ? "Soft reset disabled while recording" : "Soft reset current game"); break;
        case 3: snprintf(out, out_size, "Fullscreen: %s", p->fullscreen ? "On" : "Off"); break;
        case 4: snprintf(out, out_size, "Quit"); break;
        }
        break;
    case 1:
        if (item < UI_SLOTS)
            snprintf(out, out_size, "Save state slot %d", item + 1);
        else
            snprintf(out, out_size, "Load state slot %d", item - UI_SLOTS + 1);
        break;
    case 2:
        if (item < (int)(sizeof(g_buttons) / sizeof(g_buttons[0]))) {
            int b = g_buttons[item];
            const char *key = SDL_GetScancodeName(p->key_map[b]);
            if (!key || !*key)
                key = "unmapped";
            if (p->joy_map[b] >= 0)
                snprintf(out, out_size, "%-12s key: %-12s joy button: %d", button_name(b), key, p->joy_map[b]);
            else
                snprintf(out, out_size, "%-12s key: %-12s joy button: -", button_name(b), key);
        } else if (item == (int)(sizeof(g_buttons) / sizeof(g_buttons[0]))) {
            snprintf(out, out_size, "Reset control bindings to defaults");
        } else {
            snprintf(out, out_size, "Save control bindings");
        }
        break;
    case 3:
        switch (item) {
        case 0: snprintf(out, out_size, "Audio: %s", p->muted ? "Muted" : "On"); break;
        case 1: snprintf(out, out_size, "Video standard: %s -> %s", video_mode_label(p->video_mode), active_video_label(p->active_video_standard)); break;
        case 2: snprintf(out, out_size, "Frame limiter: %s (%d Hz)", p->frame_limiter ? "On" : "Off", p->target_hz > 0 ? p->target_hz : 60); break;
        case 3: snprintf(out, out_size, "VSync: %s", p->vsync ? "On" : "Off"); break;
        case 4: snprintf(out, out_size, "Scaling: %s", p->integer_scale ? "Integer / aspect" : "Stretch"); break;
        case 5: snprintf(out, out_size, "Filter: %s", p->linear_filter ? "Linear" : "Nearest"); break;
        case 6: snprintf(out, out_size, "Auto-crop: %s", p->auto_crop ? "On" : "Off"); break;
        case 7: snprintf(out, out_size, "Take screenshot"); break;
        case 8: snprintf(out, out_size, "Shortcuts / help"); break;
        case 9: snprintf(out, out_size, "Quit"); break;
        }
        break;
    }
}

static void clamp_selection(threedoh_platform *p)
{
    int count;
    if (!p)
        return;
    if (p->section < 0)
        p->section = section_count() - 1;
    if (p->section >= section_count())
        p->section = 0;
    count = item_count_for_section(p->section);
    if (count <= 0) {
        p->selected = 0;
        return;
    }
    if (p->selected < 0)
        p->selected = count - 1;
    if (p->selected >= count)
        p->selected = 0;
}

static void activate_item(threedoh_platform *p)
{
    if (!p)
        return;
    switch (p->section) {
    case 0:
        switch (p->selected) {
        case 0:
            if (p->input_recording) {
                set_status(p, "Pause is disabled while input recording is active.");
                break;
            }
            if (p->paused) {
                queue_command(p, THREEDOH_PLATFORM_CMD_RESUME, 0, NULL);
                p->menu_paused_by_ui = false;
                set_status(p, "Emulation resumed.");
            } else {
                queue_command(p, THREEDOH_PLATFORM_CMD_PAUSE, 0, NULL);
                set_status(p, "Emulation paused.");
            }
            break;
        case 1: open_disc_dialog(p); break;
        case 2:
            if (p->input_recording)
                set_status(p, "Soft reset is disabled while input recording is active.");
            else
                queue_command(p, THREEDOH_PLATFORM_CMD_SOFT_RESET, 0, NULL);
            break;
        case 3: queue_command(p, THREEDOH_PLATFORM_CMD_TOGGLE_FULLSCREEN, 0, NULL); break;
        case 4: queue_command(p, THREEDOH_PLATFORM_CMD_QUIT, 0, NULL); break;
        }
        break;
    case 1:
        if (p->selected < UI_SLOTS)
            queue_command(p, THREEDOH_PLATFORM_CMD_SAVE_STATE, p->selected + 1, NULL);
        else
            queue_command(p, THREEDOH_PLATFORM_CMD_LOAD_STATE, p->selected - UI_SLOTS + 1, NULL);
        break;
    case 2:
        if (p->selected < (int)(sizeof(g_buttons) / sizeof(g_buttons[0]))) {
            p->remap_button = g_buttons[p->selected];
            set_statusf(p, "Press a key or joystick button for %s. Esc cancels.", button_name(p->remap_button));
        } else if (p->selected == (int)(sizeof(g_buttons) / sizeof(g_buttons[0]))) {
            set_defaults(p);
            save_bindings(p);
            set_status(p, "Control bindings reset to defaults.");
        } else {
            save_bindings(p);
        }
        break;
    case 3:
        switch (p->selected) {
        case 0:
            p->muted = !p->muted;
            g_audio_muted = p->muted ? 1 : 0;
            set_status(p, p->muted ? "Audio muted." : "Audio enabled.");
            break;
        case 1:
            p->video_mode = next_video_mode(p->video_mode);
            queue_command(p, THREEDOH_PLATFORM_CMD_SET_VIDEO_STANDARD, p->video_mode, NULL);
            set_statusf(p, "Requested video standard: %s.", video_mode_label(p->video_mode));
            break;
        case 2:
            p->frame_limiter = !p->frame_limiter;
            p->limiter_primed = false;
            set_status(p, p->frame_limiter ? "Internal frame limiter enabled." : "Internal frame limiter disabled.");
            break;
        case 3:
            p->vsync = !p->vsync;
            SDL_SetRenderVSync(p->renderer, p->vsync ? 1 : 0);
            set_status(p, p->vsync ? "Renderer VSync enabled." : "Renderer VSync disabled.");
            break;
        case 4:
            p->integer_scale = !p->integer_scale;
            set_status(p, p->integer_scale ? "Integer/aspect scaling enabled." : "Stretch scaling enabled.");
            break;
        case 5:
            p->linear_filter = !p->linear_filter;
            SDL_SetTextureScaleMode(p->texture, p->linear_filter ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
            set_status(p, p->linear_filter ? "Linear texture filtering enabled." : "Nearest texture filtering enabled.");
            break;
        case 6:
            p->auto_crop = !p->auto_crop;
            threedoh_autocrop_reset(&p->crop_state);
            p->window_crop_valid = false;
            set_status(p, p->auto_crop ? "Auto-crop enabled." : "Auto-crop disabled.");
            break;
        case 7:
            queue_command(p, THREEDOH_PLATFORM_CMD_SCREENSHOT, 0, NULL);
            break;
        case 8:
            set_status(p, "F1/Esc menu, F5 save slot 1, F7 load slot 1, F8 reset, F11 fullscreen, F12 screenshot, Ctrl+O load disc.");
            break;
        case 9:
            queue_command(p, THREEDOH_PLATFORM_CMD_QUIT, 0, NULL);
            break;
        }
        break;
    }
}

static void open_menu(threedoh_platform *p)
{
    if (!p || p->menu_open)
        return;
    p->menu_open = true;
    if (p->input_recording) {
        p->menu_paused_by_ui = false;
        release_all_game_buttons(p);
        set_status(p, "Menu opened. Recording mode keeps emulation running.");
    } else if (p->running && !p->paused) {
        p->menu_paused_by_ui = true;
        queue_command(p, THREEDOH_PLATFORM_CMD_PAUSE, 0, NULL);
        set_status(p, "Menu opened. Emulation paused; last frame remains visible behind the menu.");
    } else {
        p->menu_paused_by_ui = false;
        set_status(p, "Menu opened.");
    }
}

static void close_menu(threedoh_platform *p)
{
    if (!p || !p->menu_open)
        return;
    p->menu_open = false;
    if (p->menu_paused_by_ui) {
        queue_command(p, THREEDOH_PLATFORM_CMD_RESUME, 0, NULL);
        p->menu_paused_by_ui = false;
        set_status(p, "Menu closed. Emulation resumed.");
    } else {
        set_status(p, "Menu closed.");
    }
}

static void handle_menu_key(threedoh_platform *p, SDL_Scancode scancode, SDL_Keycode key)
{
    (void)key;
    if (!p)
        return;

    if (p->remap_button >= 0) {
        if (scancode == SDL_SCANCODE_ESCAPE) {
            p->remap_button = -1;
            set_status(p, "Remap cancelled.");
            return;
        }
        p->key_map[p->remap_button] = scancode;
        save_bindings(p);
        set_statusf(p, "%s mapped to key %s.",
                    button_name(p->remap_button), SDL_GetScancodeName(scancode));
        p->remap_button = -1;
        return;
    }

    switch (scancode) {
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_F1:
        close_menu(p);
        break;
    case SDL_SCANCODE_LEFT:
        p->section--;
        p->selected = 0;
        clamp_selection(p);
        break;
    case SDL_SCANCODE_RIGHT:
        p->section++;
        p->selected = 0;
        clamp_selection(p);
        break;
    case SDL_SCANCODE_UP:
        p->selected--;
        clamp_selection(p);
        break;
    case SDL_SCANCODE_DOWN:
        p->selected++;
        clamp_selection(p);
        break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_SPACE:
        activate_item(p);
        break;
    default:
        if ((SDL_GetModState() & SDL_KMOD_CTRL) && scancode == SDL_SCANCODE_O)
            open_disc_dialog(p);
        break;
    }
}

static void set_key(threedoh_platform *p, SDL_Scancode scancode, int pressed)
{
    int button = button_from_scancode(p, scancode);
    if (button >= 0)
        set_button_state(p, button, pressed);
}

static void set_joy_button(threedoh_platform *p, int button_index, int pressed)
{
    int button = button_from_joy_button(p, button_index);
    if (button >= 0)
        set_button_state(p, button, pressed);
}

static void remap_joy_button(threedoh_platform *p, int joy_button)
{
    if (!p || p->remap_button < 0)
        return;
    p->joy_map[p->remap_button] = joy_button;
    save_bindings(p);
    set_statusf(p, "%s mapped to joystick button %d.",
                button_name(p->remap_button), joy_button);
    p->remap_button = -1;
}

static void handle_game_hotkey(threedoh_platform *p, SDL_Scancode scancode, SDL_Keycode key)
{
    if (!p)
        return;
    if (scancode == SDL_SCANCODE_F1 || scancode == SDL_SCANCODE_ESCAPE) {
        open_menu(p);
    } else if (scancode == SDL_SCANCODE_F5) {
        queue_command(p, THREEDOH_PLATFORM_CMD_SAVE_STATE, 1, NULL);
    } else if (scancode == SDL_SCANCODE_F7) {
        queue_command(p, THREEDOH_PLATFORM_CMD_LOAD_STATE, 1, NULL);
    } else if (scancode == SDL_SCANCODE_F8) {
        if (p->input_recording)
            set_status(p, "Soft reset is disabled while input recording is active.");
        else
            queue_command(p, THREEDOH_PLATFORM_CMD_SOFT_RESET, 0, NULL);
    } else if (scancode == SDL_SCANCODE_F11 ||
               ((SDL_GetModState() & SDL_KMOD_ALT) && key == SDLK_RETURN)) {
        queue_command(p, THREEDOH_PLATFORM_CMD_TOGGLE_FULLSCREEN, 0, NULL);
    } else if (scancode == SDL_SCANCODE_F12) {
        queue_command(p, THREEDOH_PLATFORM_CMD_SCREENSHOT, 0, NULL);
    } else if ((SDL_GetModState() & SDL_KMOD_CTRL) && scancode == SDL_SCANCODE_O) {
        open_disc_dialog(p);
    } else {
        set_key(p, scancode, 1);
    }
}

int threedoh_platform_poll(threedoh_platform *platform)
{
    SDL_Event event;
    if (!platform)
        return 0;

    queue_dialog_result(platform);

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            platform->quit = 1;
            isexit = 1;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.repeat)
                break;
            if (platform->menu_open)
                handle_menu_key(platform, event.key.scancode, event.key.key);
            else
                handle_game_hotkey(platform, event.key.scancode, event.key.key);
            break;
        case SDL_EVENT_KEY_UP:
            if (!platform->menu_open)
                set_key(platform, event.key.scancode, 0);
            break;
        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
            if (!platform->menu_open) {
                if (event.jaxis.axis == 0) {
                    set_button_state(platform, THREEDOH_BUTTON_LEFT, event.jaxis.value < -12000);
                    set_button_state(platform, THREEDOH_BUTTON_RIGHT, event.jaxis.value > 12000);
                } else if (event.jaxis.axis == 1) {
                    set_button_state(platform, THREEDOH_BUTTON_UP, event.jaxis.value < -12000);
                    set_button_state(platform, THREEDOH_BUTTON_DOWN, event.jaxis.value > 12000);
                }
            }
            break;
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            if (platform->menu_open && platform->remap_button >= 0)
                remap_joy_button(platform, event.jbutton.button);
            else if (!platform->menu_open)
                set_joy_button(platform, event.jbutton.button, 1);
            break;
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
            if (!platform->menu_open)
                set_joy_button(platform, event.jbutton.button, 0);
            break;
        case SDL_EVENT_DROP_FILE:
            if (event.drop.data) {
                queue_command(platform, THREEDOH_PLATFORM_CMD_LOAD_DISC, 0, event.drop.data);
                set_status(platform, "Loading dropped disc image.");
                /* SDL3 owns drop-event memory. Do not SDL_free(event.drop.data). */
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (platform->menu_open && event.button.button == SDL_BUTTON_LEFT) {
                float x = event.button.x;
                float y = event.button.y;
                int s;
                for (s = 0; s < section_count(); s++) {
                    SDL_FRect tab = { 38.0f + s * 142.0f, 78.0f, 132.0f, 28.0f };
                    if (x >= tab.x && x <= tab.x + tab.w && y >= tab.y && y <= tab.y + tab.h) {
                        platform->section = s;
                        platform->selected = 0;
                        clamp_selection(platform);
                        break;
                    }
                }
                if (y >= 128.0f && y < 128.0f + item_count_for_section(platform->section) * 30.0f) {
                    platform->selected = (int)((y - 128.0f) / 30.0f);
                    clamp_selection(platform);
                    activate_item(platform);
                }
            }
            break;
        default:
            break;
        }
    }
    return 1;
}

static int clamp_window_size_component(int v)
{
    if (v < 64)
        return 64;
    if (v > 16384)
        return 16384;
    return v;
}

static int rounded_positive_scale(float v)
{
    int s;
    if (v < 1.0f)
        return 1;
    s = (int)(v + 0.5f);
    return s < 1 ? 1 : s;
}

static void resize_window_for_crop(threedoh_platform *p, threedoh_crop_rect crop, int full_w, int full_h)
{
    int win_w;
    int win_h;
    int desired_w;
    int desired_h;
    int vertical_crop;
    int horizontal_crop;

    if (!p || !p->window || p->fullscreen)
        return;
    if (crop.w <= 0 || crop.h <= 0 || full_w <= 0 || full_h <= 0)
        return;

    SDL_GetWindowSize(p->window, &win_w, &win_h);
    if (win_w <= 0 || win_h <= 0)
        return;

    vertical_crop = (crop.w >= full_w - 1 && crop.h < full_h);
    horizontal_crop = (crop.h >= full_h - 1 && crop.w < full_w);

    if (p->integer_scale) {
        int scale;
        if (vertical_crop)
            scale = rounded_positive_scale((float)win_w / (float)full_w);
        else if (horizontal_crop)
            scale = rounded_positive_scale((float)win_h / (float)full_h);
        else {
            float sx = (float)win_w / (float)full_w;
            float sy = (float)win_h / (float)full_h;
            if (threedoh_crop_rect_is_full(crop, full_w, full_h))
                scale = rounded_positive_scale(sx < sy ? sx : sy);
            else
                scale = rounded_positive_scale(sx > sy ? sx : sy);
        }
        desired_w = crop.w * scale;
        desired_h = crop.h * scale;
    } else if (vertical_crop) {
        desired_w = win_w;
        desired_h = (int)((float)win_w * (float)crop.h / (float)crop.w + 0.5f);
    } else if (horizontal_crop) {
        desired_h = win_h;
        desired_w = (int)((float)win_h * (float)crop.w / (float)crop.h + 0.5f);
    } else {
        float sx = (float)win_w / (float)full_w;
        float sy = (float)win_h / (float)full_h;
        float scale = threedoh_crop_rect_is_full(crop, full_w, full_h)
            ? (sx < sy ? sx : sy)
            : (sx > sy ? sx : sy);
        if (scale < 1.0f)
            scale = 1.0f;
        desired_w = (int)((float)crop.w * scale + 0.5f);
        desired_h = (int)((float)crop.h * scale + 0.5f);
    }

    desired_w = clamp_window_size_component(desired_w);
    desired_h = clamp_window_size_component(desired_h);

    if (p->window_crop_valid && threedoh_crop_rect_equal(p->window_crop, crop) &&
        win_w == desired_w && win_h == desired_h)
        return;

    if (win_w != desired_w || win_h != desired_h)
        SDL_SetWindowSize(p->window, desired_w, desired_h);
    p->window_crop = crop;
    p->window_crop_valid = true;
}

static void compute_destination(threedoh_platform *p, SDL_FRect *dst, int src_w, int src_h)
{
    int win_w = p->width;
    int win_h = p->height;
    float scale_x;
    float scale_y;
    float scale;
    if (src_w <= 0)
        src_w = p->width;
    if (src_h <= 0)
        src_h = p->height;
    SDL_GetWindowSize(p->window, &win_w, &win_h);
    if (!p->integer_scale) {
        dst->x = 0;
        dst->y = 0;
        dst->w = (float)win_w;
        dst->h = (float)win_h;
        return;
    }
    scale_x = (float)win_w / (float)src_w;
    scale_y = (float)win_h / (float)src_h;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale < 1.0f)
        scale = 1.0f;
    else
        scale = (float)((int)scale);
    dst->w = (float)src_w * scale;
    dst->h = (float)src_h * scale;
    dst->x = ((float)win_w - dst->w) * 0.5f;
    dst->y = ((float)win_h - dst->h) * 0.5f;
}

static void draw_rect(SDL_Renderer *r, float x, float y, float w, float h,
                      unsigned char red, unsigned char green, unsigned char blue,
                      unsigned char alpha, bool fill)
{
    SDL_FRect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_SetRenderDrawColor(r, red, green, blue, alpha);
    if (fill)
        SDL_RenderFillRect(r, &rect);
    else
        SDL_RenderRect(r, &rect);
}

static void draw_text(SDL_Renderer *r, float x, float y, const char *text,
                      unsigned char red, unsigned char green, unsigned char blue)
{
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
    SDL_RenderDebugText(r, x, y, text ? text : "");
}

static void draw_menu(threedoh_platform *p)
{
    SDL_Renderer *r = p->renderer;
    char line[512];
    int i;
    int count;

    draw_rect(r, 0, 0, 2000, 2000, 0, 0, 0, 92, true);
    draw_rect(r, 24, 36, 680, 520, 24, 28, 36, 212, true);
    draw_rect(r, 24, 36, 680, 520, 180, 190, 210, 255, false);

    draw_text(r, 44, 52, "3DOh SDL3 Menu", 255, 255, 255);
    snprintf(line, sizeof(line), "Disc: %s", basename_ptr(p->disc_path));
    draw_text(r, 260, 52, line, 210, 220, 230);

    for (i = 0; i < section_count(); i++) {
        float x = 38.0f + i * 142.0f;
        if (i == p->section)
            draw_rect(r, x, 78, 132, 28, 70, 100, 150, 230, true);
        else
            draw_rect(r, x, 78, 132, 28, 42, 48, 58, 230, true);
        draw_rect(r, x, 78, 132, 28, 120, 130, 145, 255, false);
        draw_text(r, x + 10, 86, section_name(i), 240, 240, 240);
    }

    count = item_count_for_section(p->section);
    for (i = 0; i < count; i++) {
        float y = 128.0f + i * 30.0f;
        item_label(p, p->section, i, line, sizeof(line));
        if (i == p->selected)
            draw_rect(r, 44, y - 5, 620, 24, 82, 100, 130, 230, true);
        draw_text(r, 58, y, line, i == p->selected ? 255 : 220, i == p->selected ? 255 : 225, i == p->selected ? 255 : 230);
    }

    if (p->remap_button >= 0) {
        draw_rect(r, 130, 394, 470, 82, 12, 14, 18, 245, true);
        draw_rect(r, 130, 394, 470, 82, 230, 230, 230, 255, false);
        snprintf(line, sizeof(line), "Remapping %s", button_name(p->remap_button));
        draw_text(r, 154, 414, line, 255, 255, 255);
        draw_text(r, 154, 438, "Press a keyboard key or joystick button. Esc cancels.", 230, 230, 230);
    }

    if (status_is_visible(p))
        draw_text(r, 44, 506, p->status, 210, 220, 230);
    draw_text(r, 44, 532, "F1/Esc close menu  |  Arrows select  |  Enter activates  |  Drop ISO/CUE to load", 180, 190, 205);
}

static void draw_status_bar(threedoh_platform *p)
{
    char line[512];
    if (!status_is_visible(p))
        return;
    draw_rect(p->renderer, 8, 8, 620, 22, 0, 0, 0, 130, true);
    snprintf(line, sizeof(line), "%s%s", p->paused ? "PAUSED - " : "", p->status);
    draw_text(p->renderer, 16, 14, line, 230, 235, 240);
}

void threedoh_platform_present(threedoh_platform *platform, const void *pixels,
                               int width, int height, int pitch)
{
    SDL_FRect dst;
    SDL_FRect src;
    threedoh_crop_rect crop;
    if (!platform || !platform->renderer || !platform->texture || !pixels)
        return;
    if (width <= 0 || width > platform->width)
        width = platform->width;
    if (height <= 0 || height > platform->height)
        height = platform->height;

    crop = threedoh_full_crop_rect(width, height);
    if (platform->auto_crop) {
        threedoh_crop_rect detected = threedoh_detect_crop_rect(pixels, width, height, pitch, platform->pixel_bytes);
        crop = threedoh_autocrop_update(&platform->crop_state, detected);
        resize_window_for_crop(platform, crop, width, height);
    } else {
        platform->window_crop_valid = false;
    }

    src.x = (float)crop.x;
    src.y = (float)crop.y;
    src.w = (float)crop.w;
    src.h = (float)crop.h;

    SDL_UpdateTexture(platform->texture, NULL, pixels, pitch);
    SDL_SetRenderDrawColor(platform->renderer, 0, 0, 0, 255);
    SDL_RenderClear(platform->renderer);

    /* The cropped rectangle is now the active video size.  Windowed mode is
     * resized to that aspect above; fullscreen keeps the display mode and
     * scales the cropped source rect rather than the original 4:3 frame.
     */
    compute_destination(platform, &dst, crop.w, crop.h);
    SDL_RenderTexture(platform->renderer, platform->texture, &src, &dst);
    if (platform->menu_open)
        draw_menu(platform);
    else
        draw_status_bar(platform);
    SDL_RenderPresent(platform->renderer);
}

void threedoh_platform_toggle_fullscreen(threedoh_platform *platform)
{
    if (!platform || !platform->window)
        return;
    platform->fullscreen = !platform->fullscreen;
    platform->window_crop_valid = false;
    SDL_SetWindowFullscreen(platform->window, platform->fullscreen);
}


void threedoh_platform_set_frame_rate(threedoh_platform *platform, int hz)
{
    if (!platform)
        return;
    if (hz != 50 && hz != 60)
        hz = 60;
    platform->target_hz = hz;
    platform->limiter_primed = false;
}

void threedoh_platform_wait_for_next_frame(threedoh_platform *platform)
{
    uint64_t now;
    uint64_t interval;
    if (!platform || !platform->frame_limiter || platform->target_hz <= 0)
        return;

    interval = UINT64_C(1000000000) / (uint64_t)platform->target_hz;
    now = SDL_GetTicksNS();
    if (!platform->limiter_primed) {
        platform->next_frame_ns = now + interval;
        platform->limiter_primed = true;
        return;
    }

    if (now < platform->next_frame_ns) {
        uint64_t remaining = platform->next_frame_ns - now;
        if (remaining > UINT64_C(2000000))
            SDL_DelayNS(remaining - UINT64_C(1000000));
        while (SDL_GetTicksNS() < platform->next_frame_ns) {
            /* short final spin keeps the limiter stable enough for 50/60 Hz
             * without trusting the OS scheduler for sub-millisecond sleep. */
        }
        now = SDL_GetTicksNS();
    }

    platform->next_frame_ns += interval;
    if (platform->next_frame_ns + interval < now)
        platform->next_frame_ns = now + interval;
}

int threedoh_platform_should_quit(const threedoh_platform *platform)
{
    return platform ? platform->quit : 1;
}

int threedoh_platform_take_command(threedoh_platform *platform,
                                   threedoh_platform_command *command)
{
    if (!platform || !command || !platform->pending.flags)
        return 0;
    *command = platform->pending;
    memset(&platform->pending, 0, sizeof(platform->pending));
    return 1;
}

void threedoh_platform_set_status(threedoh_platform *platform, const char *message)
{
    if (!platform)
        return;
    set_status(platform, message);
}

void threedoh_platform_set_runtime_state(threedoh_platform *platform,
                                         int running, int paused,
                                         const char *disc_path)
{
    if (!platform)
        return;
    platform->running = running ? true : false;
    platform->paused = paused ? true : false;
    if (disc_path)
        safe_copy(platform->disc_path, sizeof(platform->disc_path), disc_path);
}


void threedoh_platform_set_video_standard(threedoh_platform *platform,
                                          int mode, int active_standard, int hz)
{
    if (!platform)
        return;
    if (mode != THREEDOH_VIDEO_AUTO && mode != THREEDOH_VIDEO_NTSC && mode != THREEDOH_VIDEO_PAL)
        mode = THREEDOH_VIDEO_AUTO;
    platform->video_mode = mode;
    platform->active_video_standard = active_standard == THREEDOH_VIDEO_PAL ? THREEDOH_VIDEO_PAL : THREEDOH_VIDEO_NTSC;
    threedoh_platform_set_frame_rate(platform, hz);
}

void threedoh_platform_set_input_recording(threedoh_platform *platform, int enabled)
{
    if (!platform)
        return;
    platform->input_recording = enabled ? true : false;
    platform->input_edge_head = 0;
    platform->input_edge_tail = 0;
}

int threedoh_platform_take_input_edge(threedoh_platform *platform,
                                      threedoh_platform_input_edge *edge)
{
    if (!platform || !edge || platform->input_edge_head == platform->input_edge_tail)
        return 0;
    *edge = platform->input_edges[platform->input_edge_head];
    platform->input_edge_head = (platform->input_edge_head + 1) % INPUT_EDGE_QUEUE_LEN;
    return 1;
}

void threedoh_input_button_event(int button, int pressed)
{
    if (button == THREEDOH_BUTTON_EXIT && pressed) {
        isexit = 1;
        return;
    }
    set_button_state(g_active_platform, button, pressed);
}

int inputInit(void)
{
    SDL_JoystickID *ids = NULL;
    int count = 0;
    int i;

    memset(g_input_state, 0, sizeof(g_input_state));
    memset(g_input_packet, 0, sizeof(g_input_packet));
    isexit = 0;

    ids = SDL_GetJoysticks(&count);
    if (ids) {
        for (i = 0; i < count && i < 6; i++)
            SDL_OpenJoystick(ids[i]);
        SDL_free(ids);
    }
    return 1;
}

int inputClose(void) { return 0; }
int inputEnum(void) { int n = 0; SDL_JoystickID *ids = SDL_GetJoysticks(&n); if (ids) SDL_free(ids); return n; }
void *inputOpen(int joyid) { (void)joyid; return NULL; }
void inputPoll(void *joy) { (void)joy; }
int inputFullscreen(void) { return 0; }
int inputLength(void) { return 16; }
unsigned char *inputRead(void)
{
    threedoh_input_build_packet(g_input_state, g_input_packet);
    return g_input_packet;
}

int soundInit(void)
{
    SDL_AudioSpec spec;
    spec.freq = 44100;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    g_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                               &spec, NULL, NULL);
    if (!g_audio_stream) {
        fprintf(stderr, "SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_ResumeAudioStreamDevice(g_audio_stream);
    g_audio_running = 1;
    g_audio_muted = 0;
    return 1;
}

void soundBeginFrame(void)
{
}

void soundFillBuffer(unsigned int dspLoop)
{
    if (!g_audio_running || !g_audio_stream || g_audio_muted)
        return;
    SDL_PutAudioStreamData(g_audio_stream, &dspLoop, 4);
}

void soundRun(void)
{
}

void soundClose(void)
{
    g_audio_running = 0;
    if (g_audio_stream) {
        SDL_DestroyAudioStream(g_audio_stream);
        g_audio_stream = NULL;
    }
}
