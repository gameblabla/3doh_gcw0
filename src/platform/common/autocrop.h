#ifndef THREEDOH_PLATFORM_COMMON_AUTOCROP_H
#define THREEDOH_PLATFORM_COMMON_AUTOCROP_H

#include <stdint.h>
#include <string.h>

typedef struct threedoh_crop_rect {
    int x;
    int y;
    int w;
    int h;
} threedoh_crop_rect;

typedef struct threedoh_autocrop_state {
    threedoh_crop_rect active;
    threedoh_crop_rect pending;
    int pending_frames;
    int initialized;
} threedoh_autocrop_state;

static int threedoh_crop_rect_equal(threedoh_crop_rect a, threedoh_crop_rect b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static threedoh_crop_rect threedoh_full_crop_rect(int width, int height)
{
    threedoh_crop_rect r;
    r.x = 0;
    r.y = 0;
    r.w = width > 0 ? width : 1;
    r.h = height > 0 ? height : 1;
    return r;
}

static int threedoh_crop_rect_is_full(threedoh_crop_rect r, int width, int height)
{
    return r.x <= 0 && r.y <= 0 && r.w >= width && r.h >= height;
}

static int threedoh_pixel_is_black(const uint8_t *p, int pixel_bytes)
{
    if (!p)
        return 1;
#if BPP_TYPE == 32
    (void)pixel_bytes;
    return p[0] <= 12 && p[1] <= 12 && p[2] <= 12;
#else
    (void)pixel_bytes;
    return (((uint16_t)p[0]) | ((uint16_t)p[1] << 8)) == 0;
#endif
}

static threedoh_crop_rect threedoh_detect_crop_rect(const void *pixels,
                                                    int width, int height,
                                                    int pitch, int pixel_bytes)
{
    const uint8_t *base = (const uint8_t *)pixels;
    int min_x, min_y, max_x, max_y;
    int x, y;
    threedoh_crop_rect full = threedoh_full_crop_rect(width, height);

    if (!pixels || width <= 0 || height <= 0 || pitch <= 0 || pixel_bytes <= 0)
        return full;

    min_x = width;
    min_y = height;
    max_x = -1;
    max_y = -1;

    for (y = 0; y < height; y++) {
        const uint8_t *row = base + (size_t)y * (size_t)pitch;
        for (x = 0; x < width; x++) {
            const uint8_t *p = row + (size_t)x * (size_t)pixel_bytes;
            if (!threedoh_pixel_is_black(p, pixel_bytes)) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }

    if (max_x < min_x || max_y < min_y)
        return full;

    /* Aggressive mode: no guard band.  The user-facing contract is that a
     * solid/near-black border is fully removed, including 1- or 2-pixel
     * borders that earlier builds intentionally preserved.
     */

    /* Only reject very small islands of content.  Anything that occupies a
     * plausible video area should be cropped exactly, including strongly
     * letterboxed/pillarboxed title/menu screens.
     */
    if ((max_x - min_x + 1) < (width / 4) ||
        (max_y - min_y + 1) < (height / 4))
        return full;

    {
        threedoh_crop_rect r;
        r.x = min_x;
        r.y = min_y;
        r.w = max_x - min_x + 1;
        r.h = max_y - min_y + 1;
        return r;
    }
}

static threedoh_crop_rect threedoh_autocrop_update(threedoh_autocrop_state *state,
                                                   threedoh_crop_rect detected)
{
    if (!state)
        return detected;
    if (!state->initialized) {
        memset(state, 0, sizeof(*state));
        state->active = detected;
        state->pending = detected;
        state->initialized = 1;
        return state->active;
    }
    if (threedoh_crop_rect_equal(detected, state->active)) {
        state->pending = detected;
        state->pending_frames = 0;
        return state->active;
    }
    if (!threedoh_crop_rect_equal(detected, state->pending)) {
        state->pending = detected;
        state->pending_frames = 1;
    } else if (state->pending_frames < 255) {
        state->pending_frames++;
    }
    if (state->pending_frames >= 6) {
        state->active = state->pending;
        state->pending_frames = 0;
    }
    return state->active;
}

static void threedoh_autocrop_reset(threedoh_autocrop_state *state)
{
    if (state)
        memset(state, 0, sizeof(*state));
}

#endif
