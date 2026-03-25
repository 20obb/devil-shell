/*
 * DevilShell (dsh) — executor.c
 * Execute external commands: fork/exec, pipes, I/O redirection,
 * background jobs, and sandbox integration.
 */

#include "executor.h"
#include "builtins.h"
#include "sandbox.h"
#include "parser.h"
#include "utils.h"
#include "env.h"

#include <fcntl.h>
#include <signal.h>

/* ── Setup I/O Redirection (called in child) ──────────────────── */

static int setup_redirects(Command *cmd)
{
    /* Input redirect: file → stdin */
    if (cmd->input_file) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            DSH_ERROR("cannot open '%s': %s", cmd->input_file,
                      strerror(errno));
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    /* Output redirect: stdout → file */
    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT;
        flags |= (cmd->redir_out == REDIR_APPEND) ? O_APPEND : O_TRUNC;

        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            DSH_ERROR("cannot open '%s': %s", cmd->output_file,
                      strerror(errno));
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    return 0;
}

/* ── Execute Single External Command (in child process) ───────── */

void executor_exec_command(Command *cmd)
{
    if (setup_redirects(cmd) != 0) {
        _exit(1);
    }

    execvp(cmd->argv[0], cmd->argv);

    /* If execvp returns, it failed */
    DSH_ERROR("%s: command not found", cmd->argv[0]);
    _exit(127);
}

/* ── Execute a Full Pipeline ──────────────────────────────────── */

int executor_run_pipeline(Pipeline *pl)
{
    ShellState *st = dsh_state();

    if (pl->num_commands == 0) return 0;

    /* ── Single command, possibly a builtin ─── */
    if (pl->num_commands == 1 && !pl->background) {
        Command *cmd = &pl->commands[0];

        /* Expand variables */
        parser_expand_vars(cmd);

        if (cmd->argc == 0) return 0;

        /* Check for built-in */
        if (builtin_is(cmd->argv[0])) {
            return builtin_exec(cmd);
        }
    }

    /* ── Pipeline or external command ─── */

    int num_cmds = pl->num_commands;
    int pipes[DSH_MAX_PIPES - 1][2];
    pid_t pids[DSH_MAX_PIPES];

    /* Create pipes between commands */
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            DSH_ERROR("pipe: %s", strerror(errno));
            return 1;
        }
    }

    /* Fork each command in the pipeline */
    for (int i = 0; i < num_cmds; i++) {
        Command *cmd = &pl->commands[i];

        /* Expand variables in each command */
        parser_expand_vars(cmd);

        if (cmd->argc == 0) {
            pids[i] = -1;
            continue;
        }

        pids[i] = fork();
        if (pids[i] < 0) {
            DSH_ERROR("fork: %s", strerror(errno));
            return 1;
        }

        if (pids[i] == 0) {
            /* ═══ Child Process ═══ */

            /* Restore default signal handlers */
            signal(SIGINT,  SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            /* Connect pipes:
             * cmd[i] reads from pipes[i-1] (if not first)
             * cmd[i] writes to pipes[i]   (if not last)
             */
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < num_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            /* Close all pipe fds in the child */
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            /* Apply sandbox if active */
            if (st->sandbox.active) {
                if (sandbox_apply_to_child() != 0) {
                    DSH_ERROR("sandbox setup failed, aborting command");
                    _exit(126);
                }
            }

            /* Execute the command (does not return on success) */
            executor_exec_command(cmd);
            _exit(127);
        }
    }

    /* ═══ Parent Process ═══ */

    /* Close all pipe fds in parent */
    for (int i = 0; i < num_cmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    /* Wait for children */
    int last_status = 0;

    if (pl->background) {
        /* Background: don't wait, print PID */
        for (int i = 0; i < num_cmds; i++) {
            if (pids[i] > 0) {
                DSH_INFO("[bg] PID %d", pids[i]);
            }
        }
    } else {
        /* Foreground: wait for all children */
        for (int i = 0; i < num_cmds; i++) {
            if (pids[i] > 0) {
                int status;
                waitpid(pids[i], &status, 0);
                if (i == num_cmds - 1) {
                    if (WIFEXITED(status))
                        last_status = WEXITSTATUS(status);
                    else if (WIFSIGNALED(status))
                        last_status = 128 + WTERMSIG(status);
                }
            }
        }
    }

    st->last_status = last_status;
    return last_status;
}
