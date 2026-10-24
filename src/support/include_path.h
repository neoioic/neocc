/* Include-cache path rules migrated from TinyCC tccpp.c/libtcc.c.
 * Copyright (c) 2001-2004 Fabrice Bellard. LGPL-2.0-or-later.
 */
#ifndef NEOCC_INCLUDE_PATH_H
#define NEOCC_INCLUDE_PATH_H

#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif
#include <stdlib.h>
#include <string.h>

/* Keep TinyCC's basename hash and host path/pragma-once comparison. */
static inline const char *include_basename(const char *path)
{
	const char *name = path + strlen(path);

	while (name > path && name[-1] != '/'
#ifdef _WIN32
	       && name[-1] != '\\'
#endif
	       )
		name--;
	return name;
}

static inline int include_path_compare(const char *a, const char *b)
{
#ifdef _WIN32
	return stricmp(a, b);
#else
	return strcmp(a, b);
#endif
}

static inline int include_normalized_compare(const char *a, const char *b)
{
	char *first, *second;
	int result = 1;

#ifdef _WIN32
	first = _fullpath(NULL, a, 260);
#else
	first = realpath(a, NULL);
#endif
	if (!first)
		return result;
#ifdef _WIN32
	second = _fullpath(NULL, b, 260);
#else
	second = realpath(b, NULL);
#endif
	if (second) {
		result = include_path_compare(first, second);
		free(second);
	}
	free(first);
	return result;
}

static inline unsigned int include_hash(const char *basename)
{
	unsigned int hash = 1;

	for (const unsigned char *p = (const unsigned char *)basename; *p; p++) {
		unsigned int c = *p;

#ifdef _WIN32
		if (c >= 'a' && c <= 'z')
			c = c - 'a' + 'A';
#endif
		hash = hash + (hash << 5) + (hash >> 27) + c;
	}
	return hash;
}

#endif
