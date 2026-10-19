#ifndef NEOCC_TEST_CHECK_H
#define NEOCC_TEST_CHECK_H

#include <stdio.h>
#include <stdlib.h>

/* Evaluates actions in Release, too. */
#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

#endif
