/* Switch state migrated from TinyCC tccgen.c, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_SWITCHES_H
#define NEOCC_SWITCHES_H

#include "neocc/expressions.h"

#define NEOCC_AC0_SWITCHES_RESET AC0_
#define NEOCC_AC1_SWITCHES_BORROW AC1_
#define NEOCC_AC1_SWITCHES_ENTER (AC1_ + 1)
#define NEOCC_AC1_SWITCHES_CASE (AC1_ + 2)
#define NEOCC_AC2_SWITCHES_EXIT AC2_
#define NEOCC_AC2_SWITCHES_REMOVE (AC2_ + 1)

struct neocc_scope;
struct neocc_case {
	int64_t v1, v2;
	int ind, line;
};

struct neocc_switch {
	struct neocc_case **p;
	int n;
	int capacity; /* Owned index capacity; modified only by the object. */
	int def_sym, nocode_wanted;
	int *bsym;
	struct neocc_scope *scope;
	struct neocc_switch *prev;
	struct neocc_expression_value sv;
};

struct neocc_switches {
	struct neocc_switch *current;
};

/* In-process memcpy messages, including unaligned streams. BORROW outputs
 * struct neocc_switches *, valid while open; RESET preserves its address.
 * ENTER outputs a zeroed struct neocc_switch *, linked as current. CASE
 * appends a zeroed case to current and outputs struct neocc_case *. Allocation
 * failure leaves the stack, index and records unchanged. Algorithm code may
 * sort p and update semantic fields; n/capacity/prev remain object-owned.
 * EXIT takes the current switch pointer, frees its cases/index/frame and
 * restores prev. REMOVE takes an int index in current->p, frees that case and
 * compacts the index (used by the original adjacent-range merge algorithm).
 * Frame/case borrows end at EXIT/REMOVE/RESET or final close. Scope, symbols,
 * and break target are borrowed; RESET never dereferences those references
 * after a diagnostic jump. RESET must precede resetting their owners.
 */
#endif
