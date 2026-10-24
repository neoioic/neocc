/* Working storage migrated from TinyCC tccpp.c (LGPL-2.0-or-later).
 * Copyright (c) 2001-2004 Fabrice Bellard.
 */
#ifndef NEOCC_PREPROCESSOR_H
#define NEOCC_PREPROCESSOR_H

#include "object.h"
#include <stdarg.h>

#define NEOCC_AC0_PREPROCESSOR_RESET AC0_
#define NEOCC_AC1_PREPROCESSOR_BORROW AC1_
#define NEOCC_AC1_PREPROCESSOR_POP (AC1_ + 1)
#define NEOCC_AC1_PREPROCESSOR_RESULTS (AC1_ + 2)
#define NEOCC_AC2_PREPROCESSOR_PUSH AC2_
#define NEOCC_AC2_PREPROCESSOR_DEPENDENCY (AC2_ + 1)
#define NEOCC_AC2_PREPROCESSOR_LIBRARY (AC2_ + 2)
#define NEOCC_AC3_PREPROCESSOR_INCLUDE AC3_
#define NEOCC_CHAR_SPACE 1
#define NEOCC_CHAR_ID 2
#define NEOCC_CHAR_NUMBER 4
#define NEOCC_PREPROCESSOR_STRING_MAX 1024
/* Existing TinyCC algorithm limits; these are not target ABI values. */
#define NEOCC_INCLUDE_STACK_CAPACITY 32
#define NEOCC_IFDEF_STACK_CAPACITY 64
#define NEOCC_PACK_STACK_CAPACITY 8
#define NEOCC_INCLUDE_HASH_CAPACITY 32

/* Cache records and filenames belong to preprocessor until RESET/final close.
 * Only ifndef_macro/once may be changed through this mutable borrow.
 * INCLUDE looks up a path, optionally adding a zero-initialized record.
 * A miss without add succeeds with a NULL result; failure publishes nothing.
 */
struct neocc_cached_include {
	struct neocc_cached_include *next;
	int ifndef_macro;
	int once;
	char filename[];
};

struct neocc_include_request {
	const char *filename;
	int add;
};

struct neocc_preprocessor_list {
	char **items;
	size_t count;
	size_t capacity;
};

/* Read-only in-process borrow, retained across translation-unit RESET.
 * APPEND messages contain exactly one NUL-terminated string (including NUL).
 * Append copies the input and preserves order/duplicates, invalidating only
 * the affected items array on success. Individual strings remain valid until
 * final close. Intermediate/partial close preserves all resources.
 */
struct neocc_preprocessor_results {
	struct neocc_preprocessor_list libraries;
	struct neocc_preprocessor_list dependencies;
};

/* Constant payload and long-double words use the host representation,
 * preserving TinyCC's token-stream and direct code-generation layout.
 */
#define NEOCC_LDOUBLE_WORDS ((sizeof(long double) + 3) / 4)

union neocc_value {
	long double ld;
	double d;
	float f;
	uint64_t i;
	struct {
		char *data;
		int size;
	} str;
	int tab[NEOCC_LDOUBLE_WORDS];
};

struct neocc_source_buffer;

struct neocc_string {
	int size; /* size in bytes */
	int size_allocated;
	char *data; /* nwchar_t* in cases */
};

/* Append formatted bytes, retaining a trailing NUL outside size.
 * Returns the appended length, or -1 without changing size on failure.
 */
int neocc_string_vprintf(struct neocc_string *, const char *, va_list);
int neocc_string_printf(struct neocc_string *, const char *, ...);

struct neocc_token_string {
	int *str;
	int len;
	int need_spc;
	int allocated_len;
	int last_line_num;
	int save_line_num;
	/* used to chain token-strings with begin/end_macro() */
	struct neocc_token_string *prev;
	const int *prev_ptr;
	char alloc;
};

/* Push borrows stream and its arena allocation until matching pop.
 * alloc is 0 (reusable), 1 (owned stream), or 2 (borrowed stream data).
 * Pop returns the detached stream for release through the tokens object,
 * or NULL for a reusable stream. saved_line restores the source cursor.
 */
struct neocc_macro_push {
	struct neocc_token_string *stream;
	int alloc;
	int line;
};

struct neocc_macro_pop {
	struct neocc_token_string *release;
	int saved_line;
};

/* In-process mutable borrow for the compiler algorithms, valid while open.
 * Reset clears the cursor and invalidates string contents and stream references.
 * Unwind macros and pop sources before reset, then reset the tokens arena last.
 * Final close frees the workspace and ends the borrow; reopen must borrow again.
 */
struct neocc_preprocessor {
	int token;
	union neocc_value value;
	struct neocc_source_buffer *source; /* Borrowed from source object. */
	/* Include entries borrow source buffers; buffers borrow the conditional
	 * cursor to check file boundaries. Pop sources before resetting this state.
	 * All three stacks are initialized on open/reset, including early errors.
	 */
	struct neocc_source_buffer *include_stack[NEOCC_INCLUDE_STACK_CAPACITY];
	struct neocc_source_buffer **include_stack_ptr;
	int ifdef_stack[NEOCC_IFDEF_STACK_CAPACITY];
	int *ifdef_stack_ptr;
	int pack_stack[NEOCC_PACK_STACK_CAPACITY];
	int *pack_stack_ptr;
	int debug_indent;
	int token_flags;
	int parse_flags;
	const int *macro_ptr;
	struct neocc_token_string *macro_stack;
	struct neocc_string parsed;
	struct neocc_string spelling;
	struct neocc_string predefs;
	struct neocc_token_string expansion;
	struct neocc_token_string unget;
	char number[NEOCC_PREPROCESSOR_STRING_MAX + 1];
	unsigned char character[257]; /* EOF (-1) through byte 255. */
	int expression;
	int debug_token;
	int debug_symbol;
	int counter;
};

/* Shared string representation; reserve preserves old storage on failure.
 * These pure allocation helpers never diagnose or perform compiler jumps.
 */
int neocc_string_reserve(struct neocc_string *string, int size);
void neocc_string_clear(struct neocc_string *string);

#endif
