#include "nested/cache_once.h"
#include "nested/./cache_once.h"
#include "nested/cache_guard.h"
#include "nested/cache_guard.h"
#undef CACHE_GUARD
#undef CACHE_GUARD_SEEN
#include "nested/cache_guard.h"
#if CACHE_ONCE_SEEN != 7 || CACHE_GUARD_SEEN != 11
#error missing header in this translation unit
#endif
#pragma comment(lib, "kernel32")
int resource_other(void)
{
	return CACHE_GUARD_SEEN;
}
