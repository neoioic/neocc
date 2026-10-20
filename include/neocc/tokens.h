/* TinyCC-derived token layout, LGPL-2.0-or-later; see tinycc/COPYING. */
#ifndef NEOCC_TOKENS_H
#define NEOCC_TOKENS_H

#include <stddef.h>
#include "object.h"
#include "neocc/preprocessor.h"

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
	struct neocc_symbol *sym_define;
	struct neocc_symbol *sym_label;
	struct neocc_symbol *sym_struct;
	struct neocc_symbol *sym_identifier;
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

/* Deferred inline bodies share this object's token allocation domain.
 * filename is a NUL-terminated string borrowed only for SAVE.
 * SAVE copies filename and publishes a stable record with an empty body.
 * The parser may build body and clear symbol after generation; next and
 * filename are read-only. Symbols are borrowed, never freed here.
 * Replay must use PREPROCESSOR_PUSH alloc=0: the stack only borrows body.
 * Detach all macro-stack borrows before RESET_INLINES, RESET or final close,
 * and release inline records before resetting symbols. RESET_INLINES frees
 * bodies and records but preserves all other tokens/streams. FIRST returns
 * a borrowed head (NULL for empty); records remain valid until cleanup.
 * All messages are in-process and copied with memcpy, including pointers.
 */
struct neocc_inline {
	struct neocc_inline *next;
	struct neocc_token_string body;
	struct neocc_symbol *symbol;
	char filename[];
};

struct neocc_inline_save {
	struct neocc_symbol *symbol;
	const char *filename;
};

#define NEOCC_AC0_TOKENS_RESET_INLINES (AC0_ + 2)
#define NEOCC_AC1_TOKENS_FIRST_INLINE AC1_
#define NEOCC_AC3_TOKENS_SAVE_INLINE (AC3_ + 4)

#define NEOCC_AC0_TOKENS_RESET (AC0_ + 1)
#define NEOCC_AC3_TOKENS_INTERN (AC3_ + 1)
#define NEOCC_AC3_TOKENS_QUERY (AC3_ + 2)
#define NEOCC_AC3_TOKENS_RESIZE (AC3_ + 3)

#endif
