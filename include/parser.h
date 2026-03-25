/*
 * DevilShell (dsh) — parser.h
 * Tokenizer and command parser
 */

#ifndef DSH_PARSER_H
#define DSH_PARSER_H

#include "shell.h"

/*
 * Parse a raw input line into one or more Pipeline structures.
 * Handles: quotes, pipes, redirections, background (&), semicolons (;).
 *
 * Returns the number of pipelines parsed, or -1 on error.
 * Caller must free each pipeline with parser_free_pipeline().
 *
 * pipelines:  output array (caller allocates, max DSH_MAX_PIPES entries)
 */
int  parser_parse(const char *line, Pipeline *pipelines, int max_pipelines);

/*
 * Expand shell variables ($VAR, ${VAR}) in all arguments of a command.
 * Modifies argv in-place (allocates new strings, frees old ones).
 */
void parser_expand_vars(Command *cmd);

/*
 * Free internal allocations in a Pipeline (argv strings, file names).
 */
void parser_free_pipeline(Pipeline *pipeline);

#endif /* DSH_PARSER_H */
