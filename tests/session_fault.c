#include "compiler/tcc.h"
#include "check.h"
#include "objects/source.h"
#include "objects/symbols.h"
#include "objects/scopes.h"

void *__real_calloc(size_t, size_t);
void *__real_malloc(size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
static int fail_at, calls;
static int arena_fail_at, arena_calls;
static int symbol_fail_at, symbol_calls, symbol_error;
static int expression_error;
static int scope_fail_at, scope_calls, scope_error;
static int resize_fail_at, resize_calls;
static size_t live;

void *__wrap_malloc(size_t size)
{
	void *pointer;

	if (size == sizeof(struct neocc_scope_frame) &&
	    ++scope_calls == scope_fail_at)
		return NULL;
	if (size >= 256 * 1024 && ++arena_calls == arena_fail_at)
		return NULL;
	if (size < 256 * 1024 && ++symbol_calls == symbol_fail_at)
		return NULL;
	pointer = __real_malloc(size);
	if (pointer)
		live++;
	return pointer;
}

void *__wrap_calloc(size_t count, size_t size)
{
	void *pointer = ++calls == fail_at ? NULL : __real_calloc(count, size);

	if (pointer)
		live++;
	return pointer;
}

void *__wrap_realloc(void *old, size_t size)
{
	void *pointer;

	if (++resize_calls == resize_fail_at)
		return NULL;
	pointer = __real_realloc(old, size);
	if (pointer && !old)
		live++;
	return pointer;
}

void __wrap_free(void *pointer)
{
	if (pointer) {
		CHECK(live);
		live--;
	}
	__real_free(pointer);
}

/* Keep legacy fatal allocator calls out of the recoverable-object injection.
 * The header also detects cross-allocator frees. Both allocator families
 * contribute to the leak count.
 */
union compiler_header {
	max_align_t alignment;
	unsigned int marker;
};

static void *compiler_reallocator(void *old, unsigned long size)
{
	union compiler_header *header = old ?
		(union compiler_header *)old - 1 : NULL;
	union compiler_header *pointer;

	if (header)
		CHECK(header->marker == 0x4e454f43);
	if (!size) {
		__wrap_free(header);
		return NULL;
	}
	CHECK(size <= SIZE_MAX - sizeof(*header));
	pointer = __real_realloc(header, sizeof(*header) + size);
	CHECK(pointer);
	pointer->marker = 0x4e454f43;
	if (!old)
		live++;
	return pointer + 1;
}

static void clean_preprocessor(struct TCCState *state)
{
	struct em_scopes *scopes = state->scopes->members;

	CHECK(!scopes->active && !scopes->available);
	CHECK(!state->scope->current && !state->scope->loop && !state->scope->root);
	CHECK(!state->scope->all_cleanups && !state->scope->pending_gotos);
	for (unsigned int i = 0; i < state->scopes->node_count; i++) {
		struct object *node = state->scopes->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
	CHECK(state->expr->top == state->expr->values);
	CHECK(!state->expr->int_type.ref && !state->expr->func_old_type.ref);
	CHECK(!state->expr->char_type.ref && !state->expr->char_pointer_type.ref);
	for (unsigned int i = 0; i <= NEOCC_EXPRESSION_CAPACITY; i++)
		CHECK(!state->expr->values[i].sym && !state->expr->values[i].c.i);
	for (unsigned int i = 0; i < state->expressions->node_count; i++) {
		struct object *node = state->expressions->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
	CHECK(!((struct em_symbols *)state->symbols->members)->blocks);
	CHECK(!((struct em_symbols *)state->symbols->members)->free_first);
	CHECK(!((struct em_symbols *)state->symbols->members)->scope);
	for (unsigned int i = 0; i < NEOCC_SYMBOL_STACK_COUNT; i++)
		CHECK(!((struct em_symbols *)state->symbols->members)->stacks[i]);
	for (unsigned int i = 0; i < state->symbols->node_count; i++) {
		struct object *node = state->symbols->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
	CHECK(!state->pp->macro_stack && !state->pp->macro_ptr);
	CHECK(!state->pp->source && !state->pp->token && !state->pp->value.i);
	CHECK(!state->pp->debug_indent);
	CHECK(!state->pp->parsed.data && !state->pp->spelling.data);
	CHECK(!state->pp->predefs.data);
	CHECK(!state->pp->expansion.str && !state->pp->unget.str);
	for (unsigned int i = 0; i < state->preprocessor->node_count; i++) {
		struct object *node = state->preprocessor->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static void quiet_error(void *unused, const char *text)
{
	(void)unused;
	if (strstr(text, "semantic symbol"))
		symbol_error = 1;
	if (strstr(text, "memory full (vstack)"))
		expression_error = 1;
	if (strstr(text, "cannot allocate scope frame"))
		scope_error = 1;
}

int main(int argc, char **argv)
{
	struct TCCState *state;
	unsigned long before, after;
	unsigned int i, j;
	int failure;
	size_t initial_live = live;

	CHECK(argc == 2);
	tcc_set_realloc(compiler_reallocator);
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &before));
	/* Options and path lists own copies in the compiler allocator domain. */
	state = tcc_new();
	CHECK(state);
	tcc_set_options(state, "-DOPTION_VALUE=5 -I\"tests/cases\"");
	tcc_set_lib_path(state, argv[1]);
	CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
	CHECK(tcc_compile_string(state, "int option_value = OPTION_VALUE;") == 0);
	tcc_delete(state);
	CHECK(live == initial_live);
	/* Reject six constructors and both aligned workspaces. */
	for (failure = 1; failure <= 8; failure++) {
		fail_at = calls + failure;
		CHECK(!tcc_new());
		fail_at = 0;
		state = tcc_new();
		CHECK(state);
		tcc_delete(state);
		CHECK(live == initial_live);
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
		clean_preprocessor(state);
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
		clean_preprocessor(state);
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
	/* The first and later semantic pool blocks fail inside VO. */
	for (failure = 1; failure <= 3; failure++) {
		char input[16000];
		size_t length = 0;

		for (int n = 0; n < 400; n++)
			length += snprintf(input + length, sizeof(input) - length,
				"int symbol_%d;\n", n);
		CHECK(length < sizeof(input));
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, NULL, quiet_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		symbol_error = 0;
		symbol_fail_at = symbol_calls + failure;
		CHECK(tcc_compile_string(state, input) < 0);
		symbol_fail_at = 0;
		CHECK(symbol_error);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state, "int recovered_symbol;") == 0);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
	}
	/* Fail successive scratch-string and token-table reallocations. */
	for (failure = 1; failure <= 8; failure++) {
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, NULL, quiet_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		resize_fail_at = resize_calls + failure;
		CHECK(tcc_compile_string(state,
			"#define TEXT(x) #x\nchar *text = TEXT(hello world);\n") < 0);
		resize_fail_at = 0;
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
	}
	/* Fail root/nested frames, including after cleanup and pending goto
	 * records have been created. VO must unwind before the diagnostic jump.
	 */
	for (failure = 1; failure <= 5; failure++) {
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, NULL, quiet_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		scope_error = 0;
		scope_fail_at = scope_calls + failure;
		CHECK(tcc_compile_string(state,
			"extern void release(int *); int scoped(int n) { "
			"int a __attribute__((cleanup(release))) = 1; { "
			"int b __attribute__((cleanup(release))) = 2; "
			"goto done; { { int c = n; } } } done: return n; }") < 0);
		scope_fail_at = 0;
		CHECK(scope_error);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state, "int recovered_scope(void) { return 3; }") == 0);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
	}
	/* Leave a populated expression stack through the diagnostic jump. */
	{
		char input[NEOCC_EXPRESSION_CAPACITY * 5 + 128];
		int length = snprintf(input, sizeof(input),
			"int deep(void) { return ");

		for (int n = 0; n < NEOCC_EXPRESSION_CAPACITY; n++) {
			memcpy(input + length, "1+(", 3);
			length += 3;
		}
		input[length++] = '1';
		for (int n = 0; n < NEOCC_EXPRESSION_CAPACITY; n++)
			input[length++] = ')';
		memcpy(input + length, "; }", 4);
		expression_error = 0;
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, NULL, quiet_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state, input) < 0);
		CHECK(expression_error);
		clean_preprocessor(state);
		tcc_delete(state);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state, "int after_depth = 19;") == 0);
		clean_preprocessor(state);
		tcc_delete(state);
	}
	CHECK(live == initial_live);
	puts("compiler allocation failures: handles and VO records released before longjmp");
	return 0;
}
