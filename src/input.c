/*
 * DevilShell (dsh) — input.c
 * Custom line editor with history, cursor movement, tab completion.
 * No external dependencies (no readline).
 */

#include "input.h"
#include "utils.h"
#include "env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/ioctl.h>

/* ── Terminal Raw Mode ────────────────────────────────────────── */

static struct termios orig_termios;
static int            raw_mode_active = 0;

void input_raw_mode_enable(void)
{
    if (raw_mode_active) return;
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
    raw_mode_active = 1;
}

void input_raw_mode_disable(void)
{
    if (!raw_mode_active) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_mode_active = 0;
}

/* ── Terminal Helpers ─────────────────────────────────────────── */

static int term_columns(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 80;
    return ws.ws_col;
}

/* ── History ──────────────────────────────────────────────────── */

void input_history_add(const char *line)
{
    if (!line || dsh_str_empty(line)) return;

    ShellState *st = dsh_state();

    /* Don't add duplicate of last entry */
    if (st->history_count > 0 &&
        strcmp(st->history[st->history_count - 1], line) == 0)
        return;

    if (st->history_count >= DSH_MAX_HISTORY) {
        /* Shift history: drop oldest entry */
        free(st->history[0]);
        memmove(st->history, st->history + 1,
                (DSH_MAX_HISTORY - 1) * sizeof(char *));
        st->history_count--;
    }

    st->history[st->history_count] = dsh_strdup(line);
    st->history_count++;
}

void input_history_load(void)
{
    ShellState *st = dsh_state();
    char path[DSH_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s",
             st->home_dir ? st->home_dir : ".", DSH_HISTORY_FILE);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[DSH_MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (!dsh_str_empty(line))
            input_history_add(line);
    }
    fclose(f);
}

void input_history_save(void)
{
    ShellState *st = dsh_state();
    char path[DSH_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s",
             st->home_dir ? st->home_dir : ".", DSH_HISTORY_FILE);

    FILE *f = fopen(path, "w");
    if (!f) return;

    /* Save last N entries */
    int start = st->history_count > DSH_MAX_HISTORY
                ? st->history_count - DSH_MAX_HISTORY : 0;
    for (int i = start; i < st->history_count; i++) {
        if (st->history[i])
            fprintf(f, "%s\n", st->history[i]);
    }
    fclose(f);
}

/* ── Tab Completion (file paths) ──────────────────────────────── */

static void tab_complete(char *buf, int *pos, int *len)
{
    /* Find the start of the current word */
    int word_start = *pos;
    while (word_start > 0 && buf[word_start - 1] != ' ')
        word_start--;

    char word[DSH_MAX_LINE];
    int wlen = *pos - word_start;
    memcpy(word, buf + word_start, wlen);
    word[wlen] = '\0';

    /* Separate directory and prefix */
    char dir_path[DSH_MAX_PATH] = ".";
    char *prefix = word;
    char *last_slash = strrchr(word, '/');

    if (last_slash) {
        *last_slash = '\0';
        if (word[0]) {
            /* Expand ~ in directory path */
            char *expanded = env_expand(word);
            if (expanded) {
                strncpy(dir_path, expanded, sizeof(dir_path) - 1);
                free(expanded);
            }
        } else {
            strcpy(dir_path, "/");
        }
        prefix = last_slash + 1;
    } else {
        /* Also expand ~ if the word starts with it */
        if (word[0] == '~') {
            char *expanded = env_expand(word);
            if (expanded) {
                strncpy(dir_path, expanded, sizeof(dir_path) - 1);
                free(expanded);
                prefix = "";
            }
        }
    }

    size_t plen = strlen(prefix);

    /* Search directory for matches */
    DIR *d = opendir(dir_path);
    if (!d) return;

    char *match = NULL;
    int match_count = 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.' && (plen == 0 || prefix[0] != '.'))
            continue;  /* skip hidden unless prefix starts with . */

        if (strncmp(ent->d_name, prefix, plen) == 0) {
            match_count++;
            if (match_count == 1) {
                match = dsh_strdup(ent->d_name);
            } else {
                /* Multiple matches — find common prefix */
                size_t mlen = strlen(match);
                size_t elen = strlen(ent->d_name);
                size_t common = 0;
                while (common < mlen && common < elen &&
                       match[common] == ent->d_name[common])
                    common++;
                match[common] = '\0';
            }
        }
    }
    closedir(d);

    if (match && strlen(match) > plen) {
        /* Insert the completion suffix */
        const char *suffix = match + plen;
        size_t slen = strlen(suffix);

        /* Make room */
        if (*len + (int)slen < DSH_MAX_LINE - 1) {
            memmove(buf + *pos + slen, buf + *pos, *len - *pos + 1);
            memcpy(buf + *pos, suffix, slen);
            *pos += (int)slen;
            *len += (int)slen;
            buf[*len] = '\0';
        }
    }

    if (match) free(match);

    if (match_count > 1) {
        /* Show all matches below the prompt */
        write(STDOUT_FILENO, "\n", 1);
        d = opendir(dir_path);
        if (d) {
            while ((ent = readdir(d)) != NULL) {
                if (ent->d_name[0] == '.' && (plen == 0 || prefix[0] != '.'))
                    continue;
                if (strncmp(ent->d_name, prefix, plen) == 0) {
                    write(STDOUT_FILENO, ent->d_name, strlen(ent->d_name));
                    write(STDOUT_FILENO, "  ", 2);
                }
            }
            closedir(d);
            write(STDOUT_FILENO, "\n", 1);
        }
    }
}

/* ── Refresh Line (redraw current input) ──────────────────────── */

static void refresh_line(const char *prompt, const char *buf,
                         int len, int pos)
{
    char seq[256];
    int prompt_vis_len = dsh_visible_len(prompt);
    (void)term_columns;  /* available for future wrapping logic */

    /* Carriage return, clear line */
    write(STDOUT_FILENO, "\r", 1);
    write(STDOUT_FILENO, "\033[K", 3);  /* clear to end of line */

    /* Write prompt */
    write(STDOUT_FILENO, prompt, strlen(prompt));

    /* Write buffer */
    write(STDOUT_FILENO, buf, len);

    /* Move cursor to correct position */
    int cursor_col = prompt_vis_len + pos;
    int n = snprintf(seq, sizeof(seq), "\r\033[%dC", cursor_col);
    write(STDOUT_FILENO, seq, n);
}

/* ── Main Readline ────────────────────────────────────────────── */

char *input_readline(const char *prompt)
{
    ShellState *st = dsh_state();
    char buf[DSH_MAX_LINE];
    int  pos = 0;  /* cursor position */
    int  len = 0;  /* buffer length   */

    buf[0] = '\0';
    st->history_pos = st->history_count;

    /* Display prompt */
    write(STDOUT_FILENO, prompt, strlen(prompt));

    input_raw_mode_enable();

    while (1) {
        char c;
        int nread = read(STDIN_FILENO, &c, 1);
        if (nread <= 0) {
            /* EOF (Ctrl+D on empty line) */
            input_raw_mode_disable();
            if (len == 0) return NULL;
            break;
        }

        switch (c) {
        case '\r':   /* Enter */
        case '\n':
            input_raw_mode_disable();
            write(STDOUT_FILENO, "\r\n", 2);
            buf[len] = '\0';
            return dsh_strdup(buf);

        case 4:  /* Ctrl+D */
            if (len == 0) {
                input_raw_mode_disable();
                return NULL;
            }
            /* Delete char under cursor */
            if (pos < len) {
                memmove(buf + pos, buf + pos + 1, len - pos);
                len--;
                buf[len] = '\0';
                refresh_line(prompt, buf, len, pos);
            }
            break;

        case 3:  /* Ctrl+C */
            input_raw_mode_disable();
            write(STDOUT_FILENO, "^C\r\n", 4);
            buf[0] = '\0';
            return dsh_strdup("");

        case 127:  /* Backspace */
        case 8:    /* Ctrl+H */
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos + 1);
                pos--;
                len--;
                buf[len] = '\0';
                refresh_line(prompt, buf, len, pos);
            }
            break;

        case 1:  /* Ctrl+A: move to beginning */
            pos = 0;
            refresh_line(prompt, buf, len, pos);
            break;

        case 5:  /* Ctrl+E: move to end */
            pos = len;
            refresh_line(prompt, buf, len, pos);
            break;

        case 11:  /* Ctrl+K: kill to end of line */
            buf[pos] = '\0';
            len = pos;
            refresh_line(prompt, buf, len, pos);
            break;

        case 21:  /* Ctrl+U: kill to beginning of line */
            memmove(buf, buf + pos, len - pos + 1);
            len -= pos;
            pos = 0;
            refresh_line(prompt, buf, len, pos);
            break;

        case 12:  /* Ctrl+L: clear screen */
            write(STDOUT_FILENO, "\033[H\033[2J", 7);
            refresh_line(prompt, buf, len, pos);
            break;

        case 23:  /* Ctrl+W: delete word backward */
            {
                int old_pos = pos;
                while (pos > 0 && buf[pos - 1] == ' ') pos--;
                while (pos > 0 && buf[pos - 1] != ' ') pos--;
                memmove(buf + pos, buf + old_pos, len - old_pos + 1);
                len -= (old_pos - pos);
                refresh_line(prompt, buf, len, pos);
            }
            break;

        case '\t':  /* Tab: completion */
            tab_complete(buf, &pos, &len);
            refresh_line(prompt, buf, len, pos);
            break;

        case 27:  /* Escape sequence */
            {
                char seq2[4] = {0};
                if (read(STDIN_FILENO, &seq2[0], 1) != 1) break;
                if (read(STDIN_FILENO, &seq2[1], 1) != 1) break;

                if (seq2[0] == '[') {
                    switch (seq2[1]) {
                    case 'A':  /* Up arrow: previous history */
                        if (st->history_pos > 0) {
                            st->history_pos--;
                            const char *h = st->history[st->history_pos];
                            strncpy(buf, h, DSH_MAX_LINE - 1);
                            buf[DSH_MAX_LINE - 1] = '\0';
                            len = pos = (int)strlen(buf);
                            refresh_line(prompt, buf, len, pos);
                        }
                        break;

                    case 'B':  /* Down arrow: next history */
                        if (st->history_pos < st->history_count - 1) {
                            st->history_pos++;
                            const char *h = st->history[st->history_pos];
                            strncpy(buf, h, DSH_MAX_LINE - 1);
                            buf[DSH_MAX_LINE - 1] = '\0';
                            len = pos = (int)strlen(buf);
                        } else {
                            st->history_pos = st->history_count;
                            buf[0] = '\0';
                            len = pos = 0;
                        }
                        refresh_line(prompt, buf, len, pos);
                        break;

                    case 'C':  /* Right arrow */
                        if (pos < len) {
                            pos++;
                            refresh_line(prompt, buf, len, pos);
                        }
                        break;

                    case 'D':  /* Left arrow */
                        if (pos > 0) {
                            pos--;
                            refresh_line(prompt, buf, len, pos);
                        }
                        break;

                    case 'H':  /* Home */
                        pos = 0;
                        refresh_line(prompt, buf, len, pos);
                        break;

                    case 'F':  /* End */
                        pos = len;
                        refresh_line(prompt, buf, len, pos);
                        break;

                    case '3':  /* Delete key (escape[3~) */
                        {
                            char tilde;
                            if (read(STDIN_FILENO, &tilde, 1) == 1 &&
                                tilde == '~' && pos < len) {
                                memmove(buf + pos, buf + pos + 1,
                                        len - pos);
                                len--;
                                buf[len] = '\0';
                                refresh_line(prompt, buf, len, pos);
                            }
                        }
                        break;
                    }
                }
            }
            break;

        default:
            /* Printable character */
            if (c >= 32 && len < DSH_MAX_LINE - 1) {
                if (pos == len) {
                    buf[pos] = c;
                    pos++;
                    len++;
                    buf[len] = '\0';
                    /* Optimization: just write the char if at end */
                    write(STDOUT_FILENO, &c, 1);
                } else {
                    /* Insert at cursor position */
                    memmove(buf + pos + 1, buf + pos, len - pos + 1);
                    buf[pos] = c;
                    pos++;
                    len++;
                    buf[len] = '\0';
                    refresh_line(prompt, buf, len, pos);
                }
            }
            break;
        }
    }

    input_raw_mode_disable();
    buf[len] = '\0';
    return dsh_strdup(buf);
}
