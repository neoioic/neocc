#include "compiler/tcc.h"
#include "check.h"
#include "objects/source.h"

void *__real_calloc(size_t, size_t);
void *__real_malloc(size_t);
static int fail_at, calls;
static int arena_fail_at, arena_calls;

void *__wrap_malloc(size_t size)
{
	if (size >= 256 * 1024 && ++arena_calls == arena_fail_at)
		return NULL;
	return __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size)
{
	return ++calls == fail_at ? NULL : __real_calloc(count, size);
}

static void quiet_error(void *unused, const char *text)
{
	(void)unused;
	(void)text;
}

int main(int argc, char **argv)
{
	struct TCCState *state;
	unsigned long before, after;
	unsigned int i, j;
	int failure;

	CHECK(argc == 2);
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &before));
	/* Reject either object constructor without leaking the preceding graph. */
	for (failure = 1; failure <= 2; failure++) {
		fail_at = calls + failure;
		CHECK(!tcc_new());
		fail_at = 0;
		state = tcc_new();
		CHECK(state);
		tcc_delete(state);
	}
	for (failure = 1; failure <= 3; failure++) {
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, NULL, quiet_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		/* Fail the primary, command-line or tccdefs.h input buffer. */
		fail_at = calls + failure;
		CHECK(tcc_add_file(state, "tests/source.c") < 0);
		fail_at = 0;
		CHECK(!((struct em_source *)state->sources->members)->top);
		for (i = 0; i < state->sources->node_count; i++) {
			struct object *node = state->sources->nodes + i;
			for (j = 0; j < node->caller_count; j++)
				CHECK(!node->callers[j].cr && !node->callers[j].mr);
		}
		tcc_delete(state);
		/* A fresh compilation after longjmp must not reuse freed globals. */
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state, "int value(void) { return 73; }") == 0);
		tcc_delete(state);
	}
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &after));
	CHECK(before == after);
	/* Fail first the macro stream arena, then the identifier arena. Both
	 * failures occur after compiler cleanup has been armed, inside VO.
	 */
	for (failure = 1; failure <= 2; failure++) {
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, NULL, quiet_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		arena_fail_at = arena_calls + failure;
		CHECK(tcc_compile_string(state, "int token_failure;") < 0);
		arena_fail_at = 0;
		CHECK(!((struct em_source *)state->sources->members)->top);
		for (i = 0; i < state->tokens->node_count; i++) {
			struct object *node = state->tokens->nodes + i;
			for (j = 0; j < node->caller_count; j++)
				CHECK(!node->callers[j].cr && !node->callers[j].mr);
		}
		tcc_delete(state);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state, "int recovered_token;") == 0);
		tcc_delete(state);
	}
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &after));
	CHECK(before == after);
	puts("compiler allocation failures: handles and VO records released before longjmp");
	return 0;
}
