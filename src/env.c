/*
 * DevilShell (dsh) — env.c
 * Environment variable management and $VAR expansion
 */

#include "env.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern char **environ;

/* ── Init ─────────────────────────────────────────────────────── */

void env_init(void)
{
    /* Set DSH-specific variables */
    env_set("SHELL", "dsh");
    env_set("DSH_VERSION", DSH_VERSION);
}

/* ── Get / Set / Unset ────────────────────────────────────────── */

const char *env_get(const char *name)
{
    if (!name) return NULL;
    return getenv(name);
}

void env_set(const char *name, const char *value)
{
    if (!name) return;
    if (value) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
}

void env_unset(const char *name)
{
    if (name) unsetenv(name);
}

/* ── Print All ────────────────────────────────────────────────── */

void env_print_all(void)
{
    for (char **e = environ; *e; e++) {
        printf("%s\n", *e);
    }
}

/* ── Variable Expansion ───────────────────────────────────────── */

/*
 * Check if a character is valid in a variable name.
 */
static int is_var_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

char *env_expand(const char *str)
{
    if (!str) return NULL;

    /* Allocate a generously-sized buffer */
    size_t cap = strlen(str) * 2 + 256;
    char *result = malloc(cap);
    if (!result) return NULL;

    size_t pos = 0;
    const char *p = str;

    while (*p) {
        /* Ensure space */
        if (pos + 256 > cap) {
            cap *= 2;
            char *tmp = realloc(result, cap);
            if (!tmp) { free(result); return NULL; }
            result = tmp;
        }

        /* ~/ at the start → expand to home directory */
        if (p == str && *p == '~' && (*(p + 1) == '/' || *(p + 1) == '\0')) {
            const char *home = getenv("HOME");
            if (home) {
                size_t hlen = strlen(home);
                if (pos + hlen + 1 > cap) {
                    cap = pos + hlen + 256;
                    char *tmp = realloc(result, cap);
                    if (!tmp) { free(result); return NULL; }
                    result = tmp;
                }
                memcpy(result + pos, home, hlen);
                pos += hlen;
                p++;  /* skip the ~ */
                continue;
            }
        }

        /* $VAR or ${VAR} expansion */
        if (*p == '$' && *(p + 1)) {
            p++;  /* skip $ */
            int braced = 0;
            if (*p == '{') {
                braced = 1;
                p++;
            }

            /* Collect variable name */
            const char *var_start = p;
            while (*p && (braced ? (*p != '}') : is_var_char(*p))) {
                p++;
            }

            size_t var_len = (size_t)(p - var_start);
            if (braced && *p == '}') p++;  /* skip closing } */

            if (var_len > 0) {
                char var_name[256];
                if (var_len >= sizeof(var_name)) var_len = sizeof(var_name) - 1;
                memcpy(var_name, var_start, var_len);
                var_name[var_len] = '\0';

                const char *val = getenv(var_name);
                if (val) {
                    size_t vlen = strlen(val);
                    if (pos + vlen + 1 > cap) {
                        cap = pos + vlen + 256;
                        char *tmp = realloc(result, cap);
                        if (!tmp) { free(result); return NULL; }
                        result = tmp;
                    }
                    memcpy(result + pos, val, vlen);
                    pos += vlen;
                }
            }
            continue;
        }

        /* Regular character */
        result[pos++] = *p++;
    }

    result[pos] = '\0';
    return result;
}
