/* TinyCC-derived token layout, LGPL-2.0-or-later; see tinycc/COPYING. */
#ifndef NEOCC_TOKENS_H
#define NEOCC_TOKENS_H

#include <stddef.h>
#include "object.h"

#define NEOCC_TOKEN_FIRST 256
#define NEOCC_TOKEN_LIMIT 0x10000000

/* Symbol links are borrowed from the semantic parser. Token storage belongs
 * to the tokens object until reset or final close; callers must clear their
 * symbol stacks before then. Interning never invalidates earlier tokens.
 * Only the sym_* links may be changed by the parser; the hash link, ID,
 * length and spelling are read-only to borrowers.
 */
struct neocc_token {
	struct neocc_token *hash_next;
	struct Sym *sym_define;
	struct Sym *sym_label;
	struct Sym *sym_struct;
	struct Sym *sym_identifier;
	int tok;
	int len;
	char str[];
};

/* Process-local messages, copied with memcpy. Text is borrowed for the call.
 * Resize preserves min(old size, new size) bytes and the old allocation on
 * failure. A NULL pointer allocates; size zero frees. Non-NULL pointers must
 * be live stream allocations returned by this same object. Streams are
 * aligned for max_align_t and remain owned by the object until freed/reset.
 */
struct neocc_token_intern {
	const char *text;
	size_t length;
};

struct neocc_token_resize {
	void *pointer;
	size_t size;
};

/* QUERY takes a token ID; FIRST-1 queries the next unused ID only. A missing
 * ID returns RO_VAINLY and a NULL token. No table or hash bucket is exposed.
 */
struct neocc_token_result {
	struct neocc_token *token;
	int next;
};

#define NEOCC_AC0_TOKENS_RESET (AC0_ + 1)
#define NEOCC_AC3_TOKENS_INTERN (AC3_ + 1)
#define NEOCC_AC3_TOKENS_QUERY (AC3_ + 2)
#define NEOCC_AC3_TOKENS_RESIZE (AC3_ + 3)

#endif
