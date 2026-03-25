/*
 * DevilShell (dsh) — input.h
 * Line input with custom line editing, history, and tab completion.
 * Self-contained: no dependency on GNU Readline.
 */

#ifndef DSH_INPUT_H
#define DSH_INPUT_H

#include "shell.h"

/*
 * Read a line of input with inline editing support.
 * Features: cursor movement, backspace, delete, history (up/down),
 *           Ctrl+A/E (home/end), Ctrl+C (cancel), Ctrl+D (EOF),
 *           basic tab completion for file paths.
 *
 * prompt: the prompt string to display (may contain ANSI escapes).
 * Returns a malloc'd string (caller frees), or NULL on EOF / error.
 */
char *input_readline(const char *prompt);

/*
 * Add a line to the in-memory history ring.
 */
void  input_history_add(const char *line);

/*
 * Load history from file (~/.dsh_history).
 */
void  input_history_load(void);

/*
 * Save history to file (~/.dsh_history).
 */
void  input_history_save(void);

/*
 * Set terminal to raw mode (disable canonical + echo).
 */
void  input_raw_mode_enable(void);

/*
 * Restore terminal to original mode.
 */
void  input_raw_mode_disable(void);

#endif /* DSH_INPUT_H */
