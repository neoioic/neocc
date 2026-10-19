#define CAT_(a, b) a ## b
#define CAT(a, b) CAT_(a, b)
#define STRING_(a) #a
#define STRING(a) STRING_(a)
#define VALUE 37
#include "nested/outer.h"
#if VALUE != 37 || INCLUDED != 73
#error preprocessor state mismatch
#endif
const char *CAT(pre, processed) = STRING(VALUE);
int source_line = __LINE__;
