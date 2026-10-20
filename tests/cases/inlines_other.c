#include "inlines_shared.h"
static inline int leaf(int n) { return n + 7; }
int other_inline(void) { return local_bias(10) + leaf(2); }
