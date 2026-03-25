/*
 * DevilShell (dsh) — builtins.c
 * Built-in commands: cd, exit, help, history, export, unset, env, sandbox
 */

#include "builtins.h"
#include "sandbox.h"
#include "input.h"
#include "env.h"
#include "utils.h"

#include <dirent.h>

/* ── Builtin Table ────────────────────────────────────────────── */

typedef struct {
    const char *name;
    int       (*fn)(Command *cmd);
    const char *help;
} BuiltinEntry;

static BuiltinEntry builtins[] = {
    { "cd",       builtin_cd,       "Change directory" },
    { "exit",     builtin_exit,     "Exit the shell" },
    { "quit",     builtin_exit,     "Exit the shell" },
    { "help",     builtin_help,     "Show this help message" },
    { "history",  builtin_history,  "Show command history" },
    { "export",   builtin_export,   "Set environment variable (export VAR=val)" },
    { "unset",    builtin_unset,    "Remove environment variable" },
    { "env",      builtin_env,      "Show all environment variables" },
    { "sandbox",  builtin_sandbox,  "Sandbox control (sandbox [profile|status|exit])" },
    { NULL, NULL, NULL }
};

/* ── Dispatch ─────────────────────────────────────────────────── */

int builtin_is(const char *name)
{
    if (!name) return 0;
    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(name, builtins[i].name) == 0) return 1;
    }
    return 0;
}

int builtin_exec(Command *cmd)
{
    if (!cmd || cmd->argc == 0) return 1;
    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(cmd->argv[0], builtins[i].name) == 0) {
            return builtins[i].fn(cmd);
        }
    }
    return 1;
}

/* ── cd ───────────────────────────────────────────────────────── */

int builtin_cd(Command *cmd)
{
    ShellState *st = dsh_state();
    const char *target;

    if (cmd->argc < 2) {
        target = st->home_dir ? st->home_dir : "/";
    } else if (strcmp(cmd->argv[1], "-") == 0) {
        target = env_get("OLDPWD");
        if (!target) {
            DSH_ERROR("cd: OLDPWD not set");
            return 1;
        }
        printf("%s\n", target);
    } else {
        /* Expand ~ */
        char *expanded = env_expand(cmd->argv[1]);
        target = expanded ? expanded : cmd->argv[1];

        if (chdir(target) != 0) {
            DSH_ERROR("cd: %s: %s", target, strerror(errno));
            if (expanded) free(expanded);
            return 1;
        }

        /* Update state */
        env_set("OLDPWD", st->cwd);
        getcwd(st->cwd, sizeof(st->cwd));
        env_set("PWD", st->cwd);
        if (expanded) free(expanded);
        return 0;
    }

    if (chdir(target) != 0) {
        DSH_ERROR("cd: %s: %s", target, strerror(errno));
        return 1;
    }

    env_set("OLDPWD", st->cwd);
    getcwd(st->cwd, sizeof(st->cwd));
    env_set("PWD", st->cwd);
    return 0;
}

/* ── exit ─────────────────────────────────────────────────────── */

int builtin_exit(Command *cmd)
{
    ShellState *st = dsh_state();

    int code = 0;
    if (cmd->argc >= 2) {
        code = atoi(cmd->argv[1]);
    }

    st->running = 0;
    st->last_status = code;
    return code;
}

/* ── help ─────────────────────────────────────────────────────── */

int builtin_help(Command *cmd)
{
    (void)cmd;

    printf("\n");
    printf(CLR_BCYAN CLR_BOLD "  ╔══════════════════════════════════════╗\n");
    printf("  ║     " CLR_BWHITE "DevilShell" CLR_BCYAN " v%s" CLR_BCYAN "              ║\n", DSH_VERSION);
    printf("  ║     " CLR_DIM "Custom Sandboxed Shell" CLR_RESET CLR_BCYAN CLR_BOLD "            ║\n");
    printf("  ╚══════════════════════════════════════╝\n" CLR_RESET);
    printf("\n");

    printf(CLR_BOLD "  Built-in Commands:\n" CLR_RESET);
    printf(CLR_DIM "  ──────────────────────────────────────\n" CLR_RESET);

    for (int i = 0; builtins[i].name; i++) {
        printf("   " CLR_BGREEN "%-12s" CLR_RESET " %s\n",
               builtins[i].name, builtins[i].help);
    }

    printf("\n");
    printf(CLR_BOLD "  Sandbox Profiles:\n" CLR_RESET);
    printf(CLR_DIM "  ──────────────────────────────────────\n" CLR_RESET);
    printf("   " CLR_BYELLOW "default" CLR_RESET "      Permissive — log blocked syscalls\n");
    printf("   " CLR_BYELLOW "strict" CLR_RESET "       Minimal syscalls, high isolation\n");
    printf("   " CLR_BYELLOW "network" CLR_RESET "      Block all network syscalls\n");

    printf("\n");
    printf(CLR_BOLD "  Features:\n" CLR_RESET);
    printf(CLR_DIM "  ──────────────────────────────────────\n" CLR_RESET);
    printf("   " CLR_CYAN "Pipes" CLR_RESET "        cmd1 | cmd2 | cmd3\n");
    printf("   " CLR_CYAN "Redirect" CLR_RESET "     cmd > file, cmd >> file, cmd < file\n");
    printf("   " CLR_CYAN "Background" CLR_RESET "   cmd &\n");
    printf("   " CLR_CYAN "Variables" CLR_RESET "    $VAR, ${VAR}\n");
    printf("   " CLR_CYAN "History" CLR_RESET "      Arrow keys ↑↓, Ctrl+R\n");
    printf("   " CLR_CYAN "Editing" CLR_RESET "      Ctrl+A/E (home/end), Ctrl+K/U (kill)\n");
    printf("\n");

    return 0;
}

/* ── history ──────────────────────────────────────────────────── */

int builtin_history(Command *cmd)
{
    (void)cmd;
    ShellState *st = dsh_state();

    for (int i = 0; i < st->history_count; i++) {
        printf(" " CLR_DIM "%4d" CLR_RESET "  %s\n", i + 1, st->history[i]);
    }
    return 0;
}

/* ── export ────────────────────────────────────────────────────── */

int builtin_export(Command *cmd)
{
    if (cmd->argc < 2) {
        env_print_all();
        return 0;
    }

    for (int i = 1; i < cmd->argc; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (eq) {
            *eq = '\0';
            env_set(cmd->argv[i], eq + 1);
            *eq = '=';
        } else {
            /* export without = just marks it for export (noop in our impl) */
            const char *val = env_get(cmd->argv[i]);
            if (val) {
                env_set(cmd->argv[i], val);
            }
        }
    }
    return 0;
}

/* ── unset ─────────────────────────────────────────────────────── */

int builtin_unset(Command *cmd)
{
    for (int i = 1; i < cmd->argc; i++) {
        env_unset(cmd->argv[i]);
    }
    return 0;
}

/* ── env ──────────────────────────────────────────────────────── */

int builtin_env(Command *cmd)
{
    (void)cmd;
    env_print_all();
    return 0;
}

/* ── sandbox ──────────────────────────────────────────────────── */

int builtin_sandbox(Command *cmd)
{
    ShellState *st = dsh_state();

    if (cmd->argc < 2) {
        printf(CLR_BOLD "Usage:" CLR_RESET " sandbox <profile|status|exit>\n");
        printf("\n");
        printf("  sandbox default    Enter sandbox with default profile\n");
        printf("  sandbox strict     Enter sandbox with strict profile\n");
        printf("  sandbox network    Enter sandbox with network isolation\n");
        printf("  sandbox status     Show current sandbox status\n");
        printf("  sandbox exit       Exit sandbox mode\n");
        return 0;
    }

    /* sandbox status */
    if (strcmp(cmd->argv[1], "status") == 0) {
        if (!st->sandbox.active) {
            printf(CLR_DIM "  Sandbox: " CLR_BRED "inactive" CLR_RESET "\n");
        } else {
            char status_buf[1024];
            if (dsh_sandbox_get_status(status_buf, sizeof(status_buf)) == 0) {
                printf("%s", status_buf);
            } else {
                printf(CLR_BGREEN "  Sandbox: active" CLR_RESET "\n");
                printf("  Profile: %s\n", st->sandbox.profile_name);
                printf("  Namespaces:\n");
                printf("    PID:     %s\n", st->sandbox.ns_pid ? "yes" : "no");
                printf("    Mount:   %s\n", st->sandbox.ns_mount ? "yes" : "no");
                printf("    Network: %s\n", st->sandbox.ns_net ? "yes" : "no");
                printf("    User:    %s\n", st->sandbox.ns_user ? "yes" : "no");
                printf("    UTS:     %s\n", st->sandbox.ns_uts ? "yes" : "no");
                printf("  Seccomp:   %s\n", st->sandbox.seccomp_enabled ? "yes" : "no");
            }
        }
        return 0;
    }

    /* sandbox exit */
    if (strcmp(cmd->argv[1], "exit") == 0) {
        if (!st->sandbox.active) {
            DSH_WARN("sandbox is not active");
            return 1;
        }
        st->sandbox.active = 0;
        memset(st->sandbox.profile_name, 0,
               sizeof(st->sandbox.profile_name));
        DSH_OK("sandbox mode deactivated");
        return 0;
    }

    /* sandbox <profile> — activate sandbox with given profile */
    const char *profile_name = cmd->argv[1];
    char profile_path[DSH_MAX_PATH];

    if (sandbox_resolve_profile(profile_name, profile_path,
                                sizeof(profile_path)) != 0) {
        DSH_ERROR("unknown sandbox profile: %s", profile_name);
        return 1;
    }

    /* Load profile via Rust FFI */
    if (dsh_sandbox_load_profile(profile_path) != 0) {
        DSH_ERROR("failed to load sandbox profile: %s", profile_name);
        return 1;
    }

    st->sandbox.active = 1;
    strncpy(st->sandbox.profile_name, profile_name,
            sizeof(st->sandbox.profile_name) - 1);
    strncpy(st->sandbox.profile_path, profile_path,
            sizeof(st->sandbox.profile_path) - 1);

    DSH_OK("sandbox activated — profile: " CLR_BYELLOW "%s" CLR_RESET,
           profile_name);

    return 0;
}
