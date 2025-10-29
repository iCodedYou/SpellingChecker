#define _POSIX_C_SOURCE 200809L
#include "spell.h"
#include "dict.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

static bool ends_with(const char *name, const char *suffix) {
    size_t n = strlen(name), m = strlen(suffix);
    if (m > n) return false;
    return strcmp(name + (n - m), suffix) == 0;
}

static int is_hidden(const char *name) {
    return name[0] == '.';
}

static int is_regular_file(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

static int is_directory(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static int process_file(const char *path, const Dict *D, bool print_filename, unsigned *miss_count, bool *opened_all) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        dprintf(STDERR_FILENO, "spell: cannot open %s: %s\n", path, strerror(errno));
        if (opened_all) *opened_all = false;
        return -1;
    }
    int rc = spell_check_fd(path, fd, D, print_filename, miss_count);
    close(fd);
    return rc;
}

static int process_dir(const char *dirpath, const char *suffix, const Dict *D, unsigned *miss_count, bool *opened_all) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        dprintf(STDERR_FILENO, "spell: cannot open directory %s: %s\n", dirpath, strerror(errno));
        if (opened_all) *opened_all = false;
        return -1;
    }
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;
        if (is_hidden(name)) continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dirpath, name);
        if (is_directory(path)) {
            process_dir(path, suffix, D, miss_count, opened_all);
        } else if (is_regular_file(path)) {
            if (ends_with(name, suffix)) {
                process_file(path, D, /*print_filename=*/true, miss_count, opened_all);
            }
        }
    }
    closedir(dir);
    return 0;
}

int main(int argc, char **argv) {
    const char *suffix = ".txt";
    int i = 1;
    if (i < argc && strcmp(argv[i], "-s") == 0) {
        if (i + 1 >= argc) {
            dprintf(STDERR_FILENO, "usage: spell [-s suffix] dictionary [file|dir]*\n");
            return EXIT_FAILURE;
        }
        suffix = argv[i+1];
        i += 2;
    }
    if (i >= argc) {
        dprintf(STDERR_FILENO, "usage: spell [-s suffix] dictionary [file|dir]*\n");
        return EXIT_FAILURE;
    }
    const char *dict_path = argv[i++];
    Dict *D = dict_load(dict_path);
    if (!D) {
        dprintf(STDERR_FILENO, "spell: failed to load dictionary %s\n", dict_path);
        return EXIT_FAILURE;
    }

    unsigned miss_count = 0;
    bool opened_all = true;

    if (i >= argc) {
        if (spell_check_fd("(stdin)", STDIN_FILENO, D, /*print_filename=*/false, &miss_count) != 0) {
            dict_free(D);
            return EXIT_FAILURE;
        }
    } else {
        int num_sources = argc - i;
        bool single_file = (num_sources == 1) && is_regular_file(argv[i]);
        for (; i < argc; ++i) {
            const char *path = argv[i];
            if (is_directory(path)) {
                process_dir(path, suffix, D, &miss_count, &opened_all);
            } else if (is_regular_file(path)) {
                process_file(path, D, /*print_filename=*/!single_file, &miss_count, &opened_all);
            } else {
                dprintf(STDERR_FILENO, "spell: %s: not a regular file or directory\n", path);
                opened_all = false;
            }
        }
    }

    dict_free(D);
    if (opened_all && miss_count == 0) return EXIT_SUCCESS;
    return EXIT_FAILURE;
}
