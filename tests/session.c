#include "compiler/tcc.h"
#include "check.h"
#include "objects/temporaries.h"
#include "objects/tokens.h"
#include "objects/source.h"
#include "objects/symbols.h"
#include "objects/scopes.h"
#include "objects/functions.h"
#include "objects/switches.h"
#include "objects/preprocessor.h"

static char diagnostic[4096];

static void record_error(void *unused, const char *text)
{
	(void)unused;
	snprintf(diagnostic, sizeof(diagnostic), "%s", text);
}

static void clean_access(struct object *object)
{
	unsigned int i, j;

	CHECK(!((struct em_source *)object->members)->top);
	for (i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;
		for (j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static void clean_semantics(struct TCCState *state)
{
	struct em_tokens *tokens = state->tokens->members;

	CHECK(state->pp->include_stack_ptr == state->pp->include_stack);
	CHECK(state->pp->ifdef_stack_ptr == state->pp->ifdef_stack);
	CHECK(state->pp->pack_stack_ptr == state->pp->pack_stack);
	CHECK(!*state->pp->pack_stack_ptr);
	for (int i = 0; i < NEOCC_INCLUDE_STACK_CAPACITY; i++)
		CHECK(!state->pp->include_stack[i]);

	for (int i = 0; i < NEOCC_INCLUDE_HASH_CAPACITY; i++)
		CHECK(!((struct em_preprocessor *)state->preprocessor->members)->includes[i]);
	CHECK(!tokens->inline_head && !tokens->inline_tail);
	struct em_symbols *symbols = state->symbols->members;
	struct em_scopes *scopes = state->scopes->members;
	struct em_functions *functions = state->functions->members;

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

	CHECK(!symbols->blocks && !symbols->free_first && !symbols->scope);
	for (int i = 0; i < NEOCC_SYMBOL_STACK_COUNT; i++)
		CHECK(!symbols->stacks[i]);
	CHECK(!scopes->active && !scopes->available);
	CHECK(!state->scope->current && !state->scope->root && !state->scope->loop);
	CHECK(!state->scope->all_cleanups && !state->scope->pending_gotos);
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
}

static struct TCCState *session(const char *runtime)
{
	struct TCCState *state = tcc_new();

	CHECK(state);
	tcc_set_lib_path(state, runtime);
	tcc_set_error_func(state, NULL, record_error);
	CHECK(tcc_set_output_type(state, TCC_OUTPUT_MEMORY) == 0);
	return state;
}

static void control_errors(const char *runtime)
{
	static const char *const inputs[] = {
		"#if 1\n#pragma pack(push, 1)\n#include \"absent_control.h\"\n",
		"#if 1\n#pragma pack(push, 2)\n",
		"#pragma pack(pop)\n",
		"#if 1\n#else\n#else\n#endif\n",
		"#endif\n",
		"#include \"tests/cases/nested/control_recursive.h\"\n",
	};
	static const char *const messages[] = {
		"absent_control.h", "missing #endif", "out of pack stack",
		"#else after #else", "#endif without matching #if",
		"#include recursion too deep",
	};
	struct TCCState *state;
	char input[2048];

	for (unsigned int i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		state = session(runtime);
		CHECK(tcc_compile_string(state, inputs[i]) < 0);
		CHECK(strstr(diagnostic, messages[i]));
		clean_semantics(state);
		clean_access(state->sources);
		tcc_delete(state);
	}
	for (int pack = 0; pack < 2; pack++) {
		int depth = pack ? NEOCC_PACK_STACK_CAPACITY :
			NEOCC_IFDEF_STACK_CAPACITY + 1;

		input[0] = 0;
		for (int i = 0; i < depth; i++)
			strcat(input, pack ? "#pragma pack(push, 1)\n" : "#if 1\n");
		state = session(runtime);
		CHECK(tcc_compile_string(state, input) < 0);
		CHECK(strstr(diagnostic, pack ? "out of pack stack" :
			"memory full (ifdef)"));
		clean_semantics(state);
		clean_access(state->sources);
		tcc_delete(state);
	}
}

int main(int argc, char **argv)
{
	struct TCCState *a, *b;
	int (*function)(void);
	unsigned long handles_before, handles_after;
	int i;

	CHECK(argc == 2);
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &handles_before));
	control_errors(argv[1]);
	for (i = 0; i < 64; i++) {
		a = session(argv[1]);
		b = session(argv[1]);
		CHECK(a->sources != b->sources && a->sources->oid != b->sources->oid);
		CHECK(a->pp != b->pp && a->preprocessor != b->preprocessor);
		CHECK(a->expr != b->expr && a->expressions != b->expressions);
		CHECK(a->temporaries != b->temporaries);
		CHECK(a->translation != b->translation && a->unit != b->unit);
		CHECK(a->scope != b->scope && a->scopes != b->scopes);
		CHECK(a->initializer != b->initializer &&
		      a->initializers != b->initializers);
		CHECK(a->function != b->function && a->functions != b->functions);
		CHECK(a->switch_state != b->switch_state && a->switches != b->switches);
		CHECK(tcc_compile_string(a,
			"#pragma pack(push, 1)\n"
			"struct packed { char a; int b; };\n"
			"_Static_assert(sizeof(struct packed) == 5, \"pack\");\n") == 0);
		clean_semantics(a);
		CHECK(tcc_compile_string(a,
			"struct natural { char a; int b; };\n"
			"_Static_assert(sizeof(struct natural) == 8, \"reset\");\n") == 0);
		CHECK(tcc_compile_string(a,
			"#pragma comment(lib, \"kernel32\")\n") == 0);
		CHECK(tcc_compile_string(a,
			"#pragma comment(lib, \"kernel32\")\n") == 0);
		CHECK(neocc_preprocessor_results(a)->libraries.count == 2);
		CHECK(!neocc_preprocessor_results(b)->libraries.count);
		tcc_define_symbol(a, "SESSION_VALUE", "17");
		tcc_define_symbol(b, "SESSION_VALUE", "29");
		CHECK(tcc_compile_string(a,
			"static inline int bias(void) { return SESSION_VALUE; } "
			"int value(void) { return bias() + __COUNTER__; }") == 0);
		CHECK(tcc_compile_string(b,
			"static inline int bias(void) { return SESSION_VALUE; } "
			"int value(void) { return bias() + __COUNTER__; }") == 0);
		CHECK(tcc_compile_string(a,
			"#ifdef PRIVATE_MACRO\n#error leaked macro\n#endif\n"
			"#define CAT_(a,b) a##b\n#define CAT(a,b) CAT_(a,b)\n"
			"#define STR_(x) #x\n#define STR(x) STR_(x)\n"
			"#define VALUE(x) ((x) + __COUNTER__)\n"
			"static inline int bias(void) { return 31; } "
			"int second(void) { const char *s = STR(CAT(he,llo)); "
			"return VALUE(bias()) + (s[0] != 'h'); }") == 0);
		CHECK(tcc_compile_string(b,
			"#define PRIVATE_MACRO 1\n"
			"static inline int bias(void) { return 43; } "
			"int second(void) { return bias() + __COUNTER__; }") == 0);
		CHECK(tcc_compile_string(a,
			"int number(void) { volatile long double x = "
			"0x1.4p0L; "
			"return (x - 1.0L) * 4.0L == 1.0L; }") == 0);
		CHECK(tcc_compile_string(b,
			"int number(void) { volatile double x = 2.5; "
			"return x * 2.0 == 5.0; }") == 0);
		CHECK(!a->pp->source && !a->pp->token && !a->pp->value.i);
		CHECK(!b->pp->source && !b->pp->token && !b->pp->value.i);
		CHECK(a->expr->top == a->expr->values);
		CHECK(b->expr->top == b->expr->values);
		CHECK(!a->expr->char_pointer_type.ref && !b->expr->func_old_type.ref);
		clean_access(a->sources);
		clean_access(b->sources);
		CHECK(a->symbols != b->symbols);
		clean_semantics(a);
		clean_semantics(b);
		CHECK(tcc_relocate(a) == 0);
		function = (int (*)(void))tcc_get_symbol(a, "value");
		CHECK(function && function() == 17);
		function = (int (*)(void))tcc_get_symbol(a, "second");
		CHECK(function && function() == 31);
		function = (int (*)(void))tcc_get_symbol(a, "number");
		CHECK(function && function() == 1);
		CHECK(!a->pp->parsed.data && !a->pp->spelling.data);
		CHECK(!a->pp->macro_stack && !a->pp->macro_ptr && !a->pp->counter);
		CHECK(tcc_relocate(b) == 0);
		function = (int (*)(void))tcc_get_symbol(b, "value");
		CHECK(function && function() == 29);
		function = (int (*)(void))tcc_get_symbol(b, "second");
		CHECK(function && function() == 43);
		function = (int (*)(void))tcc_get_symbol(b, "number");
		CHECK(function && function() == 1);
		tcc_delete(a);
		tcc_delete(b);

		a = session(argv[1]);
		CHECK(tcc_compile_string(a,
			"#line 73 \"broken.c\"\nint broken(void) { return missing; }\n") < 0);
		CHECK(strstr(diagnostic, "broken.c:73:") && strstr(diagnostic, "missing"));
		clean_access(a->sources);
		tcc_delete(a);
		a = session(argv[1]);
		CHECK(tcc_compile_string(a,
			"#define INNER(x) ((x) + missing)\n"
			"#define OUTER(x) INNER(x)\n"
			"int broken(void) { return OUTER(7); }\n") < 0);
		CHECK(!a->pp->macro_stack && !a->pp->macro_ptr);
		CHECK(!a->pp->source && !a->pp->token && !a->pp->value.i);
		CHECK(!a->pp->parsed.data && !a->pp->spelling.data);
		clean_access(a->sources);
		tcc_delete(a);
		a = session(argv[1]);
		CHECK(tcc_compile_string(a,
			"int broken(void) { goto missing; }\n") < 0);
		CHECK(strstr(diagnostic, "missing") &&
			strstr(diagnostic, "used but not defined"));
		clean_semantics(a);
		clean_access(a->sources);
		tcc_delete(a);
		a = session(argv[1]);
		CHECK(tcc_compile_string(a, "int fresh(void) { return 9; }") == 0);
		clean_access(a->sources);
		tcc_delete(a);
	}
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &handles_after));
	CHECK(handles_after == handles_before);
	puts("alternating sessions, macro isolation, relocation and error cleanup passed");
	return 0;
}
