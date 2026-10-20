#ifndef INLINE_SHARED_H
#define INLINE_SHARED_H
static inline int local_bias(int n)
{
	static int calls;
	return n + ++calls;
}
#endif
