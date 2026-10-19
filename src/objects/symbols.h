#ifndef NEOCC_SYMBOLS_OBJECT_H
#define NEOCC_SYMBOLS_OBJECT_H

#include "neocc/symbols.h"

struct neocc_symbol_block;

struct em_symbols {
	struct neocc_symbol_block *blocks;
	struct neocc_symbol *free_first;
	struct neocc_symbol *stacks[NEOCC_SYMBOL_STACK_COUNT];
	int scope;
};

#endif
