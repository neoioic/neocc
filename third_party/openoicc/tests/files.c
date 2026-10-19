/* SPDX-License-Identifier: MIT */
#include "support/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define task_pid _getpid
#else
#include <unistd.h>
#define task_pid getpid
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void)
{
	const char *target = "output.txt";
	struct text old = { 0 }, output = { 0 }, readback = { 0 }, collision = { 0 };
	char *temporary = NULL;
	FILE *file;
	CHECK(text_append(&old, "original", 8));
	CHECK(text_append(&output, "replacement", 11));
	CHECK(text_printf(&collision, "%s.openoicc-%lu-0.tmp", target,
			  (unsigned long)task_pid()));
	file = fopen(collision.data, "wb");
	CHECK(file && fwrite("sentinel", 1, 8, file) == 8 && fclose(file) == 0);
	CHECK(!file_prepare(target, &output, &temporary));
	CHECK(!temporary);
	CHECK(file_read(collision.data, &readback));
	CHECK(readback.length == 8 && !memcmp(readback.data, "sentinel", 8));
	free(readback.data);
	readback = (struct text){ 0 };
	CHECK(file_prepare(target, &old, &temporary));
	CHECK(file_commit(target, temporary, 0));
	free(temporary);
	temporary = NULL;
	CHECK(file_prepare(target, &output, &temporary));
	CHECK(!file_commit(target, temporary, 0));
	CHECK(file_read(target, &readback));
	CHECK(readback.length == old.length && !memcmp(readback.data, old.data, old.length));
	free(readback.data);
	readback = (struct text){ 0 };
	CHECK(file_commit(target, temporary, 1));
	free(temporary);
	temporary = NULL;
	CHECK(file_read(target, &readback));
	CHECK(readback.length == output.length && !memcmp(readback.data, output.data, output.length));
	CHECK(!file_prepare("missing/output.txt", &output, &temporary));
	CHECK(!temporary);
	CHECK(remove(target) == 0 && remove(collision.data) == 0);
	free(readback.data);
	free(collision.data);
	free(old.data);
	free(output.data);
	puts("file preparation and exclusive commit checks passed");
	return 0;
}
