/* TinyCC-derived expression storage, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_EXPRESSIONS_H
#define NEOCC_EXPRESSIONS_H

#include "neocc/preprocessor.h"
#include "neocc/symbols.h"

#define NEOCC_EXPRESSION_CAPACITY 512
#define NEOCC_AC0_EXPRESSIONS_RESET AC0_
#define NEOCC_AC1_EXPRESSIONS_BORROW AC1_

/* value on stack */
struct neocc_expression_value {
	struct neocc_type type;
	unsigned short r; /* register + flags */
	/* Second register for long long; VT_CONST when unused. */
	unsigned short r2;
	union {
		struct { int jtrue, jfalse; }; /* forward jumps */
		union neocc_value c; /* constant, if VT_CONST */
	};
	union {
		struct { unsigned short cmp_op, cmp_r; }; /* VT_CMP */
		/* VT_SYM | VT_CONST, or unary() identifier result. */
		struct neocc_symbol *sym;
	};
};

/* Owned working storage, borrowed through VO while open. Reset clears all
 * symbol references and values, keeps the address stable and restores top to
 * the real sentinel at values[0]. Algorithms use values + 1 as their base.
 * Close invalidates the borrow; reopening requires a new borrow. The extra
 * element preserves TinyCC's empty-stack and preprocessor sentinel semantics.
 * Canonical target types are initialized by the compiler after each reset;
 * symbol records referenced by types remain owned by the symbols object.
 */
struct neocc_expressions {
	struct neocc_expression_value values[1 + NEOCC_EXPRESSION_CAPACITY];
	struct neocc_expression_value *top;
	struct neocc_type int_type, char_type, char_pointer_type, func_old_type;
};

#endif
