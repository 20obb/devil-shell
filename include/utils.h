/*
 * DevilShell (dsh) — utils.h
 * ANSI colors, string utilities, error reporting
 */

#ifndef DSH_UTILS_H
#define DSH_UTILS_H

#include <stdio.h>

/* ── ANSI Color Codes ─────────────────────────────────────────── */

#define CLR_RESET    "\033[0m"
#define CLR_BOLD     "\033[1m"
#define CLR_DIM      "\033[2m"
#define CLR_ITALIC   "\033[3m"
#define CLR_UNDER    "\033[4m"

/* Foreground colors */
#define CLR_BLACK    "\033[30m"
#define CLR_RED      "\033[31m"
#define CLR_GREEN    "\033[32m"
#define CLR_YELLOW   "\033[33m"
#define CLR_BLUE     "\033[34m"
#define CLR_MAGENTA  "\033[35m"
#define CLR_CYAN     "\033[36m"
#define CLR_WHITE    "\033[37m"

/* Bright foreground */
#define CLR_BRED     "\033[91m"
#define CLR_BGREEN   "\033[92m"
#define CLR_BYELLOW  "\033[93m"
#define CLR_BBLUE    "\033[94m"
#define CLR_BMAGENTA "\033[95m"
#define CLR_BCYAN    "\033[96m"
#define CLR_BWHITE   "\033[97m"

/* Background colors */
#define CLR_BG_BLACK "\033[40m"
#define CLR_BG_RED   "\033[41m"
#define CLR_BG_GREEN "\033[42m"

/* ── Logging / Error Macros ───────────────────────────────────── */

#define DSH_INFO(...)  \
    dsh_log(CLR_BCYAN CLR_BOLD "[dsh] " CLR_RESET, __VA_ARGS__)

#define DSH_WARN(...)  \
    dsh_log(CLR_BYELLOW CLR_BOLD "[dsh warn] " CLR_RESET, __VA_ARGS__)

#define DSH_ERROR(...) \
    dsh_log(CLR_BRED CLR_BOLD "[dsh error] " CLR_RESET, __VA_ARGS__)

#define DSH_OK(...)    \
    dsh_log(CLR_BGREEN CLR_BOLD "[dsh ✓] " CLR_RESET, __VA_ARGS__)

void dsh_log(const char *prefix, const char *fmt, ...);

/* ── String Utilities ─────────────────────────────────────────── */

/*
 * Duplicate a string (like strdup, but always available).
 */
char *dsh_strdup(const char *s);

/*
 * Trim leading and trailing whitespace in-place.
 * Returns pointer into the same buffer.
 */
char *dsh_strtrim(char *s);

/*
 * Check if a string is empty or whitespace-only.
 */
int   dsh_str_empty(const char *s);

/*
 * Safe snprintf wrapper: returns the output buffer for chaining.
 */
char *dsh_snprintf(char *buf, size_t size, const char *fmt, ...);

/*
 * Get the visible length of a string (ignoring ANSI escape codes).
 */
int   dsh_visible_len(const char *s);

/*
 * Shorten a path by replacing the home directory with ~.
 * Writes into out (max len bytes). Returns out.
 */
char *dsh_shorten_path(const char *path, const char *home, char *out, size_t len);

#endif /* DSH_UTILS_H */
