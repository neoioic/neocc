/* Temporary strings migrated from TinyCC libtcc.c, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_TEMPORARIES_H
#define NEOCC_TEMPORARIES_H

#include "neocc/preprocessor.h"

#define NEOCC_AC0_TEMPORARIES_RESET AC0_
#define NEOCC_AC0_TEMPORARIES_ENTER (AC0_ + 1)
#define NEOCC_AC0_TEMPORARIES_UNWIND (AC0_ + 2)
#define NEOCC_AC0_TEMPORARIES_LEAVE (AC0_ + 3)
#define NEOCC_AC1_TEMPORARIES_PUSH AC1_
#define NEOCC_AC2_TEMPORARIES_POP AC2_

/* PUSH creates an empty string and outputs struct neocc_string *. The object
 * owns both the descriptor and its C-allocator buffer; callers only borrow.
 * POP takes that pointer and releases exactly the current string (LIFO).
 * Messages use checked lengths and memcpy, accepting unaligned buffers.
 * Allocation failure leaves the stack unchanged and never diagnoses in VO.
 *
 * ENTER pushes a compilation boundary. UNWIND frees its strings before a
 * diagnostic jump, retaining the boundary. With no boundary UNWIND is a no-op.
 * LEAVE frees strings and removes the nearest boundary; it rejects an absent
 * boundary. Outer strings, including source text for generated compilations,
 * remain valid. RESET/final close release all records without dereferencing
 * parser stack addresses. POP/UNWIND/LEAVE/RESET end the corresponding borrows.
 * Shared intermediate close preserves records; partial close only permits
 * retrying close. No fixed depth, table growth or descriptor relocation.
 */

#endif
