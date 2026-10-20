/* Translation-unit state migrated from TinyCC tccgen.c, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_TRANSLATION_H
#define NEOCC_TRANSLATION_H

#include "object.h"

#define NEOCC_AC0_TRANSLATION_RESET AC0_
#define NEOCC_AC1_TRANSLATION_BORROW AC1_

struct Section;

/* Mutable in-process workspace, borrowed through VO until final close.
 * The parser retains TinyCC's numbering and suppression bit semantics;
 * instruction is the current emission/patch cursor, not a second section size.
 * section only borrows linker storage and never owns its buffer.
 * RESET clears all state and section references, preserving this address.
 * Reset before parsing even in preprocessing mode, and after debug/semantic
 * cleanup on normal or diagnostic completion. Reopen requires another BORROW.
 */
struct neocc_translation {
	int anonymous_symbol;
	int instruction;
	int suppression;
	char debug_modes;
	struct Section *section;
};

/* BORROW outputs one struct neocc_translation * via memcpy; short capacity
 * reports zero bytes and is rejected. RESET has no stream or allocation.
 * Shared intermediate close preserves state; final close clears all borrows.
 * Partial close permits only another close. Objects never diagnose or jump.
 */

#endif
