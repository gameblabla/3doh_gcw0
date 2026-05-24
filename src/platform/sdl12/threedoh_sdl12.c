#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "platform/threedoh_platform.h"
#include "platform/common/input_packet.h"
#include "platform/common/autocrop.h"
#include "input.h"
#include "sound.h"

#define SOUND_BUFFER_SIZE (1024 * 4 * 4)

struct threedoh_platform {
    SDL_Surface *screen;
    int width;
    int height;
    int pixel_bytes;
    int quit;
    int fullscreen;
    Uint32 screen_flags;
    int bpp;
    threedoh_crop_rect window_crop;
    int window_crop_valid;
    int target_hz;
    Uint32 next_frame_ms;
    int limiter_primed;
    int auto_crop;
    threedoh_autocrop_state crop_state;
    Uint32 status_until_ms;
    int status_active;
    char title[128];
};

static inputState g_input_state[6];
static unsigned char g_input_packet[16];
static SDL_Joystick *g_joysticks[6];
int isexit = 0;

static Uint8 *g_audio_buffer;
static Uint32 g_audio_read_pos;
static Uint32 g_audio_write_pos;
static Uint32 g_audio_bytes;
static int g_audio_running;
static SDL_mutex *g_audio_mutex;
static SDL_cond *g_audio_cv;

static void sdl12_audio_callback(void *udata, Uint8 *stream, int len)
{
    (void)udata;
    memset(stream, 0, (size_t)len);

    if (!g_audio_running || !g_audio_buffer || !g_audio_mutex)
        return;

    SDL_LockMutex(g_audio_mutex);
    if (g_audio_bytes >= (Uint32)len) {
        if (g_audio_read_pos + (Uint32)len <= SOUND_BUFFER_SIZE) {
            memcpy(stream, g_audio_buffer + g_audio_read_pos, (size_t)len);
        } else {
            Uint32 tail = SOUND_BUFFER_SIZE - g_audio_read_pos;
            memcpy(stream, g_audio_buffer + g_audio_read_pos, tail);
            memcpy(stream + tail, g_audio_buffer, (size_t)((Uint32)len - tail));
        }
        g_audio_read_pos = (g_audio_read_pos + (Uint32)len) % SOUND_BUFFER_SIZE;
        g_audio_bytes -= (Uint32)len;
    }
    SDL_CondSignal(g_audio_cv);
    SDL_UnlockMutex(g_audio_mutex);
}

threedoh_platform *threedoh_platform_create(void)
{
    return (threedoh_platform *)calloc(1, sizeof(threedoh_platform));
}

int threedoh_platform_init(threedoh_platform *platform, const char *title,
                           int width, int height, int pixel_bytes)
{
    Uint32 flags = SDL_SWSURFACE;
    int bpp = pixel_bytes == 4 ? 32 : 16;

    if (!platform)
        return 0;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "SDL 1.2 init failed: %s\n", SDL_GetError());
        return 0;
    }

    platform->width = width;
    platform->height = height;
    platform->pixel_bytes = pixel_bytes;
    platform->screen_flags = flags;
    platform->bpp = bpp;
    snprintf(platform->title, sizeof(platform->title), "%s", title ? title : "3DOh");
    platform->target_hz = 60;
    platform->limiter_primed = 0;
    platform->auto_crop = 1;
    SDL_WM_SetCaption(platform->title, NULL);
    SDL_ShowCursor(SDL_DISABLE);
    platform->screen = SDL_SetVideoMode(width, height, bpp, flags);
    if (!platform->screen) {
        fprintf(stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

void threedoh_platform_destroy(threedoh_platform *platform)
{
    if (!platform)
        return;
    if (platform->screen) {
        SDL_FreeSurface(platform->screen);
        platform->screen = NULL;
    }
    SDL_Quit();
    free(platform);
}

static void set_key(int sym, int pressed)
{
    switch (sym) {
    case SDLK_LEFT: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_LEFT, pressed); break;
    case SDLK_RIGHT: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_RIGHT, pressed); break;
    case SDLK_UP: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_UP, pressed); break;
    case SDLK_DOWN: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_DOWN, pressed); break;
    case SDLK_LCTRL:
    case SDLK_RCTRL: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_A, pressed); break;
    case SDLK_LALT:
    case SDLK_RALT: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_B, pressed); break;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_C, pressed); break;
    case SDLK_SPACE: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_X, pressed); break;
    case SDLK_TAB: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_L, pressed); break;
    case SDLK_BACKSPACE: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_R, pressed); break;
    case SDLK_RETURN: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_P, pressed); break;
    case SDLK_ESCAPE:
        if (pressed)
            isexit = 1;
        break;
    default:
        break;
    }
}

static void set_joy_button(int button, int pressed)
{
    switch (button) {
    case 0: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_A, pressed); break;
    case 1: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_B, pressed); break;
    case 2: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_C, pressed); break;
    case 3: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_X, pressed); break;
    case 4: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_L, pressed); break;
    case 5: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_R, pressed); break;
    case 6:
    case 7: threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_P, pressed); break;
    default: break;
    }
}

int threedoh_platform_poll(threedoh_platform *platform)
{
    SDL_Event event;
    if (!platform)
        return 0;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            platform->quit = 1;
            isexit = 1;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_F11 ||
                ((event.key.keysym.mod & KMOD_ALT) && event.key.keysym.sym == SDLK_RETURN))
                threedoh_platform_toggle_fullscreen(platform);
            else
                set_key(event.key.keysym.sym, 1);
            break;
        case SDL_KEYUP:
            set_key(event.key.keysym.sym, 0);
            break;
        case SDL_JOYAXISMOTION:
            if (event.jaxis.axis == 0) {
                threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_LEFT, event.jaxis.value < -12000);
                threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_RIGHT, event.jaxis.value > 12000);
            } else if (event.jaxis.axis == 1) {
                threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_UP, event.jaxis.value < -12000);
                threedoh_input_set_button(g_input_state, 0, THREEDOH_BUTTON_DOWN, event.jaxis.value > 12000);
            }
            break;
        case SDL_JOYBUTTONDOWN:
            set_joy_button(event.jbutton.button, 1);
            break;
        case SDL_JOYBUTTONUP:
            set_joy_button(event.jbutton.button, 0);
            break;
        default:
            break;
        }
    }
    return 1;
}

static int clamp_window_size_component_sdl12(int v)
{
    if (v < 64)
        return 64;
    if (v > 16384)
        return 16384;
    return v;
}

static int rounded_positive_scale_sdl12(float v)
{
    int s;
    if (v < 1.0f)
        return 1;
    s = (int)(v + 0.5f);
    return s < 1 ? 1 : s;
}

static void resize_window_for_crop_sdl12(threedoh_platform *platform, threedoh_crop_rect crop,
                                         int full_w, int full_h)
{
    int win_w;
    int win_h;
    int desired_w;
    int desired_h;
    int vertical_crop;
    int horizontal_crop;

    if (!platform || !platform->screen || platform->fullscreen)
        return;
    if (crop.w <= 0 || crop.h <= 0 || full_w <= 0 || full_h <= 0)
        return;

    win_w = platform->screen->w;
    win_h = platform->screen->h;
    if (win_w <= 0 || win_h <= 0)
        return;

    vertical_crop = (crop.w >= full_w - 1 && crop.h < full_h);
    horizontal_crop = (crop.h >= full_h - 1 && crop.w < full_w);

    if (vertical_crop) {
        int scale = rounded_positive_scale_sdl12((float)win_w / (float)full_w);
        desired_w = crop.w * scale;
        desired_h = crop.h * scale;
    } else if (horizontal_crop) {
        int scale = rounded_positive_scale_sdl12((float)win_h / (float)full_h);
        desired_w = crop.w * scale;
        desired_h = crop.h * scale;
    } else {
        float sx = (float)win_w / (float)full_w;
        float sy = (float)win_h / (float)full_h;
        int scale;
        if (threedoh_crop_rect_is_full(crop, full_w, full_h))
            scale = rounded_positive_scale_sdl12(sx < sy ? sx : sy);
        else
            scale = rounded_positive_scale_sdl12(sx > sy ? sx : sy);
        desired_w = crop.w * scale;
        desired_h = crop.h * scale;
    }

    desired_w = clamp_window_size_component_sdl12(desired_w);
    desired_h = clamp_window_size_component_sdl12(desired_h);

    if (platform->window_crop_valid && threedoh_crop_rect_equal(platform->window_crop, crop) &&
        win_w == desired_w && win_h == desired_h)
        return;

    if (win_w != desired_w || win_h != desired_h) {
        SDL_Surface *screen = SDL_SetVideoMode(desired_w, desired_h, platform->bpp, platform->screen_flags);
        if (screen)
            platform->screen = screen;
    }
    platform->window_crop = crop;
    platform->window_crop_valid = 1;
}

static void compute_destination_sdl12(threedoh_platform *platform, int src_w, int src_h,
                                      int *dst_x, int *dst_y, int *dst_w, int *dst_h)
{
    int screen_w = platform->screen ? platform->screen->w : src_w;
    int screen_h = platform->screen ? platform->screen->h : src_h;
    int w_by_h;
    int h_by_w;

    if (src_w <= 0) src_w = screen_w;
    if (src_h <= 0) src_h = screen_h;

    w_by_h = (screen_h * src_w) / src_h;
    if (w_by_h <= screen_w) {
        *dst_w = w_by_h > 0 ? w_by_h : 1;
        *dst_h = screen_h;
    } else {
        h_by_w = (screen_w * src_h) / src_w;
        *dst_w = screen_w;
        *dst_h = h_by_w > 0 ? h_by_w : 1;
    }
    *dst_x = (screen_w - *dst_w) / 2;
    *dst_y = (screen_h - *dst_h) / 2;
}

static void scale_crop_to_screen(threedoh_platform *platform, const void *pixels, int pitch,
                                 threedoh_crop_rect crop, int dst_x, int dst_y, int dst_w, int dst_h)
{
    int y;
    const Uint8 *base = (const Uint8 *)pixels;
    Uint8 *screen_pixels = (Uint8 *)platform->screen->pixels;
    int pixel_bytes = platform->pixel_bytes;

    if (dst_w <= 0 || dst_h <= 0 || crop.w <= 0 || crop.h <= 0)
        return;

    for (y = 0; y < dst_h; y++) {
        int x;
        int src_y = crop.y + (y * crop.h) / dst_h;
        const Uint8 *src_row = base + (size_t)src_y * (size_t)pitch;
        Uint8 *dst_row = screen_pixels + (size_t)(dst_y + y) * (size_t)platform->screen->pitch +
                         (size_t)dst_x * (size_t)pixel_bytes;
        for (x = 0; x < dst_w; x++) {
            int src_x = crop.x + (x * crop.w) / dst_w;
            const Uint8 *src = src_row + (size_t)src_x * (size_t)pixel_bytes;
            Uint8 *dst = dst_row + (size_t)x * (size_t)pixel_bytes;
            if (pixel_bytes == 4) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = src[3];
            } else {
                dst[0] = src[0];
                dst[1] = src[1];
            }
        }
    }
}

void threedoh_platform_present(threedoh_platform *platform, const void *pixels,
                               int width, int height, int pitch)
{
    int dst_x, dst_y, dst_w, dst_h;
    threedoh_crop_rect crop;

    if (!platform || !platform->screen || !pixels)
        return;
    if (width <= 0 || height <= 0)
        return;

    crop = threedoh_full_crop_rect(width, height);
    if (platform->auto_crop) {
        threedoh_crop_rect detected = threedoh_detect_crop_rect(pixels, width, height, pitch, platform->pixel_bytes);
        crop = threedoh_autocrop_update(&platform->crop_state, detected);
    }

    if (platform->auto_crop)
        resize_window_for_crop_sdl12(platform, crop, width, height);
    else
        platform->window_crop_valid = 0;

    compute_destination_sdl12(platform, crop.w, crop.h, &dst_x, &dst_y, &dst_w, &dst_h);

    if (SDL_MUSTLOCK(platform->screen))
        SDL_LockSurface(platform->screen);

    memset(platform->screen->pixels, 0, (size_t)platform->screen->pitch * (size_t)platform->screen->h);
    scale_crop_to_screen(platform, pixels, pitch, crop, dst_x, dst_y, dst_w, dst_h);

    if (SDL_MUSTLOCK(platform->screen))
        SDL_UnlockSurface(platform->screen);
    SDL_Flip(platform->screen);

    if (platform->status_active && SDL_GetTicks() >= platform->status_until_ms) {
        platform->status_active = 0;
        SDL_WM_SetCaption(platform->title, NULL);
    }
}

void threedoh_platform_toggle_fullscreen(threedoh_platform *platform)
{
    if (!platform || !platform->screen)
        return;
    SDL_WM_ToggleFullScreen(platform->screen);
    platform->fullscreen = !platform->fullscreen;
    platform->window_crop_valid = 0;
}


void threedoh_platform_set_frame_rate(threedoh_platform *platform, int hz)
{
    if (!platform)
        return;
    platform->target_hz = (hz == 50 || hz == 60) ? hz : 60;
    platform->limiter_primed = 0;
}

void threedoh_platform_wait_for_next_frame(threedoh_platform *platform)
{
    Uint32 now;
    Uint32 interval;
    if (!platform || platform->target_hz <= 0)
        return;
    interval = (Uint32)(1000U / (Uint32)platform->target_hz);
    if (interval < 1)
        interval = 1;
    now = SDL_GetTicks();
    if (!platform->limiter_primed) {
        platform->next_frame_ms = now + interval;
        platform->limiter_primed = 1;
        return;
    }
    if ((int32_t)(platform->next_frame_ms - now) > 0)
        SDL_Delay(platform->next_frame_ms - now);
    now = SDL_GetTicks();
    platform->next_frame_ms += interval;
    if ((int32_t)(now - platform->next_frame_ms) > (int32_t)interval)
        platform->next_frame_ms = now + interval;
}

int threedoh_platform_should_quit(const threedoh_platform *platform)
{
    return platform ? platform->quit : 1;
}

void threedoh_input_button_event(int button, int pressed)
{
    if (button == THREEDOH_BUTTON_EXIT && pressed) {
        isexit = 1;
        return;
    }
    threedoh_input_set_button(g_input_state, 0, button, pressed);
}

int inputInit(void)
{
    int i;
    memset(g_input_state, 0, sizeof(g_input_state));
    memset(g_input_packet, 0, sizeof(g_input_packet));
    isexit = 0;
    SDL_JoystickEventState(SDL_ENABLE);
    for (i = 0; i < SDL_NumJoysticks() && i < 6; i++)
        g_joysticks[i] = SDL_JoystickOpen(i);
    return 1;
}

int inputClose(void)
{
    int i;
    for (i = 0; i < 6; i++) {
        if (g_joysticks[i]) {
            SDL_JoystickClose(g_joysticks[i]);
            g_joysticks[i] = NULL;
        }
    }
    return 0;
}

int inputEnum(void) { return SDL_NumJoysticks(); }
void *inputOpen(int joyid) { return SDL_JoystickOpen(joyid); }
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
    SDL_AudioSpec wanted;
    SDL_AudioSpec obtained;

    memset(&wanted, 0, sizeof(wanted));
    wanted.freq = 44100;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 1024;
    wanted.callback = sdl12_audio_callback;

    if (SDL_OpenAudio(&wanted, &obtained) < 0) {
        fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
        return 0;
    }

    g_audio_buffer = (Uint8 *)calloc(SOUND_BUFFER_SIZE, 1);
    g_audio_mutex = SDL_CreateMutex();
    g_audio_cv = SDL_CreateCond();
    g_audio_read_pos = g_audio_write_pos = g_audio_bytes = 0;
    g_audio_running = 1;
    SDL_PauseAudio(0);
    return g_audio_buffer && g_audio_mutex && g_audio_cv;
}

void soundBeginFrame(void)
{
}

void soundFillBuffer(unsigned int dspLoop)
{
    if (!g_audio_running || !g_audio_buffer || !g_audio_mutex)
        return;

    SDL_LockMutex(g_audio_mutex);
    if (g_audio_bytes <= SOUND_BUFFER_SIZE - 4) {
        memcpy(g_audio_buffer + g_audio_write_pos, &dspLoop, 4);
        g_audio_write_pos = (g_audio_write_pos + 4) % SOUND_BUFFER_SIZE;
        g_audio_bytes += 4;
    }
    SDL_CondSignal(g_audio_cv);
    SDL_UnlockMutex(g_audio_mutex);
}

void soundRun(void)
{
}

void soundClose(void)
{
    g_audio_running = 0;
    if (g_audio_mutex && g_audio_cv) {
        SDL_LockMutex(g_audio_mutex);
        SDL_CondSignal(g_audio_cv);
        SDL_UnlockMutex(g_audio_mutex);
    }
    SDL_CloseAudio();
    if (g_audio_cv) SDL_DestroyCond(g_audio_cv);
    if (g_audio_mutex) SDL_DestroyMutex(g_audio_mutex);
    free(g_audio_buffer);
    g_audio_buffer = NULL;
    g_audio_cv = NULL;
    g_audio_mutex = NULL;
    g_audio_bytes = 0;
}

int threedoh_platform_take_command(threedoh_platform *platform,
                                   threedoh_platform_command *command)
{
    (void)platform;
    if (command)
        memset(command, 0, sizeof(*command));
    return 0;
}

void threedoh_platform_set_status(threedoh_platform *platform, const char *message)
{
    if (!platform)
        return;
    if (!message || !*message) {
        platform->status_active = 0;
        SDL_WM_SetCaption(platform->title[0] ? platform->title : "3DOh", NULL);
        return;
    }
    SDL_WM_SetCaption(message, NULL);
    platform->status_until_ms = SDL_GetTicks() + 1500U;
    platform->status_active = 1;
}

void threedoh_platform_set_runtime_state(threedoh_platform *platform,
                                         int running, int paused,
                                         const char *disc_path)
{
    (void)platform;
    (void)running;
    (void)paused;
    (void)disc_path;
}


void threedoh_platform_set_video_standard(threedoh_platform *platform,
                                          int mode, int active_standard, int hz)
{
    (void)mode;
    (void)active_standard;
    threedoh_platform_set_frame_rate(platform, hz);
}
