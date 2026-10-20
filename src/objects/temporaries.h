/* Temporary strings migrated from TinyCC libtcc.c, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_TEMPORARIES_OBJECT_H
#define NEOCC_TEMPORARIES_OBJECT_H

#include "neocc/temporaries.h"

struct neocc_temporary {
	struct neocc_string string;
	struct neocc_temporary *prev;
	int boundary;
};

struct em_temporaries {
	struct neocc_temporary *top;
};

#endif
