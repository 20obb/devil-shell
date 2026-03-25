/*
 * DevilShell (dsh) — sandbox.h
 * C-side declarations for the Rust sandbox FFI layer.
 *
 * The actual implementation lives in sandbox-rs/ (Rust static library).
 * This header declares the C-callable FFI functions plus a thin
 * wrapper used by the executor.
 */

#ifndef DSH_SANDBOX_H
#define DSH_SANDBOX_H

#include "shell.h"

/* ── Rust FFI functions (implemented in sandbox-rs) ───────────── */

/*
 * Initialize the sandbox subsystem (call once at shell startup).
 * Returns 0 on success, -1 on failure.
 */
extern int  dsh_sandbox_init(void);

/*
 * Load and parse a sandbox profile file.
 * Populates internal Rust state with namespace/seccomp configuration.
 * Returns 0 on success, -1 on failure.
 */
extern int  dsh_sandbox_load_profile(const char *profile_path);

/*
 * Apply namespace isolation (called in child process after fork,
 * before exec). Uses unshare() with the flags from the loaded profile.
 * Returns 0 on success, -1 on failure.
 */
extern int  dsh_sandbox_apply_namespaces(void);

/*
 * Apply seccomp BPF filter (called in child process after
 * namespace setup, before exec).
 * Returns 0 on success, -1 on failure.
 */
extern int  dsh_sandbox_apply_seccomp(void);

/*
 * Write a human-readable status string into buf (max len bytes).
 * Returns 0 on success.
 */
extern int  dsh_sandbox_get_status(char *buf, int len);

/*
 * Clean up sandbox subsystem (call at shell exit).
 */
extern void dsh_sandbox_cleanup(void);

/* ── C-side helpers ───────────────────────────────────────────── */

/*
 * Resolve a profile name ("default", "strict", etc.) to its
 * full filesystem path.  Writes into out_path (max len bytes).
 * Returns 0 on success, -1 if profile not found.
 */
int sandbox_resolve_profile(const char *name, char *out_path, int len);

/*
 * Apply full sandbox to the current process (namespaces + seccomp).
 * Intended to be called inside a child process before exec.
 * Returns 0 on success, -1 on failure.
 */
int sandbox_apply_to_child(void);

#endif /* DSH_SANDBOX_H */
