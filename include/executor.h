/*
 * DevilShell (dsh) — executor.h
 * Command execution engine: fork, exec, pipes, redirection, sandbox
 */

#ifndef DSH_EXECUTOR_H
#define DSH_EXECUTOR_H

#include "shell.h"

/*
 * Execute a full pipeline (one or more piped commands).
 * Handles fork/exec, pipe plumbing, I/O redirection, and background jobs.
 * If sandbox mode is active, applies namespace + seccomp isolation
 * to each child process before exec.
 *
 * Returns the exit status of the last command in the pipeline.
 */
int executor_run_pipeline(Pipeline *pipeline);

/*
 * Execute a single external command (called inside child process).
 * Sets up redirections, then calls execvp().
 * Does NOT return on success (replaced by exec).
 */
void executor_exec_command(Command *cmd);

#endif /* DSH_EXECUTOR_H */
