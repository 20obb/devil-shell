/*
 * DevilShell (dsh) — parser.c
 * Tokenizer: split input into commands, handle quotes, pipes, redirects.
 */

#include "parser.h"
#include "env.h"
#include "utils.h"
#include <ctype.h>

/* ── Internal: Token Types ────────────────────────────────────── */

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APPEND,
    TOK_BACKGROUND,
    TOK_SEMICOLON,
    TOK_END
} TokenType;

typedef struct {
    TokenType type;
    char     *value;     /* only meaningful for TOK_WORD */
} Token;

/* ── Tokenizer ────────────────────────────────────────────────── */

/*
 * Tokenize the input line.
 * tokens: output array (must be big enough; DSH_MAX_LINE entries is safe).
 * Returns number of tokens.
 */
static int tokenize(const char *line, Token *tokens, int max_tokens)
{
    int  n = 0;
    const char *p = line;

    while (*p && n < max_tokens - 1) {
        /* Skip whitespace */
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* Pipe */
        if (*p == '|') {
            tokens[n].type = TOK_PIPE;
            tokens[n].value = NULL;
            n++; p++;
            continue;
        }

        /* Semicolon */
        if (*p == ';') {
            tokens[n].type = TOK_SEMICOLON;
            tokens[n].value = NULL;
            n++; p++;
            continue;
        }

        /* Background */
        if (*p == '&') {
            tokens[n].type = TOK_BACKGROUND;
            tokens[n].value = NULL;
            n++; p++;
            continue;
        }

        /* Redirect append >> */
        if (*p == '>' && *(p + 1) == '>') {
            tokens[n].type = TOK_REDIR_APPEND;
            tokens[n].value = NULL;
            n++; p += 2;
            continue;
        }

        /* Redirect output > */
        if (*p == '>') {
            tokens[n].type = TOK_REDIR_OUT;
            tokens[n].value = NULL;
            n++; p++;
            continue;
        }

        /* Redirect input < */
        if (*p == '<') {
            tokens[n].type = TOK_REDIR_IN;
            tokens[n].value = NULL;
            n++; p++;
            continue;
        }

        /* Comment: skip rest of line */
        if (*p == '#') {
            break;
        }

        /* Word: handle quotes and escapes */
        char word[DSH_MAX_LINE];
        int  wlen = 0;

        while (*p && !isspace((unsigned char)*p)) {
            /* Stop at special characters (unless inside quotes) */
            if (*p == '|' || *p == ';' || *p == '&' ||
                *p == '>' || *p == '<' || *p == '#')
                break;

            if (*p == '\\' && *(p + 1)) {
                /* Escape: take next char literally */
                p++;
                if (wlen < DSH_MAX_LINE - 1) word[wlen++] = *p;
                p++;
            } else if (*p == '"') {
                /* Double-quoted string: $VAR expansion happens later */
                p++;  /* skip opening quote */
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p + 1) == '"') {
                        p++;
                        if (wlen < DSH_MAX_LINE - 1) word[wlen++] = '"';
                        p++;
                    } else {
                        if (wlen < DSH_MAX_LINE - 1) word[wlen++] = *p;
                        p++;
                    }
                }
                if (*p == '"') p++;  /* skip closing quote */
            } else if (*p == '\'') {
                /* Single-quoted string: no expansion */
                p++;
                while (*p && *p != '\'') {
                    if (wlen < DSH_MAX_LINE - 1) word[wlen++] = *p;
                    p++;
                }
                if (*p == '\'') p++;
            } else {
                if (wlen < DSH_MAX_LINE - 1) word[wlen++] = *p;
                p++;
            }
        }

        word[wlen] = '\0';
        tokens[n].type = TOK_WORD;
        tokens[n].value = dsh_strdup(word);
        n++;
    }

    tokens[n].type = TOK_END;
    tokens[n].value = NULL;
    return n;
}

/* ── Command Initialization ───────────────────────────────────── */

static void cmd_init(Command *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->redir_out = REDIR_NONE;
}

/* ── Parse Token Stream into Pipelines ────────────────────────── */

int parser_parse(const char *line, Pipeline *pipelines, int max_pipelines)
{
    if (!line || dsh_str_empty(line)) return 0;

    Token tokens[DSH_MAX_LINE];
    int ntokens = tokenize(line, tokens, DSH_MAX_LINE);
    if (ntokens == 0) return 0;

    int npipelines = 0;
    int ti = 0;  /* token index */

    while (ti < ntokens && npipelines < max_pipelines) {
        Pipeline *pl = &pipelines[npipelines];
        memset(pl, 0, sizeof(*pl));
        pl->num_commands = 0;
        pl->background = 0;

        /* Parse one pipeline (commands separated by pipes) */
        while (ti < ntokens) {
            if (tokens[ti].type == TOK_SEMICOLON) {
                ti++;
                break;  /* end of this pipeline */
            }

            if (tokens[ti].type == TOK_BACKGROUND) {
                pl->background = 1;
                ti++;
                if (ti < ntokens && tokens[ti].type == TOK_SEMICOLON) ti++;
                break;
            }

            if (tokens[ti].type == TOK_PIPE) {
                ti++;
                /* Start next command in this pipeline */
                continue;
            }

            /* Build current command */
            if (pl->num_commands >= DSH_MAX_PIPES) break;
            Command *cmd = &pl->commands[pl->num_commands];
            cmd_init(cmd);

            /* Collect words and redirections for this command segment */
            while (ti < ntokens &&
                   tokens[ti].type != TOK_PIPE &&
                   tokens[ti].type != TOK_SEMICOLON &&
                   tokens[ti].type != TOK_BACKGROUND) {

                switch (tokens[ti].type) {
                case TOK_REDIR_IN:
                    ti++;
                    if (ti < ntokens && tokens[ti].type == TOK_WORD) {
                        cmd->input_file = tokens[ti].value;
                        tokens[ti].value = NULL;
                        ti++;
                    }
                    break;

                case TOK_REDIR_OUT:
                    ti++;
                    if (ti < ntokens && tokens[ti].type == TOK_WORD) {
                        cmd->output_file = tokens[ti].value;
                        tokens[ti].value = NULL;
                        cmd->redir_out = REDIR_OUTPUT;
                        ti++;
                    }
                    break;

                case TOK_REDIR_APPEND:
                    ti++;
                    if (ti < ntokens && tokens[ti].type == TOK_WORD) {
                        cmd->output_file = tokens[ti].value;
                        tokens[ti].value = NULL;
                        cmd->redir_out = REDIR_APPEND;
                        ti++;
                    }
                    break;

                case TOK_WORD:
                    if (cmd->argc < DSH_MAX_ARGS - 1) {
                        cmd->argv[cmd->argc] = tokens[ti].value;
                        tokens[ti].value = NULL;
                        cmd->argc++;
                    } else {
                        free(tokens[ti].value);
                        tokens[ti].value = NULL;
                    }
                    ti++;
                    break;

                default:
                    ti++;
                    break;
                }
            }

            cmd->argv[cmd->argc] = NULL;
            if (cmd->argc > 0) {
                pl->num_commands++;
            }
        }

        if (pl->num_commands > 0) {
            npipelines++;
        }
    }

    /* Free any remaining unused token values */
    for (int i = 0; i < ntokens; i++) {
        if (tokens[i].value) {
            free(tokens[i].value);
            tokens[i].value = NULL;
        }
    }

    return npipelines;
}

/* ── Variable Expansion ───────────────────────────────────────── */

void parser_expand_vars(Command *cmd)
{
    for (int i = 0; i < cmd->argc; i++) {
        if (cmd->argv[i] && strchr(cmd->argv[i], '$') != NULL) {
            char *expanded = env_expand(cmd->argv[i]);
            if (expanded) {
                free(cmd->argv[i]);
                cmd->argv[i] = expanded;
            }
        }
        /* Also expand ~ at the start of arguments */
        if (cmd->argv[i] && cmd->argv[i][0] == '~') {
            char *expanded = env_expand(cmd->argv[i]);
            if (expanded) {
                free(cmd->argv[i]);
                cmd->argv[i] = expanded;
            }
        }
    }

    /* Expand in redirect filenames too */
    if (cmd->input_file) {
        char *exp = env_expand(cmd->input_file);
        if (exp) { free(cmd->input_file); cmd->input_file = exp; }
    }
    if (cmd->output_file) {
        char *exp = env_expand(cmd->output_file);
        if (exp) { free(cmd->output_file); cmd->output_file = exp; }
    }
}

/* ── Cleanup ──────────────────────────────────────────────────── */

void parser_free_pipeline(Pipeline *pl)
{
    for (int i = 0; i < pl->num_commands; i++) {
        Command *cmd = &pl->commands[i];
        for (int j = 0; j < cmd->argc; j++) {
            free(cmd->argv[j]);
            cmd->argv[j] = NULL;
        }
        free(cmd->input_file);
        free(cmd->output_file);
        cmd->input_file = NULL;
        cmd->output_file = NULL;
        cmd->argc = 0;
    }
    pl->num_commands = 0;
}
