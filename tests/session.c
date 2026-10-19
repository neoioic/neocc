#include "compiler/tcc.h"
#include "check.h"
#include "objects/source.h"

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
		tcc_define_symbol(a, "SESSION_VALUE", "17");
		tcc_define_symbol(b, "SESSION_VALUE", "29");
		CHECK(tcc_compile_string(a, "int value(void) { return SESSION_VALUE; }") == 0);
		CHECK(tcc_compile_string(b, "int value(void) { return SESSION_VALUE; }") == 0);
		CHECK(tcc_compile_string(a, "#ifdef PRIVATE_MACRO\n#error leaked macro\n#endif\nint second(void) { return 31; }") == 0);
		CHECK(tcc_compile_string(b, "#define PRIVATE_MACRO 1\nint second(void) { return 43; }") == 0);
		clean_access(a->sources);
		clean_access(b->sources);
		CHECK(tcc_relocate(a) == 0);
		function = (int (*)(void))tcc_get_symbol(a, "value");
		CHECK(function && function() == 17);
		CHECK(tcc_relocate(b) == 0);
		function = (int (*)(void))tcc_get_symbol(b, "value");
		CHECK(function && function() == 29);
		tcc_delete(a);
		tcc_delete(b);

		a = session(argv[1]);
		CHECK(tcc_compile_string(a,
			"#line 73 \"broken.c\"\nint broken(void) { return missing; }\n") < 0);
		CHECK(strstr(diagnostic, "broken.c:73:") && strstr(diagnostic, "missing"));
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
