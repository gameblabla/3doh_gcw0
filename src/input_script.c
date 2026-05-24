#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "input_script.h"
#include "input.h"

static void set_error(threedoh_input_script *s, const char *msg, const char *arg)
{
    if (!s)
        return;
    if (!arg)
        arg = "";
    snprintf(s->error, sizeof(s->error), msg, arg);
}

static char *trim(char *p)
{
    char *end;
    if (!p)
        return p;
    while (*p && isspace((unsigned char)*p))
        p++;
    end = p + strlen(p);
    while (end > p && isspace((unsigned char)end[-1]))
        *--end = 0;
    return p;
}

static void lower_copy(char *dst, size_t dst_size, const char *src)
{
    size_t i;
    if (!dst || !dst_size)
        return;
    if (!src)
        src = "";
    for (i = 0; src[i] && i + 1 < dst_size; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = 0;
}

static int button_from_name(const char *name)
{
    char b[32];
    lower_copy(b, sizeof(b), name);
    if (!strcmp(b, "up") || !strcmp(b, "dpad_up") || !strcmp(b, "dpad-up")) return THREEDOH_BUTTON_UP;
    if (!strcmp(b, "down") || !strcmp(b, "dpad_down") || !strcmp(b, "dpad-down")) return THREEDOH_BUTTON_DOWN;
    if (!strcmp(b, "left") || !strcmp(b, "dpad_left") || !strcmp(b, "dpad-left")) return THREEDOH_BUTTON_LEFT;
    if (!strcmp(b, "right") || !strcmp(b, "dpad_right") || !strcmp(b, "dpad-right")) return THREEDOH_BUTTON_RIGHT;
    if (!strcmp(b, "a")) return THREEDOH_BUTTON_A;
    if (!strcmp(b, "b")) return THREEDOH_BUTTON_B;
    if (!strcmp(b, "c")) return THREEDOH_BUTTON_C;
    if (!strcmp(b, "x")) return THREEDOH_BUTTON_X;
    if (!strcmp(b, "l")) return THREEDOH_BUTTON_L;
    if (!strcmp(b, "r")) return THREEDOH_BUTTON_R;
    if (!strcmp(b, "p") || !strcmp(b, "pause") || !strcmp(b, "start") || !strcmp(b, "select")) return THREEDOH_BUTTON_P;
    if (!strcmp(b, "exit") || !strcmp(b, "quit")) return THREEDOH_BUTTON_EXIT;
    return -1;
}

static int action_from_name(const char *action, int *pressed, int *auto_release)
{
    char a[32];
    if (!pressed || !auto_release)
        return 0;
    *pressed = 1;
    *auto_release = 1;
    if (!action || !*action)
        return 1;
    lower_copy(a, sizeof(a), action);
    if (!strcmp(a, "press") || !strcmp(a, "tap")) {
        *pressed = 1;
        *auto_release = 1;
        return 1;
    }
    if (!strcmp(a, "down") || !strcmp(a, "on") || !strcmp(a, "1")) {
        *pressed = 1;
        *auto_release = 0;
        return 1;
    }
    if (!strcmp(a, "up") || !strcmp(a, "off") || !strcmp(a, "0") || !strcmp(a, "release")) {
        *pressed = 0;
        *auto_release = 0;
        return 1;
    }
    return 0;
}

static int append_raw(threedoh_input_script *s, double seconds, long frame, int uses_frame,
                      int button, int pressed)
{
    if (!s || s->raw_count >= (int)(sizeof(s->raw) / sizeof(s->raw[0]))) {
        set_error(s, "too many input-script events near '%s'", "");
        return 0;
    }
    s->raw[s->raw_count].seconds = seconds;
    s->raw[s->raw_count].frame = frame;
    s->raw[s->raw_count].uses_frame = uses_frame;
    s->raw[s->raw_count].button = button;
    s->raw[s->raw_count].pressed = pressed;
    s->raw[s->raw_count].order = s->next_order++;
    s->raw_count++;
    return 1;
}

static int parse_time_token(const char *token, double *seconds, long *frame, int *uses_frame)
{
    char *end = NULL;
    char tmp[64];
    size_t len;
    if (!token || !*token || !seconds || !frame || !uses_frame)
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", token);
    len = strlen(tmp);
    *uses_frame = 0;
    *seconds = 0.0;
    *frame = 0;
    if (len > 1 && (tmp[len - 1] == 'f' || tmp[len - 1] == 'F')) {
        tmp[len - 1] = 0;
        errno = 0;
        *frame = strtol(tmp, &end, 10);
        if (errno || !end || *end || *frame < 0)
            return 0;
        *uses_frame = 1;
        return 1;
    }
    errno = 0;
    *seconds = strtod(tmp, &end);
    if (errno || !end || *end || *seconds < 0.0)
        return 0;
    return 1;
}

static char *read_token(char **pp)
{
    char *p;
    char *start;
    if (!pp || !*pp)
        return NULL;
    p = trim(*pp);
    if (!*p) {
        *pp = p;
        return NULL;
    }
    start = p;
    while (*p && *p != ':' && *p != '=' && *p != ',' && *p != ';' && !isspace((unsigned char)*p))
        p++;
    if (*p) {
        *p++ = 0;
        while (*p == ':' || *p == '=' || isspace((unsigned char)*p))
            p++;
    }
    *pp = p;
    return start;
}

static int parse_one(threedoh_input_script *s, char *segment)
{
    char *p;
    char *time_tok;
    char *button_tok;
    char *action_tok;
    double seconds = 0.0;
    long frame = 0;
    int uses_frame = 0;
    int button;
    int pressed;
    int auto_release;
    char sign = 0;

    if (!segment)
        return 1;
    p = trim(segment);
    if (!*p || *p == '#')
        return 1;

    time_tok = read_token(&p);
    button_tok = read_token(&p);
    action_tok = read_token(&p);

    if (!time_tok || !button_tok) {
        set_error(s, "bad input-script event '%s'", segment);
        return 0;
    }
    if (!parse_time_token(time_tok, &seconds, &frame, &uses_frame)) {
        set_error(s, "bad input-script time '%s'", time_tok);
        return 0;
    }
    if (button_tok[0] == '+' || button_tok[0] == '-') {
        sign = button_tok[0];
        button_tok++;
    }
    button = button_from_name(button_tok);
    if (button < 0) {
        set_error(s, "unknown input-script button '%s'", button_tok);
        return 0;
    }
    if (sign == '+') {
        pressed = 1;
        auto_release = 0;
    } else if (sign == '-') {
        pressed = 0;
        auto_release = 0;
    } else if (!action_from_name(action_tok, &pressed, &auto_release)) {
        set_error(s, "bad input-script action '%s'", action_tok ? action_tok : "");
        return 0;
    }

    if (!append_raw(s, seconds, frame, uses_frame, button, pressed))
        return 0;
    if (auto_release) {
        if (uses_frame) {
            long hold = s->hold_ms > 0 ? (s->hold_ms + 16) / 17 : 6;
            if (hold < 1)
                hold = 1;
            if (!append_raw(s, seconds, frame + hold, uses_frame, button, 0))
                return 0;
        } else {
            double hold = (s->hold_ms > 0 ? s->hold_ms : 100) / 1000.0;
            if (!append_raw(s, seconds + hold, frame, uses_frame, button, 0))
                return 0;
        }
    }
    return 1;
}

static int parse_text(threedoh_input_script *s, const char *text, const char **error_out)
{
    char *copy;
    char *seg;
    char *next;
    if (error_out)
        *error_out = NULL;
    if (!s || !text)
        return 0;
    copy = (char *)malloc(strlen(text) + 1);
    if (!copy) {
        set_error(s, "out of memory while parsing input script%s", "");
        if (error_out) *error_out = s->error;
        return 0;
    }
    strcpy(copy, text);
    for (seg = copy; seg && *seg; seg = next) {
        next = seg;
        while (*next && *next != ',' && *next != ';' && *next != '\n' && *next != '\r')
            next++;
        if (*next)
            *next++ = 0;
        if (!parse_one(s, seg)) {
            free(copy);
            if (error_out) *error_out = s->error;
            return 0;
        }
    }
    free(copy);
    return 1;
}

void threedoh_input_script_init(threedoh_input_script *script)
{
    if (!script)
        return;
    memset(script, 0, sizeof(*script));
    script->hold_ms = 100;
}

void threedoh_input_script_set_hold_ms(threedoh_input_script *script, int hold_ms)
{
    if (!script)
        return;
    if (hold_ms < 1)
        hold_ms = 1;
    if (hold_ms > 5000)
        hold_ms = 5000;
    script->hold_ms = hold_ms;
}

int threedoh_input_script_parse_inline(threedoh_input_script *script, const char *text, const char **error_out)
{
    return parse_text(script, text, error_out);
}

int threedoh_input_script_parse_file(threedoh_input_script *script, const char *path, const char **error_out)
{
    FILE *fp;
    long size;
    char *data;
    int ok;
    if (error_out)
        *error_out = NULL;
    if (!script || !path || !*path)
        return 0;
    fp = fopen(path, "rb");
    if (!fp) {
        set_error(script, "could not open input-script file '%s'", path);
        if (error_out) *error_out = script->error;
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);
    if (size < 0 || size > 1024 * 1024) {
        fclose(fp);
        set_error(script, "input-script file is too large: '%s'", path);
        if (error_out) *error_out = script->error;
        return 0;
    }
    data = (char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(fp);
        set_error(script, "out of memory reading input-script file '%s'", path);
        if (error_out) *error_out = script->error;
        return 0;
    }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        fclose(fp);
        free(data);
        set_error(script, "could not read input-script file '%s'", path);
        if (error_out) *error_out = script->error;
        return 0;
    }
    fclose(fp);
    data[size] = 0;
    ok = parse_text(script, data, error_out);
    free(data);
    return ok;
}

static int cmp_event(const void *a, const void *b)
{
    const typeof(((threedoh_input_script *)0)->event[0]) *ea = a;
    const typeof(((threedoh_input_script *)0)->event[0]) *eb = b;
    if (ea->frame < eb->frame) return -1;
    if (ea->frame > eb->frame) return 1;
    if (ea->order < eb->order) return -1;
    if (ea->order > eb->order) return 1;
    return 0;
}

void threedoh_input_script_compile(threedoh_input_script *script, int frame_rate_hz)
{
    int i;
    if (!script)
        return;
    if (frame_rate_hz <= 0)
        frame_rate_hz = 60;
    script->event_count = 0;
    script->cursor = 0;
    script->compiled_hz = frame_rate_hz;
    for (i = 0; i < script->raw_count; i++) {
        long frame = script->raw[i].uses_frame ? script->raw[i].frame : (long)(script->raw[i].seconds * (double)frame_rate_hz + 0.5);
        if (frame < 0)
            frame = 0;
        script->event[script->event_count].frame = frame;
        script->event[script->event_count].button = script->raw[i].button;
        script->event[script->event_count].pressed = script->raw[i].pressed;
        script->event[script->event_count].order = script->raw[i].order;
        script->event_count++;
    }
    qsort(script->event, (size_t)script->event_count, sizeof(script->event[0]), cmp_event);
}

void threedoh_input_script_apply(threedoh_input_script *script, unsigned long frame_index)
{
    if (!script || script->event_count <= 0)
        return;
    while (script->cursor < script->event_count && script->event[script->cursor].frame <= (long)frame_index) {
        threedoh_input_button_event(script->event[script->cursor].button, script->event[script->cursor].pressed);
        script->cursor++;
    }
}

int threedoh_input_script_count(const threedoh_input_script *script)
{
    return script ? script->event_count : 0;
}

int threedoh_input_script_is_enabled(const threedoh_input_script *script)
{
    return script && script->event_count > 0;
}
