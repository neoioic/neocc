#include "nested/cache_guard.h"
#pragma comment(lib, "neocc_failure_library")
#ifdef NEOCC_PP_DIAGNOSTIC
#error preprocessing resource diagnostic
#endif
int preprocessor_result_value = CACHE_GUARD_SEEN;
