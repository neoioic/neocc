#include "compiler/tcc.h"
#include "check.h"
#include "objects/source.h"
#include "objects/symbols.h"

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

static void clean_symbols(struct em_symbols *symbols)
{
	CHECK(!symbols->blocks && !symbols->free_first && !symbols->scope);
	for (int i = 0; i < NEOCC_SYMBOL_STACK_COUNT; i++)
		CHECK(!symbols->stacks[i]);
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
		clean_access(a->sources);
		clean_access(b->sources);
		CHECK(a->symbols != b->symbols);
		clean_symbols(a->symbols->members);
		clean_symbols(b->symbols->members);
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
		clean_symbols(a->symbols->members);
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
