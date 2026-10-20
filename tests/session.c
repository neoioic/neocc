#include "compiler/tcc.h"
#include "check.h"
#include "objects/temporaries.h"
#include "objects/source.h"
#include "objects/symbols.h"
#include "objects/scopes.h"
#include "objects/functions.h"
#include "objects/switches.h"

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

int main(int argc, char **argv)
{
	struct TCCState *a, *b;
	int (*function)(void);
	unsigned long handles_before, handles_after;
	int i;

	CHECK(argc == 2);
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &handles_before));
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
		tcc_define_symbol(a, "SESSION_VALUE", "17");
		tcc_define_symbol(b, "SESSION_VALUE", "29");
		CHECK(tcc_compile_string(a,
			"int value(void) { return SESSION_VALUE + __COUNTER__; }") == 0);
		CHECK(tcc_compile_string(b,
			"int value(void) { return SESSION_VALUE + __COUNTER__; }") == 0);
		CHECK(tcc_compile_string(a,
			"#ifdef PRIVATE_MACRO\n#error leaked macro\n#endif\n"
			"#define CAT_(a,b) a##b\n#define CAT(a,b) CAT_(a,b)\n"
			"#define STR_(x) #x\n#define STR(x) STR_(x)\n"
			"#define VALUE(x) ((x) + __COUNTER__)\n"
			"int second(void) { const char *s = STR(CAT(he,llo)); "
			"return VALUE(31) + (s[0] != 'h'); }") == 0);
		CHECK(tcc_compile_string(b,
			"#define PRIVATE_MACRO 1\n"
			"int second(void) { return 43 + __COUNTER__; }") == 0);
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
