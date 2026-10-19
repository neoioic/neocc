/* Real allocator entry points for the portable fault-injection build.
 * Tested files rename calloc/free at compile time; this file does not.
 */
#include <stdlib.h>
void *__real_calloc(size_t count, size_t size)
{
	return calloc(count, size);
}
void __real_free(void *pointer)
{
	free(pointer);
}
