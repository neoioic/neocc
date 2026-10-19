#ifndef NEOIOIC_TEST_CHECK_H
#define NEOIOIC_TEST_CHECK_H

#include <stdio.h>
#include <stdlib.h>

/* Test actions and checks must also execute with NDEBUG. */
#define CHECK(expr)                                                   \
	do {                                                         \
		if (!(expr)) {                                       \
			fprintf(stderr, "%s:%d: %s\n", __FILE__,       \
				__LINE__, #expr);                     \
			exit(EXIT_FAILURE);                          \
		}                                                    \
	} while (0)

#endif
