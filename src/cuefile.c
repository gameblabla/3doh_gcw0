#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cuefile.h"

#define STRING_MAX 2048

static void safe_copy(char *dst, const char *src, size_t dst_size)
{
    if (!dst || !dst_size)
        return;
    if (!src)
        src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static int has_suffix_ci(const char *s, const char *suffix)
{
    size_t slen, tlen, i;
    if (!s || !suffix)
        return 0;
    slen = strlen(s);
    tlen = strlen(suffix);
    if (tlen > slen)
        return 0;
    s += slen - tlen;
    for (i = 0; i < tlen; i++) {
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)suffix[i]))
            return 0;
    }
    return 1;
}

static void str_to_upper(char *s)
{
    for (; *s; ++s)
        *s = (char)toupper((unsigned char)*s);
}

static FILE *cue_get_file_for_image(const char *path)
{
    char cue_path_base[STRING_MAX];
    char cue_path[STRING_MAX];
    const char *exts[] = {".cue", ".CUE"};
    char *last_dot;
    int i;

    safe_copy(cue_path_base, path, sizeof(cue_path_base));
    last_dot = strrchr(cue_path_base, '.');
    if (!last_dot)
        return NULL;

    *last_dot = '\0';
    for (i = 0; i < 2; i++) {
        snprintf(cue_path, sizeof(cue_path), "%s%s", cue_path_base, exts[i]);
        FILE *cue_file = fopen(cue_path, "r");
        if (cue_file)
            return cue_file;
    }

    return NULL;
}

static char *trim_left(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    return s;
}

static void trim_right(char *s)
{
    size_t len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

static char *extract_file_name(const char *path, char *line)
{
    char file[STRING_MAX];
    char base_path[STRING_MAX];
    char *src;
    char *end;
    char *last_fwd;
    char *last_back;
    char slash = '/';

    file[0] = '\0';
    src = strchr(line, '"');
    if (src) {
        src++;
        end = strchr(src, '"');
        if (!end)
            return NULL;
        if ((size_t)(end - src) >= sizeof(file))
            return NULL;
        memcpy(file, src, (size_t)(end - src));
        file[end - src] = '\0';
    } else {
        char tmp[STRING_MAX];
        char upper[STRING_MAX];
        char *binary;
        char *token;

        safe_copy(tmp, line, sizeof(tmp));
        safe_copy(upper, line, sizeof(upper));
        str_to_upper(upper);
        binary = strstr(upper, " BINARY");
        if (binary)
            tmp[binary - upper] = '\0';
        token = trim_left(tmp);
        if (toupper((unsigned char)token[0]) == 'F' &&
            toupper((unsigned char)token[1]) == 'I' &&
            toupper((unsigned char)token[2]) == 'L' &&
            toupper((unsigned char)token[3]) == 'E')
            token = trim_left(token + 4);
        trim_right(token);
        if (!*token)
            return NULL;
        safe_copy(file, token, sizeof(file));
    }

    /* CUE files may contain Windows separators even on POSIX hosts. */
    for (src = file; *src; src++) {
        if (*src == '\\')
            *src = '/';
    }

    /* Absolute or explicitly relative paths can be used as-is. */
    if (file[0] == '/' || !strncmp(file, "./", 2) || !strncmp(file, "../", 3))
        return strdup(file);

    safe_copy(base_path, path, sizeof(base_path));
    last_fwd = strrchr(base_path, '/');
    last_back = strrchr(base_path, '\\');
    if (last_back && (!last_fwd || last_back > last_fwd)) {
        slash = '\\';
        last_fwd = last_back;
    }

    if (last_fwd) {
        size_t need;
        char *joined;
        *last_fwd = '\0';
        need = strlen(base_path) + 1 + strlen(file) + 1;
        joined = (char *)malloc(need);
        if (!joined)
            return NULL;
        snprintf(joined, need, "%s%c%s", base_path, slash, file);
        return joined;
    }

    return strdup(file);
}

static int line_has_word_ci(const char *line, const char *word)
{
    char tmp[STRING_MAX];
    safe_copy(tmp, line, sizeof(tmp));
    str_to_upper(tmp);
    return strstr(tmp, word) != NULL;
}

cueFile *cue_get(const char *path)
{
    FILE *cue_file = cue_is_cue_path(path) ? fopen(path, "r") : cue_get_file_for_image(path);
    cueFile *cue;
    char line[STRING_MAX];
    int files_found = 0;

    if (!cue_file)
        return NULL;

    cue = (cueFile *)calloc(1, sizeof(cueFile));
    if (!cue) {
        fclose(cue_file);
        return NULL;
    }
    cue->cd_format = CUE_MODE_UNKNOWN;

    while (fgets(line, sizeof(line), cue_file)) {
        char upper[STRING_MAX];

        if (!files_found && line_has_word_ci(line, "FILE")) {
            char *cd_image = extract_file_name(path, line);
            if (cd_image) {
                files_found++;
                cue->cd_image = cd_image;
            }
        }

        safe_copy(upper, line, sizeof(upper));
        str_to_upper(upper);
        if (strstr(upper, "TRACK 01")) {
            if (strstr(upper, "TRACK 01 MODE1/2048"))
                cue->cd_format = MODE1_2048;
            else if (strstr(upper, "TRACK 01 MODE1/2352"))
                cue->cd_format = MODE1_2352;
            else if (strstr(upper, "TRACK 01 MODE2/2352"))
                cue->cd_format = MODE2_2352;
            break;
        }
    }
    fclose(cue_file);

    if (cue->cd_format != CUE_MODE_UNKNOWN)
        return cue;

    free(cue->cd_image);
    free(cue);
    return NULL;
}

void cue_free(cueFile *cue)
{
    if (!cue)
        return;
    free(cue->cd_image);
    free(cue);
}

const char *cue_get_cd_format_name(CD_format cd_format)
{
    switch (cd_format) {
        case MODE1_2048: return "MODE1/2048";
        case MODE1_2352: return "MODE1/2352";
        case MODE2_2352: return "MODE2/2352";
        default: return "UNKNOWN";
    }
}

int cue_is_cue_path(const char *path)
{
    return has_suffix_ci(path, ".cue");
}
