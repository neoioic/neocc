#include <stdio.h>
#include "inlines_shared.h"

static inline int leaf(int n) { return n * 3; }
static inline int branch(int n) { return leaf(n) + 2; }
static inline int odd(int n);
static inline int even(int n) { return n ? odd(n - 1) : 1; }
static inline int odd(int n) { return n ? even(n - 1) : 0; }
static inline int factorial(int n) { return n ? n * factorial(n - 1) : 1; }
extern int unused_external(void);
static inline int unused(void) { return unused_external(); }
/* A later non-inline declaration forces emission even without a call. */
static inline int forced(void) { return 19; }
int forced(void);
int other_inline(void);

int main(void)
{
	int a = local_bias(10), b = local_bias(10);

	printf("%d %d %d %d %d %d\n", branch(4), even(8), factorial(5),
		a, b, other_inline());
	return 0;
}
