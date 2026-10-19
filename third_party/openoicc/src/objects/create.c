/* SPDX-License-Identifier: MIT */
#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int node_scaffold(struct text *source, const char *name, const char *bases,
	const char *header)
{
	size_t i;
	const char *interfaces[] = {
		"input", "output", "io_put", "interact0", "interact1", "interact2", "interact3"
	};
	const char *parameters[] = {
		"unsigned char *input_stream, unsigned int quantity",
		"unsigned char *output_stream, unsigned int capacity, unsigned int *output_quantity",
		"unsigned char *input_stream, unsigned int quantity, unsigned char *output_stream, unsigned int capacity, unsigned int *output_quantity",
		"unsigned int action",
		"unsigned int action, unsigned char *output_stream, unsigned int capacity, unsigned int *output_quantity",
		"unsigned int action, unsigned char *input_stream, unsigned int quantity",
		"unsigned int action, unsigned char *input_stream, unsigned int quantity, unsigned char *output_stream, unsigned int capacity, unsigned int *output_quantity"
	};
	const char *dispatch[] = {
		"INPUT", "OUTPUT", "IOPUT", "INTERACT0", "INTERACT1", "INTERACT2", "INTERACT3"
	};
#define ADD(...) do { if (!text_printf(source, __VA_ARGS__)) return 0; } while (0)
	if (!node_name_valid(name)) {
		fprintf(stderr, "invalid or reserved node name\n");
		return 0;
	}
	ADD("/* SPDX-License-Identifier: MIT */\n#include \"%s\"\n#include \"neoioic.h\"\n\n/* <oicc> */\n/* <ibn> %s </ibn> */\n/* <crt> */\n/* </crt> */\n/* </oicc> */\n\n", header, bases);
	ADD("static int %s_open(struct object *self, const struct vr *caller_context)\n{\n\tOBS_OBJECT_OPEN_;\n\treturn R_P;\n}\n\n", name);
	for (i = 0; i < 7; i++)
		ADD("static int %s_%s(struct object *self, %s,\n\tconst struct vr *caller_context)\n{\n\tSBO_OBJECT_%s;\n}\n\n", name, interfaces[i], parameters[i], dispatch[i]);
	ADD("static int %s_close(struct object *self, const struct vr *caller_context)\n{\n\tOBS_OBJECT_CLOSE_;\n\treturn self->ref_count ? R_P : RP_RCZERO;\n}\n\nvoid tog_%s(struct object *object)\n{\n\tobject->open = %s_open;\n", name, name, name);
	for (i = 0; i < 7; i++)
		ADD("\tobject->%s = %s_%s;\n", interfaces[i], name, interfaces[i]);
	ADD("\tobject->close = %s_close;\n}\n", name);
	return 1;
#undef ADD
}

int node_create(const char *name, const char *path, const char *bases)
{
	struct text source = { 0 };
	char *temporary = NULL;
	int ok = 0;
	size_t i;
	if (!node_scaffold(&source, name, bases, "host.h"))
		goto done;
	/* Validate the complete scaffold before creating the user destination. */
	if (!file_prepare(path, &source, &temporary))
		goto done;
	/* node_parse derives its name from the destination, not the temp suffix. */
	const char *basename = path;
	for (i = 0; path[i]; i++)
		if (path[i] == '/' || path[i] == '\\')
			basename = path + i + 1;
	if (strlen(basename) != strlen(name) + 2 ||
	    strncmp(basename, name, strlen(name)) || strcmp(basename + strlen(name), ".c"))
		goto done;
	/* Restrict the list to names and separators before publishing it. */
	int expect_name = 1;
	const char *p = bases;
	while (*p) {
		while (isspace((unsigned char)*p))
			p++;
		if (!*p)
			break;
		if (expect_name) {
			if (!isalpha((unsigned char)*p) || (unsigned char)*p >= 128)
				goto done;
			while (isalnum((unsigned char)*p) || *p == '_')
				p++;
		} else if (*p++ != ',') {
			goto done;
		}
		expect_name = !expect_name;
	}
	if (expect_name)
		goto done;
	ok = file_commit(path, temporary, 0);
done:
	if (temporary)
		remove(temporary);
	free(temporary);
	free(source.data);
	if (!ok)
		fprintf(stderr, "%s: cannot create node (invalid name/bases/path, existing destination or write error)\n", path);
	return ok;
}
