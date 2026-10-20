#include "compiler/tcc.h"
#include "check.h"
#include "objects/temporaries.h"
#include "objects/source.h"
#include "objects/symbols.h"
#include "objects/scopes.h"
#include "objects/functions.h"
#include "objects/switches.h"

void *__real_calloc(size_t, size_t);
void *__real_malloc(size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
static int fail_at, calls;
static int temporary_fail_at, temporary_calls, temporary_resize_fail;
static struct TCCState *temporary_state;
static char temporary_diagnostic[512];
static int arena_fail_at, arena_calls;
static int symbol_fail_at, symbol_calls, symbol_error;
static int expression_error;
static int scope_fail_at, scope_calls, scope_error;
static int resize_fail_at, resize_calls;
static size_t live;
static int switch_fail_at, switch_calls, switch_resize_fail, switch_error;
static struct TCCState *switch_state;
static struct TCCState *initializer_state;
static int initializer_resize_fail, initializer_resize_seen;


void *__wrap_malloc(size_t size)
{
	void *pointer;

	if (temporary_state && size == sizeof(struct neocc_temporary) &&
	    ++temporary_calls == temporary_fail_at)
		return NULL;
	if (switch_state &&
	    (size == sizeof(struct neocc_switch) || size == sizeof(struct neocc_case)) &&
	    ++switch_calls == switch_fail_at)
		return NULL;
	if (size == sizeof(struct neocc_scope_frame) &&
	    ++scope_calls == scope_fail_at)
		return NULL;
	if (size >= 256 * 1024 && ++arena_calls == arena_fail_at)
		return NULL;
	if (size == sizeof(void *) +
	    (8192 / sizeof(struct neocc_symbol)) * sizeof(struct neocc_symbol) &&
	    ++symbol_calls == symbol_fail_at)
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

	if (temporary_state && temporary_resize_fail) {
		struct em_temporaries *members = temporary_state->temporaries->members;

		struct neocc_temporary *entry = members->top;
		int depth = 0;

		for (; entry && !entry->boundary; entry = entry->prev)
			depth++;
		if (members->top && depth == (temporary_resize_fail == 1 ? 3 : 2) &&
		    old == members->top->string.data) {
			temporary_resize_fail = 0;
			return NULL;
		}
	}

	if (initializer_state && initializer_resize_fail && old &&
	    old == initializer_state->initializer->string.data) {
		CHECK(initializer_state->initializer->string.size_allocated >= 8);
		initializer_resize_fail = 0;
		initializer_resize_seen++;
		return NULL;
	}
	if (switch_state && switch_resize_fail &&
	    switch_state->switch_state->current &&
	    old == switch_state->switch_state->current->p) {
		switch_resize_fail = 0;
		return NULL;
	}
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
	CHECK(!state->unit->anonymous_symbol && !state->unit->instruction);
	CHECK(!state->unit->suppression && !state->unit->debug_modes &&
	      !state->unit->section);
	for (unsigned int i = 0; i < state->translation->node_count; i++) {
		struct object *node = state->translation->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}

	CHECK(!((struct em_temporaries *)state->temporaries->members)->top);
	for (unsigned int i = 0; i < state->temporaries->node_count; i++) {
		struct object *node = state->temporaries->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}

	struct em_scopes *scopes = state->scopes->members;
	struct em_functions *functions = state->functions->members;

	CHECK(!state->initializer->global_expression);
	CHECK(!state->initializer->string.data &&
	      !state->initializer->string.size &&
	      !state->initializer->string.size_allocated);
	for (unsigned int i = 0; i < state->initializers->node_count; i++) {
		struct object *node = state->initializers->nodes + i;
		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
	CHECK(!state->switch_state->current);
	for (unsigned int i = 0; i < state->switches->node_count; i++) {
		struct object *node = state->switches->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}

	CHECK(!state->function->type.ref && !state->function->type.t);
	CHECK(!*state->function->name && state->function->start == -1);
	CHECK(!state->function->variadic && !state->function->old_style);
	CHECK(!state->function->stack_offset && !state->function->struct_return);
	CHECK(!state->function->return_jumps && !functions->temp_count);
	for (int i = 0; i < NEOCC_FUNCTION_TEMP_CAPACITY; i++)
		CHECK(!functions->temps[i].location && !functions->temps[i].size &&
		      !functions->temps[i].alignment);
	for (unsigned int i = 0; i < state->functions->node_count; i++) {
		struct object *node = state->functions->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
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
	if (strstr(text, "cannot allocate switch"))
		switch_error = 1;
	if (strstr(text, "semantic symbol"))
		symbol_error = 1;
	if (strstr(text, "memory full (vstack)"))
		expression_error = 1;
	if (strstr(text, "cannot allocate scope frame"))
		scope_error = 1;
}

static void temporary_error(void *opaque, const char *text)
{
	struct TCCState *state = opaque;

	snprintf(temporary_diagnostic, sizeof(temporary_diagnostic), "%s", text);
	/* Error handling can run only after the failing VO access returns. */
	for (unsigned int i = 0; i < state->temporaries->node_count; i++) {
		struct object *node = state->temporaries->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

struct translation_failure {
	struct TCCState *state;
	Section *section;
	int mode;
	int seen;
};

static void translation_error(void *opaque, const char *text)
{
	struct translation_failure *failure = opaque;
	struct TCCState *state = failure->state;
	struct neocc_translation *unit = state->unit;

	CHECK(strstr(text, failure->mode == 3 ? "bad preprocessor expression" :
		failure->mode == 4 ? "unknown opcode" : "missing_translation"));
	CHECK(unit->anonymous_symbol >= SYM_FIRST_ANOM);
	if (failure->mode == 3) {
		CHECK(!unit->section && !unit->instruction && !unit->debug_modes);
	} else {
		CHECK(unit->section && unit->instruction > 0);
		if (failure->mode < 3) {
			CHECK(unit->suppression & 0xffff); /* sizeof operand */
			CHECK(unit->debug_modes == (failure->mode != 0));
		}
		failure->section = unit->section;
	}
	for (unsigned int i = 0; i < state->translation->node_count; i++) {
		struct object *node = state->translation->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
	failure->seen++;
}

struct function_failure {
	struct TCCState *state;
	int seen;
};

static void function_error(void *opaque, const char *text)
{
	struct function_failure *failure = opaque;
	struct neocc_function *function = failure->state->function;
	struct em_functions *members = failure->state->functions->members;

	if (!strstr(text, "missing_function_value"))
		return;
	CHECK(!strcmp(function->name, "broken_function"));
	CHECK(function->type.ref && function->variadic && function->start >= 0);
	CHECK(function->stack_offset < 0 && function->struct_return);
	CHECK(function->return_jumps);
	CHECK(members->temp_count);
	failure->seen++;
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
	/* Boundary failure, nested asm registration, diagnostic registration and
	 * diagnostic-buffer OOM must unwind before recovery, without dead locals.
	 */
	for (failure = 0; failure <= 6; failure++) {
		struct neocc_string *outer;
		unsigned int quantity;
		const char *input =
			"int broken(void) { __asm__(\"\" : : \"r\" (({"
			"__asm__(\"\" : : \"r\" (missing_temporary)); 7; }))); return 0; }";

		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, state, temporary_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		/* Simulate generated source held by the driver across compilation. */
		CHECK(RP(vo_interact1(state->temporaries, NEOCC_AC1_TEMPORARIES_PUSH,
			(unsigned char *)&outer, sizeof(outer), &quantity,
			&neocc_host_caller)) && quantity == sizeof(outer));
		CHECK(neocc_string_printf(outer, "%s", input) > 0);
		temporary_state = state;
		temporary_calls = 0;
		temporary_fail_at = failure <= 4 ? failure : 0;
		temporary_resize_fail = failure >= 5 ? failure - 4 : 0;
		*temporary_diagnostic = 0;
		CHECK(tcc_compile_string(state, outer->data) < 0);
		if (failure >= 1 && failure <= 4)
			CHECK(temporary_calls >= failure);
		if (failure == 4 || failure == 5)
			CHECK(strstr(temporary_diagnostic, "cannot format diagnostic"));
		else if (!failure)
			CHECK(strstr(temporary_diagnostic, "missing_temporary"));
		else if (failure == 6)
			CHECK(strstr(temporary_diagnostic, "cannot grow compiler string"));
		else
			CHECK(strstr(temporary_diagnostic, "temporary"));
		CHECK(!temporary_resize_fail);
		temporary_state = NULL;
		CHECK(!strcmp(outer->data, input));
		CHECK(RP(vo_interact2(state->temporaries, NEOCC_AC2_TEMPORARIES_POP,
			(unsigned char *)&outer, sizeof(outer), &neocc_host_caller)));
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state, "int recovered_temporary;") == 0);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
	}
	/* Observe live generation state, then verify diagnostic cleanup without
	 * releasing the borrowed linker section. Cover C, preprocessing and asm.
	 */
	for (int mode = 0; mode < 5; mode++) {
		struct translation_failure context = { .mode = mode };
		FILE *output = NULL;

		state = tcc_new();
		CHECK(state);
		context.state = state;
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, &context, translation_error);
		if (mode == 1 || mode == 2)
			tcc_set_options(state, mode == 1 ? "-g" : "-gdwarf-5");
		if (mode == 4)
			tcc_set_options(state, "-x assembler");
		CHECK(tcc_set_output_type(state, mode == 3 ?
			TCC_OUTPUT_PREPROCESS : TCC_OUTPUT_OBJ) == 0);
		if (mode == 3) {
			output = tmpfile();
			CHECK(output);
			state->ppfp = output;
		}
		if (mode == 3)
			CHECK(tcc_compile_string(state, "#if 1 +\n#endif\n") < 0);
		else if (mode == 4)
			CHECK(tcc_compile_string(state,
				".text\n.byte 1\nunknown_translation_opcode\n") < 0);
		else
			CHECK(tcc_compile_string(state,
				"int broken(int n) __attribute__((section(\".text.unit\")));"
				"int broken(int n) { if (n) goto done; "
				"return sizeof(missing_translation); done: return 1; }") < 0);
		CHECK(context.seen == 1);
		clean_preprocessor(state);
		if (context.section) {
			CHECK(state->sections[context.section->sh_num] == context.section);
			CHECK(context.section->data && context.section->data_allocated);
		}
		if (output)
			CHECK(fclose(output) == 0);
		tcc_delete(state);
		CHECK(live == initial_live);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state,
			"int recovered_translation(void) { return sizeof(int); }") == 0);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
	}
	/* Reject eleven constructors and both aligned workspaces. */
	for (failure = 1; failure <= 13; failure++) {
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
	/* Fail frames, case records and the first index growth inside VO. */
	for (int n = 1; n <= 7; n++) {
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, NULL, quiet_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		switch_state = state;
		switch_calls = switch_error = 0;
		switch_fail_at = n <= 6 ? n : 0;
		switch_resize_fail = n == 7;
		CHECK(tcc_compile_string(state,
			"int broken_switch(int n) { switch (n) { "
			"case 1: case 2: switch (n + 1) { "
			"case 3: case 4: return 1; } } return 0; }") < 0);
		switch_state = NULL;
		CHECK(switch_error);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state,
			"int recovered_switch(int n) { switch(n) { "
			"case 1: case 2: return 3; default: return 0; } }") == 0);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
	}
	/* Diagnostics at different parser stages must release the active chain. */
	{
		const char *inputs[] = {
			"int f(int n) { switch(n) { case 1: switch(n) { case 2: return absent; } } }",
			"int f(int n) { switch(n) { case 1: case 1: return 0; } }",
			"int f(int n) { switch(n) { default: default: return 0; } }",
			"int f(void) { switch(1.5) {} }",
			"int f(int n) { switch(n) { case absent: return 0; } }"
		};

		for (unsigned int i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
			state = tcc_new();
			CHECK(state);
			tcc_set_lib_path(state, argv[1]);
			tcc_set_error_func(state, NULL, quiet_error);
			CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
			CHECK(tcc_compile_string(state, inputs[i]) < 0);
			clean_preprocessor(state);
			tcc_delete(state);
			CHECK(live == initial_live);
		}
	}
	/* Both string consumers grow an already populated initializer buffer.
	 * Failure must retain its allocation until compiler reset, then release
	 * it exactly once. Follow each failure with a fresh successful session.
	 */
	for (int consumer = 0; consumer < 2; consumer++) {
		const char *input = consumer ?
			"int first __attribute__((section(\".data\"))); "
			"int second __attribute__((section(\".long_section_name\")));" :
			"char first[]=\"seed\"; "
			"char second[]=\"a much longer string initializer\";";

		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		tcc_set_error_func(state, NULL, quiet_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		initializer_state = state;
		initializer_resize_fail = 1;
		initializer_resize_seen = 0;
		CHECK(tcc_compile_string(state, input) < 0);
		CHECK(initializer_resize_seen == 1 && !initializer_resize_fail);
		initializer_state = NULL;
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state, input) == 0);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
	}
	/* Replay dry-runs and nested static compound literals can diagnose with
	 * a populated shared string, global mode and macro stack still active.
	 */
	{
		const char *inputs[] = {
			"struct a { char text[8]; int n; }; "
			"struct a array[] = {{\"a\" \"b\", missing_initializer}};",
			"struct a { char text[8]; int n; }; "
			"struct a *p = &(struct a){\"string\", missing_initializer};",
			"int f(void) { char s[]=\"local\"; "
			"int a[][2]={{1,2},{3,missing_initializer}}; return a[0][0]; }"
		};

		for (unsigned int n = 0; n < sizeof(inputs) / sizeof(inputs[0]); n++) {
			state = tcc_new();
			CHECK(state);
			tcc_set_lib_path(state, argv[1]);
			tcc_set_error_func(state, NULL, quiet_error);
			CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
			CHECK(tcc_compile_string(state, inputs[n]) < 0);
			clean_preprocessor(state);
			tcc_delete(state);
			CHECK(live == initial_live);
		}
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state,
			"char recovered[]=\"ok\"; int *p=(int[]){1,2};") == 0);
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
	/* Observe a populated function workspace/cache before the error jump,
	 * then verify normal cleanup removes every borrowed name/type and slot.
	 */
	for (int debug = 0; debug < 2; debug++) {
		struct function_failure failure = { 0 };

		state = tcc_new();
		CHECK(state);
		failure.state = state;
		tcc_set_lib_path(state, argv[1]);
		if (debug)
			tcc_set_options(state, "-gdwarf-5");
		tcc_set_error_func(state, &failure, function_error);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state,
			"struct big { long long a, b, c; }; "
			"extern struct big create(int); "
			"extern struct big join(struct big, struct big); "
			"extern int number(void); "
			"struct big broken_function(int n, ...) { int array[n]; "
			"array[0] = n + number(); "
			"if (n) return join(create(1), create(2)); "
			"return missing_function_value; }") < 0);
		CHECK(failure.seen == 1);
		clean_preprocessor(state);
		tcc_delete(state);
		CHECK(live == initial_live);
		state = tcc_new();
		CHECK(state);
		tcc_set_lib_path(state, argv[1]);
		CHECK(tcc_set_output_type(state, TCC_OUTPUT_OBJ) == 0);
		CHECK(tcc_compile_string(state,
			"int recovered_function(int n) { return n + 1; }") == 0);
		clean_preprocessor(state);
		tcc_delete(state);
	}
	CHECK(live == initial_live);
	puts("compiler allocation failures: handles and VO records released before longjmp");
	return 0;
}
