/* TinyCC-derived semantic symbol layout, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_SYMBOLS_H
#define NEOCC_SYMBOLS_H

#include "object.h"

#define NEOCC_AC0_SYMBOLS_RESET AC0_
#define NEOCC_AC1_SYMBOLS_ALLOCATE AC1_
#define NEOCC_AC1_SYMBOLS_SCOPE (AC1_ + 1)
#define NEOCC_AC2_SYMBOLS_RELEASE AC2_
#define NEOCC_AC2_SYMBOLS_DROP (AC2_ + 1)
#define NEOCC_AC2_SYMBOLS_SCOPE (AC2_ + 2)
#define NEOCC_AC3_SYMBOLS_PUSH (AC3_ + 1)
#define NEOCC_AC3_SYMBOLS_COPY (AC3_ + 2)
#define NEOCC_AC3_SYMBOLS_TOP (AC3_ + 3)
#define NEOCC_AC3_SYMBOLS_MOVE (AC3_ + 4)

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

/* Stack identities preserve TinyCC's separate semantic, macro and label
 * lookup orders. The object owns every head; callers receive borrowed nodes
 * and may traverse prev links read-only. Token symbol links stay with the
 * compiler and must be removed before dropping nodes or resetting storage.
 */
enum neocc_symbol_stack {
	NEOCC_SYMBOL_STACK_GLOBAL = 0,
	NEOCC_SYMBOL_STACK_LOCAL,
	NEOCC_SYMBOL_STACK_DEFINE,
	NEOCC_SYMBOL_STACK_GLOBAL_LABEL,
	NEOCC_SYMBOL_STACK_LOCAL_LABEL,
	NEOCC_SYMBOL_STACK_COUNT
};

/* In-process pointer messages, copied with memcpy. Allocate returns a
 * writable symbol owned by this object; its contents are not initialized.
 * Push initializes a zeroed record and prepends it to the selected stack.
 * Copy duplicates a live record and prepends the copy. Top returns a
 * borrowed head or NULL for an empty stack. Drop releases the current head
 * and rejects any other pointer. Move detaches a symbol found anywhere in
 * the source stack and prepends it to the destination; copy requests first
 * allocate an identical replacement at the source position and return it.
 * Move returns RO_VAINLY without output when the symbol is not in the source.
 * Scope reports the current level; its change is signed and may not make the
 * level negative. Reset and final close clear all stacks and the pool.
 * Release/drop accept a live symbol from this object exactly once, and all
 * reset/close operations invalidate every previously borrowed pointer.
 */
struct neocc_symbol_push {
	unsigned int stack;
	int v;
	int type;
	int c;
};

struct neocc_symbol_copy {
	unsigned int stack;
	struct neocc_symbol *symbol;
};

struct neocc_symbol_stack_query {
	unsigned int stack;
};

struct neocc_symbol_stack_result {
	struct neocc_symbol *top;
};

struct neocc_symbol_drop {
	unsigned int stack;
	struct neocc_symbol *symbol;
};

struct neocc_symbol_move {
	unsigned int source;
	unsigned int destination;
	struct neocc_symbol *symbol;
	int copy;
};

struct neocc_symbol_move_result {
	struct neocc_symbol *replacement;
};

struct neocc_symbol_scope {
	int change;
};

#endif
