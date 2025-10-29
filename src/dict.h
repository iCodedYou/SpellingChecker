#pragma once
#include <stdbool.h>

typedef struct Dict Dict;

Dict *dict_load(const char *path);              // NULL on error
bool   dict_contains(const Dict *D,
                     const char *lower_key,     // lowercased token
                     const char *original);     // original token for caps check
void   dict_free(Dict *D);
