#ifndef THREEDOH_PLATFORM_H
#define THREEDOH_PLATFORM_H

#include <stdint.h>
#include "core/threedoh_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct threedoh_platform threedoh_platform;

enum threedoh_platform_command_bits {
    THREEDOH_PLATFORM_CMD_NONE = 0,
    THREEDOH_PLATFORM_CMD_QUIT = 1 << 0,
    THREEDOH_PLATFORM_CMD_SOFT_RESET = 1 << 1,
    THREEDOH_PLATFORM_CMD_SAVE_STATE = 1 << 2,
    THREEDOH_PLATFORM_CMD_LOAD_STATE = 1 << 3,
    THREEDOH_PLATFORM_CMD_LOAD_DISC = 1 << 4,
    THREEDOH_PLATFORM_CMD_TOGGLE_PAUSE = 1 << 5,
    THREEDOH_PLATFORM_CMD_TOGGLE_FULLSCREEN = 1 << 6,
    THREEDOH_PLATFORM_CMD_PAUSE = 1 << 7,
    THREEDOH_PLATFORM_CMD_RESUME = 1 << 8,
    THREEDOH_PLATFORM_CMD_SET_VIDEO_STANDARD = 1 << 9
};

typedef struct threedoh_platform_command {
    int flags;
    int slot;
    char path[1024];
} threedoh_platform_command;

threedoh_platform *threedoh_platform_create(void);
int threedoh_platform_init(threedoh_platform *platform, const char *title,
                           int width, int height, int pixel_bytes);
void threedoh_platform_destroy(threedoh_platform *platform);
int threedoh_platform_poll(threedoh_platform *platform);
void threedoh_platform_present(threedoh_platform *platform, const void *pixels,
                               int width, int height, int pitch);
void threedoh_platform_toggle_fullscreen(threedoh_platform *platform);
void threedoh_platform_set_frame_rate(threedoh_platform *platform, int hz);
void threedoh_platform_wait_for_next_frame(threedoh_platform *platform);
int threedoh_platform_should_quit(const threedoh_platform *platform);

/* Optional host/UI hooks.  Backends that do not implement an in-window UI can
 * keep these as no-ops while preserving the opaque platform boundary. */
int threedoh_platform_take_command(threedoh_platform *platform,
                                   threedoh_platform_command *command);
void threedoh_platform_set_status(threedoh_platform *platform, const char *message);
void threedoh_platform_set_runtime_state(threedoh_platform *platform,
                                         int running, int paused,
                                         const char *disc_path);
void threedoh_platform_set_video_standard(threedoh_platform *platform,
                                          int mode, int active_standard, int hz);

#ifdef __cplusplus
}
#endif

#endif
