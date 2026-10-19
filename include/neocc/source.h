/*
 * Buffered input layout derived from TinyCC, Copyright (c) 2001-2004
 * Fabrice Bellard. LGPL-2.0-or-later; see tinycc/COPYING.
 */
#ifndef NEOCC_SOURCE_H
#define NEOCC_SOURCE_H

#include <stddef.h>
#include <stdint.h>
#include "object.h"

#define NEOCC_SOURCE_BUFFER_SIZE 8192
#define NEOCC_SOURCE_EOB '\\'

struct neocc_source_buffer {
	uint8_t *buf_ptr;
	uint8_t *buf_end;
	int fd;
	struct neocc_source_buffer *prev;
	int line_num;
	int line_ref;
	int ifndef_macro;
	int ifndef_macro_saved;
	int *ifdef_stack_ptr;
	int include_next_index;
	int prev_tok_flags;
	char filename[1024];
	char *true_filename;
	unsigned char unget[4];
	unsigned char buffer[];
};

/* Process-local messages, copied with memcpy; never a serialized format.
 * Push transfers fd > 0 only on success; fd 0 borrows stdin, -1 is memory.
 * name is borrowed for the call. Buffer and filename storage belong to the
 * source object. The parser may update fields until pop or final close.
 * length is initialized storage (0 selects an empty streaming buffer).
 */
struct neocc_source_request {
	const char *name;
	size_t length;
	int fd;
	int *ifdef_stack_ptr;
	int tok_flags;
};

struct neocc_source_result {
	struct neocc_source_buffer *current;
	int tok_flags;
	int lines;
};

#define NEOCC_AC3_SOURCE_PUSH (AC3_ + 0)
#define NEOCC_AC1_SOURCE_POP (AC1_ + 0)
#define NEOCC_AC0_SOURCE_SAVE_NAME (AC0_ + 0)

#endif
