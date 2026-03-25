/*
 * DevilShell (dsh) — builtins.h
 * Built-in shell commands (cd, exit, help, sandbox, etc.)
 */

#ifndef DSH_BUILTINS_H
#define DSH_BUILTINS_H

#include "shell.h"

/*
 * Check if a command name is a built-in.
 * Returns 1 if built-in, 0 otherwise.
 */
int  builtin_is(const char *name);

/*
 * Execute a built-in command.
 * Returns the exit status of the built-in.
 * Precondition: builtin_is(cmd->argv[0]) == 1.
 */
int  builtin_exec(Command *cmd);

/* Individual built-ins (also callable internally) */
int  builtin_cd(Command *cmd);
int  builtin_exit(Command *cmd);
int  builtin_help(Command *cmd);
int  builtin_history(Command *cmd);
int  builtin_export(Command *cmd);
int  builtin_unset(Command *cmd);
int  builtin_env(Command *cmd);
int  builtin_sandbox(Command *cmd);

#endif /* DSH_BUILTINS_H */
