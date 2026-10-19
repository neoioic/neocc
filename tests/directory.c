#include "check.h"
#include "support/text.h"
#include <windows.h>
#include <string.h>

struct visitor {
	int count;
	int stop;
};

static int visit(const char *path, void *context)
{
	struct visitor *visitor = context;
	CHECK(strstr(path, "file "));
	visitor->count++;
	return !visitor->stop;
}

int main(void)
{
	char directory[128], path[256];
	struct visitor visitor = { 0 };
	unsigned long before, after;
	FILE *file;
	int i;

	snprintf(directory, sizeof(directory), "build/directory-%lu",
		(unsigned long)GetCurrentProcessId());
	CHECK(directory_create(directory));
	CHECK(directory_visit(directory, visit, &visitor));
	CHECK(!visitor.count);
	CHECK(!directory_visit("build/nonexistent-directory", visit, &visitor));
	for (i = 0; i < 3; i++) {
		snprintf(path, sizeof(path), "%s/file %d.c", directory, i);
		file = fopen(path, "wb");
		CHECK(file && fclose(file) == 0);
	}
	CHECK(directory_visit(directory, visit, &visitor));
	CHECK(visitor.count == 3);
	CHECK(!directory_visit(path, visit, &visitor));
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &before));
	for (i = 0; i < 1000; i++) {
		visitor = (struct visitor){ 0, 1 };
		CHECK(!directory_visit(directory, visit, &visitor));
		CHECK(visitor.count == 1);
	}
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &after));
	CHECK(before == after);
	for (i = 0; i < 3; i++) {
		snprintf(path, sizeof(path), "%s/file %d.c", directory, i);
		CHECK(remove(path) == 0);
	}
	CHECK(directory_remove(directory));
	puts("directory enumeration: empty, missing, spaced names and callback failure passed");
	return 0;
}
