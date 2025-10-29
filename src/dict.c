#define _POSIX_C_SOURCE 200809L
#include "dict.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    char *lower;
    char *orig;
} DictEntry;

struct Dict {
    DictEntry *entries;
    size_t     count;
};

static int cmp_lower(const void *a, const void *b) {
    const DictEntry *ea = (const DictEntry *)a;
    const DictEntry *eb = (const DictEntry *)b;
    int c = strcmp(ea->lower, eb->lower);
    if (c != 0) return c;
    return strcmp(ea->orig, eb->orig);
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void to_lower_inplace(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static int is_all_lower_alpha_or_nonalpha(const char *s) {
    // Return 1 if there is at least one letter and *all* letters are lowercase.
    // If there are no letters, return 0 (so we don't treat it as "all lowercase word").
    int has_letter = 0;
    for (; *s; ++s) {
        if (isalpha((unsigned char)*s)) {
            has_letter = 1;
            if (!islower((unsigned char)*s)) return 0;
        }
    }
    return has_letter ? 1 : 0;
}

Dict *dict_load(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    char *buf = NULL;
    size_t cap = 0, len = 0;

    char tmp[4096];
    ssize_t rd;
    while ((rd = read(fd, tmp, sizeof(tmp))) > 0) {
        if (len + (size_t)rd + 1 > cap) {
            size_t ncap = (cap ? cap * 2 : 8192);
            while (ncap < len + (size_t)rd + 1) ncap *= 2;
            char *nb = (char *)realloc(buf, ncap);
            if (!nb) { free(buf); close(fd); return NULL; }
            buf = nb; cap = ncap;
        }
        memcpy(buf + len, tmp, (size_t)rd);
        len += (size_t)rd;
    }
    close(fd);
    if (rd < 0) { free(buf); return NULL; }
    if (!buf) { buf = (char *)malloc(1); if (!buf) return NULL; }
    buf[len] = '\0';

    Dict *D = (Dict *)calloc(1, sizeof(*D));
    if (!D) { free(buf); return NULL; }

    size_t count = 0;
    for (size_t i = 0; i < len; ) {
        size_t j = i;
        while (j < len && buf[j] != '\n') j++;
        if (j > i) count++;
        i = j + 1;
    }
    D->entries = (DictEntry *)calloc(count, sizeof(DictEntry));
    if (!D->entries) { free(D); free(buf); return NULL; }

    size_t idx = 0;
    for (size_t i = 0; i < len; ) {
        size_t j = i;
        while (j < len && buf[j] != '\n') j++;
        if (j > i) {
            size_t L = j - i;
            char *line = (char *)malloc(L + 1);
            if (!line) { /* defer cleanup */ }
            memcpy(line, buf + i, L);
            line[L] = '\0';
            if (L && line[L-1] == '\r') line[L-1] = '\0';

            char *orig = line;
            char *lower = xstrdup(line);
            if (!lower || !orig) { /* defer */ }
            to_lower_inplace(lower);

            D->entries[idx].lower = lower;
            D->entries[idx].orig  = orig;
            idx++;
        }
        i = j + 1;
    }
    free(buf);
    D->count = idx;

    qsort(D->entries, D->count, sizeof(DictEntry), cmp_lower);
    return D;
}

static size_t lower_bound(const DictEntry *a, size_t n, const char *key) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = strcmp(a[mid].lower, key);
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return lo;
}

bool dict_contains(const Dict *D, const char *lower_key, const char *original) {
    if (!D || !lower_key || !original) return false;
    size_t i = lower_bound(D->entries, D->count, lower_key);
    if (i == D->count || strcmp(D->entries[i].lower, lower_key) != 0) {
        return false;
    }
    int has_all_lower_form = 0;
    for (size_t k = i; k < D->count && strcmp(D->entries[k].lower, lower_key) == 0; ++k) {
        if (is_all_lower_alpha_or_nonalpha(D->entries[k].orig)) {
            has_all_lower_form = 1;
        }
        if (strcmp(D->entries[k].orig, original) == 0) {
            return true; // exact case match found
        }
    }
    if (has_all_lower_form) return true;
    return false;
}

void dict_free(Dict *D) {
    if (!D) return;
    for (size_t i = 0; i < D->count; ++i) {
        free(D->entries[i].lower);
        free(D->entries[i].orig);
    }
    free(D->entries);
    free(D);
}
