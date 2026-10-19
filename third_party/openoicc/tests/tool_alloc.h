/* SPDX-License-Identifier: MIT */
/* -include processes this header before source-level feature macros. Match
 * text.c's POSIX declarations before alloc.h loads any system headers. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "alloc.h"
void *test_realloc(void *memory, size_t size);
#define realloc test_realloc
