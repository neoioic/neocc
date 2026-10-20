/* Function state migrated from TinyCC tccgen.c, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_FUNCTIONS_H
#define NEOCC_FUNCTIONS_H

#include "neocc/symbols.h"

#define NEOCC_FUNCTION_TEMP_CAPACITY 8
#define NEOCC_AC0_FUNCTIONS_RESET AC0_
#define NEOCC_AC1_FUNCTIONS_BORROW AC1_
#define NEOCC_AC2_FUNCTIONS_BEGIN AC2_
#define NEOCC_AC3_FUNCTIONS_TEMP AC3_

/* Inline workspace borrowed through VO while open. Types borrow symbols;
 * name borrows a token spelling. Reset before releasing either owner.
 * The parser also uses type for old-style parameter declarations, then BEGIN
 * replaces it with the function return type. Backend algorithms update the
 * local stack offset and implicit structure-return address in this workspace.
 * Reset preserves the address, clears all references/cache and sets name to
 * "" and start to -1. Close ends the borrow; reopen requires another borrow.
 */
struct neocc_function {
	struct neocc_type type;
	const char *name;
	int start;
	int variadic, old_style;
	int stack_offset, struct_return, return_jumps;
};

struct neocc_function_temp_request {
	int size, alignment;
	unsigned int used; /* Bit per cached slot still live on the value stack. */
};

struct neocc_function_temp_result {
	int offset;
	int slot; /* Cache index, or -1 when the fixed TinyCC cache is full. */
};

/* In-process messages use memcpy, including unaligned streams.
 * BORROW outputs struct neocc_function *. BEGIN takes one initial workspace
 * (non-NULL name, nonnegative start) and clears the old cache before copying.
 * TEMP takes one request and outputs one result: first-fit reuse of an unused
 * slot of sufficient size/alignment, otherwise reserve an aligned stack slot.
 * The eight-entry cache is an optimization, not an allocation limit. Size may
 * be zero; alignment must be a positive power of two. Invalid messages or
 * unrepresentable stack offsets leave the workspace and cache unchanged.
 */

#endif
