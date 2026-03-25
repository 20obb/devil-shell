/*
 * DevilShell (dsh) — utils.c
 * ANSI color helpers, string utilities, error reporting
 */

#include "utils.h"
#include "shell.h"
#include <stdarg.h>
#include <ctype.h>

/* ── Logging ──────────────────────────────────────────────────── */

void dsh_log(const char *prefix, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s", prefix);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/* ── String Utilities ─────────────────────────────────────────── */

char *dsh_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (dup) memcpy(dup, s, len + 1);
    return dup;
}

char *dsh_strtrim(char *s)
{
    if (!s) return NULL;

    /* trim leading */
    while (isspace((unsigned char)*s)) s++;

    if (*s == '\0') return s;

    /* trim trailing */
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    return s;
}

int dsh_str_empty(const char *s)
{
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

char *dsh_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return buf;
}

int dsh_visible_len(const char *s)
{
    int len = 0;
    int in_escape = 0;

    while (*s) {
        if (*s == '\033') {
            in_escape = 1;
        } else if (in_escape) {
            if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))
                in_escape = 0;
        } else {
            /* Handle multi-byte UTF-8: count codepoints, not bytes */
            if ((*s & 0xC0) != 0x80)
                len++;
        }
        s++;
    }
    return len;
}

char *dsh_shorten_path(const char *path, const char *home,
                       char *out, size_t len)
{
    if (!path || !out) return out;

    if (home && strncmp(path, home, strlen(home)) == 0) {
        snprintf(out, len, "~%s", path + strlen(home));
    } else {
        snprintf(out, len, "%s", path);
    }
    return out;
}
