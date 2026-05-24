#ifndef THREEDOH_INPUT_SCRIPT_H
#define THREEDOH_INPUT_SCRIPT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct threedoh_input_script threedoh_input_script;

void threedoh_input_script_init(threedoh_input_script *script);
int threedoh_input_script_parse_inline(threedoh_input_script *script, const char *text, const char **error_out);
int threedoh_input_script_parse_file(threedoh_input_script *script, const char *path, const char **error_out);
void threedoh_input_script_set_hold_ms(threedoh_input_script *script, int hold_ms);
void threedoh_input_script_compile(threedoh_input_script *script, int frame_rate_hz);
void threedoh_input_script_apply(threedoh_input_script *script, unsigned long frame_index);
int threedoh_input_script_count(const threedoh_input_script *script);
int threedoh_input_script_is_enabled(const threedoh_input_script *script);

struct threedoh_input_script {
    struct {
        double seconds;
        long frame;
        int uses_frame;
        int button;
        int pressed;
        unsigned int order;
    } raw[2048];
    struct {
        long frame;
        int button;
        int pressed;
        unsigned int order;
    } event[2048];
    int raw_count;
    int event_count;
    int cursor;
    int hold_ms;
    int compiled_hz;
    unsigned int next_order;
    char error[256];
};

#ifdef __cplusplus
}
#endif

#endif
