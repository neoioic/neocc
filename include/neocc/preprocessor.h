/* Working storage migrated from TinyCC tccpp.c (LGPL-2.0-or-later).
 * Copyright (c) 2001-2004 Fabrice Bellard.
 */
#ifndef NEOCC_PREPROCESSOR_H
#define NEOCC_PREPROCESSOR_H

#include "object.h"

#define NEOCC_AC0_PREPROCESSOR_RESET AC0_
#define NEOCC_AC1_PREPROCESSOR_BORROW AC1_
#define NEOCC_AC1_PREPROCESSOR_POP (AC1_ + 1)
#define NEOCC_AC2_PREPROCESSOR_PUSH AC2_
#define NEOCC_CHAR_SPACE 1
#define NEOCC_CHAR_ID 2
#define NEOCC_CHAR_NUMBER 4
#define NEOCC_PREPROCESSOR_STRING_MAX 1024

struct neocc_string {
	int size; /* size in bytes */
	int size_allocated;
	char *data; /* nwchar_t* in cases */
};

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
 * Reset invalidates string contents and stream references. Unwind macros
 * before resetting this object, then reset the tokens arena last.
 */
struct neocc_preprocessor {
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
