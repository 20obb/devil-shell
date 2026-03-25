/*
 * DevilShell (dsh) — main.c
 * Entry point: initialization, REPL loop, signal handling, prompt.
 */

#include "shell.h"
#include "input.h"
#include "parser.h"
#include "executor.h"
#include "builtins.h"
#include "sandbox.h"
#include "env.h"
#include "utils.h"

#include <signal.h>
#include <pwd.h>

/* ── Global Shell State ───────────────────────────────────────── */

static ShellState g_state;

ShellState *dsh_state(void)
{
    return &g_state;
}

/* ── Signal Handlers ──────────────────────────────────────────── */

static void sig_handler(int sig)
{
    if (sig == SIGINT) {
        /* Ctrl+C: interrupt current command, not the shell */
        write(STDOUT_FILENO, "\n", 1);
    }
    /* SIGCHLD: reap background children */
    if (sig == SIGCHLD) {
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0)
            ;  /* reap all finished children */
    }
}

static void setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    /* Ignore these in the shell process */
    signal(SIGTSTP, SIG_IGN);  /* Ctrl+Z */
    signal(SIGQUIT, SIG_IGN);  /* Ctrl+\ */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

/* ── Build Prompt ─────────────────────────────────────────────── */

static void build_prompt(char *buf, size_t size)
{
    ShellState *st = dsh_state();

    char short_path[DSH_MAX_PATH];
    dsh_shorten_path(st->cwd, st->home_dir, short_path, sizeof(short_path));

    if (st->sandbox.active) {
        snprintf(buf, size,
            CLR_BOLD CLR_BMAGENTA "┌─[" CLR_BWHITE "%s"
            CLR_BMAGENTA "@" CLR_BCYAN "dsh" CLR_BMAGENTA "]─["
            CLR_BYELLOW "%s" CLR_BMAGENTA "]─["
            CLR_BRED "🔒 %s" CLR_BMAGENTA "]\n"
            "└──╼ " CLR_RESET,
            st->username ? st->username : "user",
            short_path,
            st->sandbox.profile_name);
    } else {
        int status_color_ok = st->last_status == 0;
        snprintf(buf, size,
            CLR_BOLD CLR_BBLUE "┌─["
            CLR_BWHITE "%s"
            CLR_BBLUE "@" CLR_BCYAN "dsh"
            CLR_BBLUE "]─[" CLR_BGREEN "%s" CLR_BBLUE "]\n"
            "└──╼ %s" DSH_PROMPT_SYMBOL " " CLR_RESET,
            st->username ? st->username : "user",
            short_path,
            status_color_ok ? CLR_BGREEN : CLR_BRED);
    }
}

/* ── Load RC File ─────────────────────────────────────────────── */

static void load_rc_file(void)
{
    ShellState *st = dsh_state();
    char path[DSH_MAX_PATH];

    snprintf(path, sizeof(path), "%s/%s",
             st->home_dir ? st->home_dir : ".", DSH_RC_FILE);

    FILE *f = fopen(path, "r");
    if (!f) return;

    DSH_INFO("loading %s", path);

    char line[DSH_MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        /* Skip empty lines and comments */
        char *trimmed = dsh_strtrim(line);
        if (dsh_str_empty(trimmed) || trimmed[0] == '#') continue;

        /* Parse and execute */
        Pipeline pls[DSH_MAX_PIPES];
        int npl = parser_parse(trimmed, pls, DSH_MAX_PIPES);
        for (int i = 0; i < npl; i++) {
            executor_run_pipeline(&pls[i]);
            parser_free_pipeline(&pls[i]);
        }
    }

    fclose(f);
}

/* ── Banner ───────────────────────────────────────────────────── */

static void print_banner(void)
{
    printf("\n");
    printf(CLR_BOLD CLR_BRED
           "     ██████╗ ███████╗██╗   ██╗██╗██╗     \n"
           "     ██╔══██╗██╔════╝██║   ██║██║██║     \n"
           "     ██║  ██║█████╗  ██║   ██║██║██║     \n"
           "     ██║  ██║██╔══╝  ╚██╗ ██╔╝██║██║     \n"
           "     ██████╔╝███████╗ ╚████╔╝ ██║███████╗\n"
           "     ╚═════╝ ╚══════╝  ╚═══╝  ╚═╝╚══════╝\n"
           CLR_RESET);
    printf(CLR_BOLD CLR_BCYAN
           "            ╔═══════════════╗\n"
           "            ║   " CLR_BWHITE "S H E L L"
           CLR_BCYAN "   ║\n"
           "            ╚═══════════════╝\n"
           CLR_RESET);
    printf(CLR_DIM
           "        v%s — sandboxed & fast\n"
           "        Type 'help' for commands\n"
           CLR_RESET "\n", DSH_VERSION);
}

/* ── Initialization ───────────────────────────────────────────── */

void dsh_init(void)
{
    ShellState *st = &g_state;
    memset(st, 0, sizeof(*st));

    st->running = 1;
    st->interactive = isatty(STDIN_FILENO);
    st->last_status = 0;
    st->history_count = 0;
    st->history_pos = 0;

    /* Get hostname */
    gethostname(st->hostname, sizeof(st->hostname));

    /* Get username */
    struct passwd *pw = getpwuid(getuid());
    st->username = pw ? dsh_strdup(pw->pw_name) : dsh_strdup("user");

    /* Get home directory */
    const char *home = getenv("HOME");
    if (!home && pw) home = pw->pw_dir;
    st->home_dir = home ? dsh_strdup(home) : dsh_strdup("/");

    /* Get current working directory */
    getcwd(st->cwd, sizeof(st->cwd));

    /* Initialize environment */
    env_init();
    env_set("PWD", st->cwd);

    /* Setup signals */
    setup_signals();

    /* Initialize sandbox subsystem (Rust FFI) */
    dsh_sandbox_init();

    /* Load history */
    input_history_load();

    /* Load RC file */
    if (st->interactive) {
        load_rc_file();
    }
}

/* ── Cleanup ──────────────────────────────────────────────────── */

void dsh_cleanup(void)
{
    ShellState *st = &g_state;

    /* Save history */
    input_history_save();

    /* Cleanup sandbox */
    dsh_sandbox_cleanup();

    /* Free history entries */
    for (int i = 0; i < st->history_count; i++) {
        free(st->history[i]);
    }

    /* Free allocated strings */
    free(st->username);
    free(st->home_dir);

    /* Restore terminal */
    input_raw_mode_disable();
}

/* ── REPL Loop ────────────────────────────────────────────────── */

static void repl(void)
{
    ShellState *st = dsh_state();
    char prompt[2048];

    while (st->running) {
        /* Build prompt */
        build_prompt(prompt, sizeof(prompt));

        /* Read input */
        char *line = input_readline(prompt);

        if (!line) {
            /* EOF (Ctrl+D) */
            printf("\n");
            break;
        }

        /* Skip empty lines */
        char *trimmed = dsh_strtrim(line);
        if (dsh_str_empty(trimmed)) {
            free(line);
            continue;
        }

        /* Add to history */
        input_history_add(trimmed);

        /* Parse into pipelines */
        Pipeline pls[DSH_MAX_PIPES];
        int npl = parser_parse(trimmed, pls, DSH_MAX_PIPES);

        /* Execute each pipeline */
        for (int i = 0; i < npl; i++) {
            executor_run_pipeline(&pls[i]);
            parser_free_pipeline(&pls[i]);
        }

        free(line);
    }
}

/* ── Non-Interactive Mode ─────────────────────────────────────── */

static void run_script(FILE *f)
{
    ShellState *st = dsh_state();
    char line[DSH_MAX_LINE];

    while (st->running && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        char *trimmed = dsh_strtrim(line);
        if (dsh_str_empty(trimmed) || trimmed[0] == '#') continue;

        Pipeline pls[DSH_MAX_PIPES];
        int npl = parser_parse(trimmed, pls, DSH_MAX_PIPES);
        for (int i = 0; i < npl; i++) {
            executor_run_pipeline(&pls[i]);
            parser_free_pipeline(&pls[i]);
        }
    }
}

/* ── Entry Point ──────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    dsh_init();

    ShellState *st = dsh_state();

    if (argc >= 2) {
        /* Script mode: dsh script.sh */
        FILE *f = fopen(argv[1], "r");
        if (!f) {
            DSH_ERROR("cannot open '%s': %s", argv[1], strerror(errno));
            dsh_cleanup();
            return 1;
        }
        run_script(f);
        fclose(f);
    } else if (st->interactive) {
        /* Interactive mode */
        print_banner();
        repl();
    } else {
        /* Piped input: echo "ls" | dsh */
        run_script(stdin);
    }

    dsh_cleanup();
    return st->last_status;
}
