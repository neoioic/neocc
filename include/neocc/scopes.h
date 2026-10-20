/* Scope layout migrated from TinyCC tccgen.c, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_SCOPES_H
#define NEOCC_SCOPES_H

#include "neocc/symbols.h"

#define NEOCC_AC0_SCOPES_RESET AC0_
#define NEOCC_AC1_SCOPES_BORROW AC1_
#define NEOCC_AC1_SCOPES_ENTER (AC1_ + 1)
#define NEOCC_AC2_SCOPES_EXIT AC2_

struct neocc_scope {
	struct neocc_scope *prev;
	struct { int loc, locorig, num; } vla;
	struct { struct neocc_symbol *s; int n; } cl;
	int *bsym, *csym;
	struct neocc_symbol *lstk, *llstk;
};

/* Workspace borrowed through VO while open. The object owns scope frames,
 * current and root; the algorithm may update loop and the borrowed symbol
 * chains. Symbol records remain owned by symbols, and jump targets borrow
 * parser locals. Remove these references before resetting symbol storage.
 * Reset preserves the workspace address but invalidates every frame borrow.
 * Close ends all borrows; reopen requires another workspace borrow.
 */
struct neocc_scopes {
	struct neocc_scope *current, *loop, *root;
	struct neocc_symbol *all_cleanups, *pending_gotos;
};

/* In-process pointer messages copied with memcpy, including unaligned data.
 * BORROW outputs struct neocc_scopes *. ENTER outputs struct neocc_scope *:
 * a zero root if empty, otherwise a copy of current with prev set to current
 * and vla.num zero. It publishes nothing on allocation/capacity failure.
 * EXIT takes exactly one struct neocc_scope *, which must be current and
 * no longer loop. It restores prev and recycles the frame; the frame borrow
 * ends immediately. The algorithm performs VLA/cleanup emission and symbol
 * lookup removal before EXIT. Frames are reused until reset or final close.
 */

#endif
