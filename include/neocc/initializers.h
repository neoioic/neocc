/* Initializer state migrated from TinyCC tccgen.c, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_INITIALIZERS_H
#define NEOCC_INITIALIZERS_H

#include "neocc/preprocessor.h"

#define NEOCC_AC0_INITIALIZERS_RESET AC0_
#define NEOCC_AC1_INITIALIZERS_BORROW AC1_

/* Mutable in-process workspace borrowed through VO while open. The shared
 * string serves array initializers and parse_mult_str consumers (attributes,
 * asm and assertions). Its contents last until the next string parse/reset;
 * consumers copy any text they retain. global_expression nests by save/restore
 * in the parser and selects static storage for compound literals.
 * RESET frees string storage and clears the mode, preserving this address.
 * Final close ends the borrow; reopen requires another BORROW.
 * Initializer replay streams belong to tokens and the preprocessor macro
 * stack once pushed; unwind them before resetting the token arena.
 */
struct neocc_initializers {
	struct neocc_string string;
	int global_expression;
};

/* BORROW outputs one struct neocc_initializers * using memcpy; short output
 * capacity is rejected and reports zero bytes. RESET needs no byte stream.
 * String allocation uses the shared reserve/clear contract, never a compiler
 * allocator. Objects never diagnose or jump across VO access records.
 */

#endif
