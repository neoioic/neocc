/* SPDX-License-Identifier: MIT */
#ifndef OPENOICC_TEXT_H
#define OPENOICC_TEXT_H
#include <stddef.h>
struct text {
	char *data;
	size_t length;
	size_t capacity;
};
int text_append(struct text *text, const void *data, size_t length);
int text_printf(struct text *text, const char *format, ...);
int file_read(const char *path, struct text *text);
/* Prepare is exclusive and in the destination directory. Commit replaces only
 * the named destination. The caller cleans prepared paths on any failure. */
int file_prepare(const char *path, const struct text *text, char **temporary);
int file_commit(const char *path, const char *temporary, int replace);
int file_regular(const char *path);
int file_directory(const char *path);
int directory_create(const char *path);
int directory_remove(const char *path);
int directory_current(struct text *path);
int directory_visit(const char *path,
	int (*visit)(const char *, void *), void *context);
#endif
