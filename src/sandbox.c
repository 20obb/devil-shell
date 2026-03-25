/*
 * DevilShell (dsh) — sandbox.c
 * C-side sandbox helpers and Rust FFI wrappers.
 * The heavy lifting (namespace setup, seccomp BPF) is in sandbox-rs (Rust).
 */

#include "sandbox.h"
#include "utils.h"

#include <libgen.h>

/* ── Profile Resolution ───────────────────────────────────────── */

/*
 * Look for a profile file in:
 *   1. ./profiles/<name>.profile
 *   2. /etc/dsh/profiles/<name>.profile
 *   3. ~/.config/dsh/profiles/<name>.profile
 */
int sandbox_resolve_profile(const char *name, char *out_path, int len)
{
    if (!name || !out_path) return -1;

    /* If the name already has a path separator, use it directly */
    if (strchr(name, '/') != NULL) {
        if (access(name, R_OK) == 0) {
            snprintf(out_path, len, "%s", name);
            return 0;
        }
        return -1;
    }

    char paths[4][DSH_MAX_PATH];
    int npaths = 0;

    /* Try relative to executable location (development mode) */
    /* Read /proc/self/exe to find the binary's directory */
    char exe_path[DSH_MAX_PATH];
    ssize_t exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (exe_len > 0) {
        exe_path[exe_len] = '\0';
        char *dir = dirname(exe_path);
        snprintf(paths[npaths++], DSH_MAX_PATH,
                 "%s/profiles/%s.profile", dir, name);
    }

    /* Current directory */
    snprintf(paths[npaths++], DSH_MAX_PATH,
             "./profiles/%s.profile", name);

    /* System-wide */
    snprintf(paths[npaths++], DSH_MAX_PATH,
             "/etc/dsh/profiles/%s.profile", name);

    /* User config */
    const char *home = getenv("HOME");
    if (home) {
        snprintf(paths[npaths++], DSH_MAX_PATH,
                 "%s/.config/dsh/profiles/%s.profile", home, name);
    }

    for (int i = 0; i < npaths; i++) {
        if (access(paths[i], R_OK) == 0) {
            snprintf(out_path, len, "%s", paths[i]);
            return 0;
        }
    }

    return -1;
}

/* ── Apply Sandbox to Child Process ───────────────────────────── */

int sandbox_apply_to_child(void)
{
    ShellState *st = dsh_state();

    if (!st->sandbox.active) return 0;

    /* Step 1: Apply namespace isolation (via Rust FFI) */
    int ret = dsh_sandbox_apply_namespaces();
    if (ret != 0) {
        DSH_ERROR("failed to apply namespace isolation (code %d)", ret);
        return -1;
    }

    /* Step 2: Apply seccomp filter (via Rust FFI) */
    if (st->sandbox.seccomp_enabled) {
        ret = dsh_sandbox_apply_seccomp();
        if (ret != 0) {
            DSH_ERROR("failed to apply seccomp filter (code %d)", ret);
            return -1;
        }
    }

    return 0;
}
