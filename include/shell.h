/*
 * DevilShell (dsh) — Custom Sandboxed Shell Framework
 * shell.h — Core types and declarations
 */

#ifndef DSH_SHELL_H
#define DSH_SHELL_H

/* Enable POSIX + GNU extensions (sigaction, setenv, gethostname, etc.) */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <linux/limits.h>
#include <limits.h>

/* ── Version & Limits ─────────────────────────────────────────── */

#define DSH_VERSION       "0.1.0"
#define DSH_NAME          "DevilShell"
#define DSH_PROMPT_SYMBOL "❯"

#define DSH_MAX_ARGS      128
#define DSH_MAX_LINE      4096
#define DSH_MAX_PIPES     16
#define DSH_MAX_HISTORY   1000
#define DSH_MAX_PATH      PATH_MAX

#define DSH_HISTORY_FILE  ".dsh_history"
#define DSH_RC_FILE       ".dshrc"

/* ── Redirect Types ───────────────────────────────────────────── */

typedef enum {
    REDIR_NONE = 0,
    REDIR_INPUT,       /* <  */
    REDIR_OUTPUT,      /* >  */
    REDIR_APPEND       /* >> */
} RedirType;

/* ── Command: a single command segment (between pipes) ────────── */

typedef struct {
    char   *argv[DSH_MAX_ARGS];   /* argument vector, NULL-terminated */
    int     argc;                  /* argument count                   */
    char   *input_file;            /* redirect input from file  (<)    */
    char   *output_file;           /* redirect output to file   (>/>>) */
    RedirType redir_out;           /* output redirect type             */
} Command;

/* ── Pipeline: one or more commands connected by pipes ────────── */

typedef struct {
    Command commands[DSH_MAX_PIPES]; /* command segments             */
    int     num_commands;            /* how many segments            */
    int     background;              /* run in background (&)        */
} Pipeline;

/* ── Sandbox State ────────────────────────────────────────────── */

typedef struct {
    int   active;                    /* 1 if sandbox mode is on       */
    char  profile_name[256];         /* name of current profile       */
    char  profile_path[DSH_MAX_PATH];/* full path to profile file     */
    int   ns_pid;                    /* isolate PID namespace         */
    int   ns_mount;                  /* isolate mount namespace       */
    int   ns_net;                    /* isolate network namespace     */
    int   ns_user;                   /* isolate user namespace        */
    int   ns_uts;                    /* isolate UTS namespace         */
    int   seccomp_enabled;           /* seccomp filtering active      */
    int   seccomp_log;               /* log blocked syscalls          */
} SandboxState;

/* ── Shell State (global singleton) ───────────────────────────── */

typedef struct {
    int           running;           /* main loop flag                */
    int           interactive;       /* is stdin a tty?               */
    int           last_status;       /* exit status of last command   */

    char          hostname[256];     /* machine hostname              */
    char         *username;          /* current user                  */
    char          cwd[DSH_MAX_PATH]; /* current working directory     */
    char         *home_dir;          /* user home directory           */

    SandboxState  sandbox;           /* sandbox configuration         */

    /* history */
    char         *history[DSH_MAX_HISTORY];
    int           history_count;
    int           history_pos;       /* for up/down navigation        */
} ShellState;

/* ── Global Access ────────────────────────────────────────────── */

ShellState *dsh_state(void);
void        dsh_init(void);
void        dsh_cleanup(void);

#endif /* DSH_SHELL_H */
