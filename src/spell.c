#define _POSIX_C_SOURCE 200809L
#include "spell.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int is_whitespace(unsigned char c) {
    return c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\v' || c=='\f';
}

static void to_lower_copy(const char *src, char *dst) {
    while (*src) { *dst++ = (char)tolower((unsigned char)*src++); }
    *dst = '\0';
}

static int is_leading_opener(unsigned char c) {
    switch (c) {
        case '(':
        case '[':
        case '{':
        case '\'':
        case '\"':
            return 1;
        default: return 0;
    }
}

static int is_trailing_symbol(unsigned char c) {
    switch (c) {
        case '!': case '@': case '#': case '$': case '%': case '^': case '&':
        case '*': case ')': case ']': case '}': case '\'': case '\"':
        case ';': case ':': case ',': case '.': case '?': case '-':
        case '_': case '+': case '=': case '/': case '\\':
            return 1;
        default: return 0;
    }
}

static int has_any_letter_n(const char *s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (isalpha((unsigned char)s[i])) return 1;
    }
    return 0;
}

static char *strndup_c(const char *s, size_t n) {
    char *p = (char*)malloc(n+1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

typedef struct {
    char *tok;
    size_t tok_len;
    const char *label;
    const Dict *D;
    bool print_filename;
    unsigned *miss_count;
} FlushCtx;

static void flush_token_impl(FlushCtx *ctx, unsigned long out_line, unsigned long out_col_raw) {
    if (ctx->tok_len == 0) return;
    ctx->tok[ctx->tok_len] = '\0';

    size_t start = 0, end = ctx->tok_len;
    while (start < end && is_leading_opener((unsigned char)ctx->tok[start])) start++;
    while (end > start && is_trailing_symbol((unsigned char)ctx->tok[end-1])) end--;

    if (end > start) {
        size_t L = end - start;
        if (has_any_letter_n(ctx->tok + start, L)) {
            char *trim = strndup_c(ctx->tok + start, L);
            if (!trim) { ctx->tok_len = 0; return; }
            char *lower = (char*)malloc(L + 1);
            if (!lower) { free(trim); ctx->tok_len = 0; return; }
            to_lower_copy(trim, lower);

            if (!dict_contains(ctx->D, lower, trim)) {
                unsigned long out_col = out_col_raw + (unsigned long)start;
                if (ctx->print_filename) {
                    dprintf(STDOUT_FILENO, "%s:%lu:%lu %s\n",
                            ctx->label, out_line, out_col, trim);
                } else {
                    dprintf(STDOUT_FILENO, "%lu:%lu %s\n",
                            out_line, out_col, trim);
                }
                if (ctx->miss_count) (*ctx->miss_count)++;
            }
            free(lower);
            free(trim);
        }
    }
    ctx->tok_len = 0;
}

int spell_check_fd(const char *label, int fd, const Dict *D, bool print_filename, unsigned *miss_count) {
    const size_t BUFSZ = 4096;
    char buf[BUFSZ];
    ssize_t rd;

    size_t tok_cap = 256;
    char *tok = (char*)malloc(tok_cap);
    if (!tok) return -1;

    size_t tok_len = 0;
    int in_token = 0;
    unsigned long line = 1;
    unsigned long col  = 0;   // column of previous char; next char is col+1
    unsigned long tok_line = 1;
    unsigned long tok_col  = 1;

    FlushCtx fctx = { .tok = tok, .tok_len = 0, .label = label, .D = D,
                      .print_filename = print_filename, .miss_count = miss_count };

    while ((rd = read(fd, buf, BUFSZ)) > 0) {
        for (ssize_t i = 0; i < rd; ++i) {
            unsigned char c = (unsigned char)buf[i];
            if (c == '\n') {
                if (in_token) {
                    fctx.tok = tok;
                    fctx.tok_len = tok_len;
                    flush_token_impl(&fctx, tok_line, tok_col);
                    in_token = 0;
                    tok_len = 0;
                }
                line++;
                col = 0;
                continue;
            }
            if (is_whitespace(c)) {
                if (in_token) {
                    fctx.tok = tok;
                    fctx.tok_len = tok_len;
                    flush_token_impl(&fctx, tok_line, tok_col);
                    in_token = 0;
                    tok_len = 0;
                }
            } else {
                if (!in_token) { in_token = 1; tok_len = 0; tok_line = line; tok_col = col + 1; }
                if (tok_len + 1 >= tok_cap) {
                    size_t ncap = tok_cap * 2;
                    char *nb = (char*)realloc(tok, ncap);
                    if (!nb) { free(tok); return -1; }
                    tok = nb; tok_cap = ncap;
                }
                tok[tok_len++] = (char)c;
            }
            col++;
        }
    }
    if (rd < 0) { free(tok); return -1; }
    if (in_token) {
        fctx.tok = tok;
        fctx.tok_len = tok_len;
        flush_token_impl(&fctx, tok_line, tok_col);
    }
    free(tok);
    return 0;
}
