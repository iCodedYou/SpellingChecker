#pragma once
#include <stdbool.h>
#include "dict.h"

int spell_check_fd(const char *label,           // filename label or "(stdin)"
                   int fd,
                   const Dict *D,
                   bool print_filename,         // false for single-file or stdin
                   unsigned *miss_count);       // accumulates misses
