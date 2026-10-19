#ifndef NEOCC_TOKENS_OBJECT_H
#define NEOCC_TOKENS_OBJECT_H

#include "neocc/tokens.h"

#define NEOCC_TOKEN_HASH_SIZE 16384

struct neocc_token_block;

struct em_tokens {
	struct neocc_token *hash[NEOCC_TOKEN_HASH_SIZE];
	struct neocc_token **table;
	size_t count;
	size_t capacity;
	struct neocc_token_block *symbols;
	struct neocc_token_block *streams;
};

#endif
