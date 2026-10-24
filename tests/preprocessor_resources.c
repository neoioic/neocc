#include "neocc/preprocessor.h"
#include "host/host.h"
#include "objects/preprocessor.h"
#include "check.h"
#include <stdio.h>
#include <string.h>

#ifdef NEOCC_TEST_FAULT
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
static int fail_malloc, fail_realloc;
static size_t live;

void *__wrap_malloc(size_t size)
{
	void *pointer = fail_malloc ? NULL : __real_malloc(size);

	fail_malloc = 0;
	if (pointer)
		live++;
	return pointer;
}

void *__wrap_calloc(size_t count, size_t size)
{
	void *pointer = __real_calloc(count, size);

	if (pointer)
		live++;
	return pointer;
}

void *__wrap_realloc(void *old, size_t size)
{
	void *pointer = fail_realloc ? NULL : __real_realloc(old, size);

	fail_realloc = 0;
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
#endif

static void clean_access(struct object *object)
{
	for (unsigned int i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static const struct neocc_preprocessor_results *results(struct object *object)
{
	const struct neocc_preprocessor_results *view;
	unsigned char output[sizeof(view) + 1];
	unsigned int quantity = 123;

	CHECK(RP(vo_interact1(object, NEOCC_AC1_PREPROCESSOR_RESULTS,
		output + 1, sizeof(view), &quantity, &neocc_host_caller)));
	CHECK(quantity == sizeof(view));
	memcpy(&view, output + 1, sizeof(view));
	clean_access(object);
	return view;
}

static int find(struct object *object, const char *name, int add,
	struct neocc_cached_include **entry, unsigned int length,
	unsigned int capacity)
{
	struct neocc_include_request request = { name, add };
	unsigned char input[sizeof(request) + 1], output[sizeof(*entry) + 1];
	unsigned int quantity = 123;
	int result;

	memcpy(input + 1, &request, sizeof(request));
	result = vo_interact3(object, NEOCC_AC3_PREPROCESSOR_INCLUDE,
		input + 1, length, output + 1, capacity, &quantity,
		&neocc_host_caller);
	CHECK(quantity == (RP(result) ? sizeof(*entry) : 0));
	if (RP(result))
		memcpy(entry, output + 1, sizeof(*entry));
	clean_access(object);
	return result;
}

static struct neocc_cached_include *cache(struct object *object,
	const char *name, int add)
{
	struct neocc_cached_include *entry;

	CHECK(RP(find(object, name, add, &entry,
		sizeof(struct neocc_include_request), sizeof(entry))));
	return entry;
}

static int append(struct object *object, unsigned int action, const char *text)
{
	int result = vo_interact2(object, action, (unsigned char *)text,
		(unsigned int)strlen(text) + 1, &neocc_host_caller);

	clean_access(object);
	return result;
}

static int partial_close(struct object *self, const struct vr *caller_context)
{
	(void)self;
	(void)caller_context;
	return RN_CLOSEPARTIAL;
}

int main(void)
{
	struct object *object = NULL, *other = NULL, *root;
	struct neocc_cached_include *entry, *first;
	const struct neocc_preprocessor_results *view;
	unsigned int quantity;
	char name[80], text[] = "retained input";
	int (*saved_close)(struct object *, const struct vr *);
#ifdef NEOCC_TEST_FAULT
	size_t initial_live = live;
#endif

	CHECK(RP(create_object(NEOCC_MSN_PREPROCESSOR, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_PREPROCESSOR, 0, 0, 1, &other)));
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	view = results(object);
	CHECK(view != results(other));
	CHECK(!view->libraries.count && !view->dependencies.count);
	CHECK(!cache(object, "missing.h", 0));
	first = cache(object, "cached.h", 1);
	CHECK(!first->once && !first->ifndef_macro);
	first->ifndef_macro = 123;
	CHECK(cache(object, "cached.h", 1) == first);
	CHECK(!cache(other, "cached.h", 0));
	for (int i = 0; i < 513; i++) {
		snprintf(name, sizeof(name), "cache/%d/header.h", i);
		entry = cache(object, name, 1);
		CHECK(!entry->once && !entry->ifndef_macro);
		entry->ifndef_macro = i + 1;
	}
	for (int i = 512; i >= 0; i--) {
		snprintf(name, sizeof(name), "cache/%d/header.h", i);
		CHECK(cache(object, name, 0)->ifndef_macro == i + 1);
	}
	CHECK(first->ifndef_macro == 123);
#ifndef NEOCC_TEST_FAULT
	/* Path normalization allocates inside the host CRT, outside GNU wrappers.
	 * The ordinary test and compiler differentials exercise this host path.
	 */
	entry = cache(object, "tests/preprocessor_resources.c", 1);
	entry->once = 1;
	CHECK(cache(object, "tests/./preprocessor_resources.c", 0) == entry);
#ifdef _WIN32
	CHECK(cache(object, "TESTS/PREPROCESSOR_RESOURCES.C", 0) == entry);
#endif
#endif
	CHECK(find(object, "bad.h", 2, &entry,
		sizeof(struct neocc_include_request), sizeof(entry)) == RN_INVALIDARG);
	CHECK(find(object, NULL, 1, &entry,
		sizeof(struct neocc_include_request), sizeof(entry)) == RN_INVALIDARG);
	CHECK(find(object, "bad.h", 1, &entry,
		sizeof(struct neocc_include_request) - 1, sizeof(entry)) == RN_INVALIDARG);
	CHECK(find(object, "bad.h", 1, &entry,
		sizeof(struct neocc_include_request), sizeof(entry) - 1) == RN_INVALIDARG);
	CHECK(!cache(object, "bad.h", 0));
	quantity = 123;
	CHECK(vo_interact1(object, NEOCC_AC1_PREPROCESSOR_RESULTS,
		(unsigned char *)&view, sizeof(view) - 1, &quantity,
		&neocc_host_caller) == RN_INVALIDARG);
	CHECK(!quantity);
	CHECK(vo_interact2(object, NEOCC_AC2_PREPROCESSOR_DEPENDENCY,
		(unsigned char *)"bad", 3, &neocc_host_caller) == RN_INVALIDARG);
	CHECK(vo_interact2(object, NEOCC_AC2_PREPROCESSOR_LIBRARY,
		(unsigned char *)"a\0b", 4, &neocc_host_caller) == RN_INVALIDARG);
	CHECK(RP(append(object, NEOCC_AC2_PREPROCESSOR_DEPENDENCY, text)));
	CHECK(RP(append(object, NEOCC_AC2_PREPROCESSOR_LIBRARY, "kernel32")));
	text[0] = 'X';
	CHECK(!strcmp(view->dependencies.items[0], "retained input"));
	for (int i = 0; i < 513; i++) {
		snprintf(name, sizeof(name), "input-%d.c", i);
		CHECK(RP(append(object, NEOCC_AC2_PREPROCESSOR_DEPENDENCY, name)));
		CHECK(RP(append(object, NEOCC_AC2_PREPROCESSOR_LIBRARY, "kernel32")));
	}
	CHECK(view->dependencies.count == 514 && view->libraries.count == 514);
	CHECK(!strcmp(view->dependencies.items[513], "input-512.c"));
	CHECK(!results(other)->dependencies.count);
#ifdef NEOCC_TEST_FAULT
	{
		size_t before = live;

		fail_malloc = 1;
		CHECK(RN(find(object, "failure.h", 1, &entry,
			sizeof(struct neocc_include_request), sizeof(entry))));
		CHECK(!fail_malloc && !cache(object, "failure.h", 0));
		CHECK(cache(object, "cached.h", 0) == first);
		CHECK(live == before);
	}
	for (int library = 0; library < 2; library++) {
		size_t before = live;

		fail_realloc = 1;
		CHECK(RN(append(other, library ? NEOCC_AC2_PREPROCESSOR_LIBRARY :
			NEOCC_AC2_PREPROCESSOR_DEPENDENCY, "first failure")));
		CHECK(!fail_realloc && live == before);
		CHECK(!results(other)->libraries.items &&
		      !results(other)->dependencies.items);
	}
	for (int library = 0; library < 2; library++) {
		unsigned int action = library ? NEOCC_AC2_PREPROCESSOR_LIBRARY :
			NEOCC_AC2_PREPROCESSOR_DEPENDENCY;
		const struct neocc_preprocessor_list *list = library ?
			&view->libraries : &view->dependencies;
		size_t count = list->count, before = live;
		char **items = list->items;

		fail_malloc = 1;
		CHECK(RN(append(object, action, "failure")));
		CHECK(!fail_malloc && list->count == count && list->items == items);
		CHECK(live == before);
		while (list->count < list->capacity)
			CHECK(RP(append(object, action, "fill")));
		count = list->count;
		items = list->items;
		before = live;
		fail_realloc = 1;
		CHECK(RN(append(object, action, "failure")));
		CHECK(!fail_realloc && list->count == count && list->items == items);
		CHECK(live == before);
		CHECK(RP(append(object, action, "recovered")));
	}
#endif
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(cache(object, "cached.h", 0) == first);
	CHECK(results(object) == view && view->dependencies.count >= 514);
	CHECK(RP(vo_interact0(object, NEOCC_AC0_PREPROCESSOR_RESET,
		&neocc_host_caller)));
	CHECK(!cache(object, "cached.h", 0));
	for (int i = 0; i < NEOCC_INCLUDE_HASH_CAPACITY; i++)
		CHECK(!((struct em_preprocessor *)object->members)->includes[i]);
	CHECK(view == results(object) && view->libraries.count >= 514);
	CHECK(!strcmp(view->dependencies.items[0], "retained input"));
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(view->libraries.count >= 514);
	CHECK(!host_destroy_object(object));
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	quantity = 123;
	CHECK(vo_interact1(object, NEOCC_AC1_PREPROCESSOR_RESULTS,
		(unsigned char *)&view, sizeof(view), &quantity,
		&neocc_host_caller) == RN_INVALIDARG);
	CHECK(!quantity);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	view = results(object);
	CHECK(!view->dependencies.count && !view->libraries.count);
	CHECK(!view->dependencies.items && !view->libraries.items);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	clean_access(object);
	clean_access(other);
	destroy_object(object);
	destroy_object(other);
	CHECK(!object && !other);
#ifdef NEOCC_TEST_FAULT
	CHECK(live == initial_live);
#endif
	puts("include cache, persistent preprocessing results and cleanup passed");
	return 0;
}
