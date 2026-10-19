/* TinyCC-derived semantic symbol layout, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_SYMBOLS_H
#define NEOCC_SYMBOLS_H

#include "object.h"

#define NEOCC_AC0_SYMBOLS_RESET AC0_
#define NEOCC_AC1_SYMBOLS_ALLOCATE AC1_
#define NEOCC_AC2_SYMBOLS_RELEASE AC2_

/* type definition */
struct neocc_type {
	int t;
	struct neocc_symbol *ref;
};

/* symbol attributes */
struct neocc_symbol_attributes {
	unsigned short
	aligned     : 5, /* alignment as log2+1 (0 == unspecified) */
	packed      : 1,
	weak        : 1,
	visibility  : 2,
	dllexport   : 1,
	nodecorate  : 1,
	dllimport   : 1,
	addrtaken   : 1,
	nodebug     : 1,
	xxxx        : 2; /* not used */
};

/* function attributes or temporary attributes for parsing */
struct neocc_function_attributes {
	unsigned
	func_call   : 3, /* calling convention, compiler FUNC_* constants */
	func_type   : 2, /* FUNC_OLD/NEW/ELLIPSIS */
	func_noreturn : 1, /* attribute((noreturn)) */
	func_ctor   : 1, /* attribute((constructor)) */
	func_dtor   : 1, /* attribute((destructor)) */
	func_args   : 8, /* PE __stdcall args */
	func_alwinl : 1, /* always_inline */
	xxxx        : 15;
};

/* symbol management */
struct neocc_symbol {
	int v; /* symbol token */
	unsigned short r; /* associated register or VT_CONST/VT_LOCAL and LVAL type */
	struct neocc_symbol_attributes a; /* symbol attributes */
	union {
		struct {
			int c; /* associated number or Elf symbol index */
			union {
				int sym_scope; /* scope level for locals */
				int jnext; /* next jump label */
				int jind; /* label position */
				struct neocc_function_attributes f; /* function attributes */
				int auxtype; /* bitfield access type */
			};
		};
		long long enum_val; /* enum constant if IS_ENUM_VAL */
		int *d; /* define token stream */
		struct neocc_symbol *cleanup_func;
	};

	struct neocc_type type; /* associated type */
	union {
		struct neocc_symbol *next; /* next related symbol (for fields and anoms) */
		int *e; /* expanded token stream with preprocessor macros */
		int asm_label; /* associated asm label */
		struct neocc_symbol *cleanupstate; /* in defined labels */
		int *vla_array_str; /* vla array code */
	};
	struct neocc_symbol *prev; /* prev symbol in stack */
	union {
		struct neocc_symbol *prev_tok; /* previous symbol for this token */
		struct neocc_symbol *cleanup_sym; /* symbol from __attribute__((cleanup())) */
		struct neocc_symbol *cleanup_label; /* label in 'pending_gotos' chain */
	};
};

/* In-process pointer messages, copied with memcpy. Allocate returns a
 * writable symbol owned by this object; its contents are not initialized.
 * Release accepts a live symbol from this same object, exactly once.
 * Reset/final close invalidate all pointers. Clear semantic and macro
 * stacks and token symbol links before resetting this storage.
 */

#endif
