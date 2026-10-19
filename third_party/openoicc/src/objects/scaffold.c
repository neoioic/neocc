/* SPDX-License-Identifier: MIT */
#include "project.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const types[] = { "go", "ao", "co", "ro", "mo" };

int node_type(const char *name)
{
	if (strlen(name) != 2)
		return -1;
	for (size_t i = 0; i < sizeof(types) / sizeof(*types); i++)
		if (tolower((unsigned char)name[0]) == types[i][0] &&
		    tolower((unsigned char)name[1]) == types[i][1])
			return (int)i;
	return -1;
}

struct creation {
	char **files, **directories;
	size_t file_count, directory_count;
};

static int remember(char ***items, size_t *count, const char *path)
{
	struct text copy = { 0 };
	char **grown;
	if (*count == SIZE_MAX / sizeof(*grown) ||
	    !text_append(&copy, path, strlen(path)))
		return 0;
	grown = realloc(*items, (*count + 1) * sizeof(*grown));
	if (!grown) {
		free(copy.data);
		return 0;
	}
	*items = grown;
	(*items)[(*count)++] = copy.data;
	return 1;
}

static void creation_clear(struct creation *creation, int rollback)
{
	for (size_t i = creation->file_count; i > 0; i--) {
		if (rollback && remove(creation->files[i - 1]))
			fprintf(stderr, "%s: cannot remove created file during cleanup\n", creation->files[i - 1]);
		free(creation->files[i - 1]);
	}
	for (size_t i = creation->directory_count; i > 0; i--) {
		if (rollback && !directory_remove(creation->directories[i - 1]))
			fprintf(stderr, "%s: cannot remove created directory during cleanup\n", creation->directories[i - 1]);
		free(creation->directories[i - 1]);
	}
	free(creation->files);
	free(creation->directories);
}

static int create_directory(struct creation *creation, const char *path)
{
	if (file_directory(path))
		return 1;
	if (!remember(&creation->directories, &creation->directory_count, path))
		return 0;
	if (directory_create(path))
		return 1;
	free(creation->directories[--creation->directory_count]);
	return 0;
}

static int create_file(struct creation *creation, const char *path, const struct text *text)
{
	char *temporary = NULL;
	if (!remember(&creation->files, &creation->file_count, path))
		return 0;
	int ok = file_prepare(path, text, &temporary) && file_commit(path, temporary, 0);
	if (temporary)
		remove(temporary);
	free(temporary);
	if (!ok)
		free(creation->files[--creation->file_count]);
	return ok;
}

static int create_header(struct creation *creation, const char *directory,
	const char *name, const char *include)
{
	struct text path = { 0 }, text = { 0 };
	int ok = text_printf(&path, "%s/%s.h", directory, name) &&
		text_printf(&text, "/* SPDX-License-Identifier: MIT */\n#ifndef OPENOICC_NODE_");
	for (const char *p = name; ok && *p; p++)
		ok = text_printf(&text, "%c", toupper((unsigned char)*p));
	if (ok)
		ok = text_printf(&text, "_H\n#define OPENOICC_NODE_");
	for (const char *p = name; ok && *p; p++)
		ok = text_printf(&text, "%c", toupper((unsigned char)*p));
	if (ok)
		ok = text_printf(&text, "_H\n#include \"%s\"\n\n/* Define struct em_%s here when this node owns members. */\n#endif\n", include, name) &&
			create_file(creation, path.data, &text);
	free(path.data);
	free(text.data);
	return ok;
}

static int create_node(struct creation *creation, const char *directory,
	const char *name, const char *include, int application)
{
	struct text source = { 0 }, path = { 0 }, header = { 0 };
	int ok = create_header(creation, directory, name, include) &&
		text_printf(&header, "%s.h", name) &&
		node_scaffold(&source, name, "NEOioic", header.data);
	if (ok && application)
		ok = text_printf(&source, "\nint main(int argc, char **argv)\n{\n\t(void)argc;\n\t(void)argv;\n\treturn 0;\n}\n");
	if (ok)
		ok = text_printf(&path, "%s/%s.c", directory, name) &&
			create_file(creation, path.data, &source);
	free(path.data);
	free(header.data);
	free(source.data);
	return ok;
}

static const char host_source[] =
"/* SPDX-License-Identifier: MIT */\n"
"#include \"ho.h\"\n"
"/* OID_HO identifies this host; OID_ is its first allocatable node ID.\n"
" * Define both in ho.h. Each object owns a contiguous range of node IDs. */\n"
"static unsigned char node_ids[UINT16_MAX + 1];\n\n"
"int host_create_object(unsigned short model_id, unsigned int input_capacity,\n"
"\tunsigned int output_capacity, int caller_capacity,\n"
"\tstruct object **object_out, unsigned char *exotic_memory)\n{\n"
"\t/* Declare each crt_name here and call it in the switch below. */\n"
"\tstruct object *created;\n\tstruct em_neoioic *root;\n"
"\tsize_t best = 0, available = 0, start = 0, length = 0;\n"
"\tuint64_t oid;\n\tint result;\n"
"\tif (object_out)\n\t\t*object_out = NULL;\n"
"\tif (caller_capacity < 1 || (!!object_out == !!exotic_memory) ||\n"
"\t    OID_ <= OID_HO || (OID_ & ~UINT64_C(0xffff)) != OID_HO)\n"
"\t\treturn RN_INVALIDARG;\n"
"\tfor (size_t i = (size_t)(OID_ & UINT64_C(0xffff)); i <= UINT16_MAX; i++) {\n"
"\t\tif (node_ids[i]) {\n\t\t\tlength = 0;\n\t\t\tcontinue;\n\t\t}\n"
"\t\tif (!length)\n\t\t\tstart = i;\n\t\tlength++;\n"
"\t\tif (length > available) {\n\t\t\tbest = start;\n\t\t\tavailable = length;\n\t\t}\n\t}\n"
"\tif (!available)\n\t\treturn RN_LACKOID;\n"
"\toid = OID_HO + best;\n\t(void)oid;\n"
"\tswitch (model_id) {\n"
"\t/* case MSN_NAME:\n"
"\t *\tresult = crt_name(&oid, input_capacity, output_capacity,\n"
"\t *\t\tcaller_capacity, object_out, exotic_memory);\n\t *\tbreak;\n\t */\n"
"\tdefault:\n\t\treturn RN_INVALIDARG;\n\t}\n"
"\tif (RN(result))\n\t\treturn result;\n"
"\tcreated = exotic_memory ? (struct object *)exotic_memory : *object_out;\n"
"\tif (created->node_count > available) {\n"
"\t\troot = get_base_members(created, MSN_NEOIOIC);\n"
"\t\tfree(root->iq.data);\n\t\tfree(root->oq.data);\n"
"\t\tif (exotic_memory) {\n\t\t\troot->iq = (struct queue){ 0 };\n"
"\t\t\troot->oq = (struct queue){ 0 };\n\t\t} else {\n"
"\t\t\tfree(created);\n\t\t\t*object_out = NULL;\n\t\t}\n\t\treturn RN_LACKOID;\n\t}\n"
"\tfor (size_t i = 0; i < created->node_count; i++)\n\t\tnode_ids[best + i] = 1;\n"
"\treturn result;\n}\n\n"
"char host_destroy_object(struct object *object)\n{\n"
"\tif (!object || object->ref_count)\n\t\treturn FALSE;\n"
"\tfor (size_t i = 0; i < object->node_count; i++)\n"
"\t\tif (object->nodes[i].ref_count)\n\t\t\treturn FALSE;\n"
"\tstruct em_neoioic *root = get_base_members(object, MSN_NEOIOIC);\n"
"\tif (!root)\n\t\treturn FALSE;\n"
"\tfree(root->iq.data);\n\tfree(root->oq.data);\n"
"\troot->iq = (struct queue){ 0 };\n\troot->oq = (struct queue){ 0 };\n"
"\tfor (size_t i = 0; i < object->node_count; i++) {\n"
"\t\tnode_ids[object->nodes[i].oid & UINT64_C(0xffff)] = 0;\n"
"\t\tobject->nodes[i].oid = 0;\n\t}\n\treturn TRUE;\n}\n";

static int create_host(struct creation *creation, const char *directory)
{
	struct text path = { 0 };
	struct text source = { (char *)host_source, sizeof(host_source) - 1, 0 };
	int ok = text_printf(&path, "%s/ho.c", directory) &&
		create_file(creation, path.data, &source);
	path.length = 0;
	static const char header[] =
		"/* SPDX-License-Identifier: MIT */\n#ifndef OPENOICC_HO_H\n#define OPENOICC_HO_H\n"
		"#include \"neoioic.h\"\n\n"
		"/* Define this application's OID_HO, OID_, MSN_* and AC*_* values here.\n"
		" * Include all member headers needed by generated constructors.\n"
		" * OID_HO has a zero low 16-bit field; OID_ starts inside that field. */\n"
		"#endif\n";
	source = (struct text){ (char *)header, sizeof(header) - 1, 0 };
	if (ok)
		ok = text_printf(&path, "%s/ho.h", directory) &&
			create_file(creation, path.data, &source);
	free(path.data);
	return ok;
}

static int create_branch(struct creation *creation, const char *directory,
	const char *name, const char *include)
{
	struct text path = { 0 };
	int ok = text_printf(&path, "%s/%s", directory, name) &&
		create_directory(creation, path.data) &&
		create_header(creation, path.data, name, include);
	free(path.data);
	return ok;
}

static int name_available(const char *path, void *context)
{
	const char *name = context, *base = path;
	for (const char *p = path; *p; p++)
		if (*p == '/' || *p == '\\')
			base = p + 1;
	while (*name && *base && tolower((unsigned char)*name) == tolower((unsigned char)*base)) {
		name++;
		base++;
	}
	return *name || *base;
}

int project_create(int type, const char *name, const char *directory)
{
	struct creation creation = { 0 };
	struct text root = { 0 }, branch = { 0 }, parent = { 0 };
	struct text current = { 0 }, normalized = { 0 };
	const char *reserved[] = { "ho", "so", "se", "ce", "re", "oioic" };
	int ok = 0;
	if (!strcmp(directory, ".") || !strcmp(directory, "./") || !strcmp(directory, ".\\")) {
		if (!directory_current(&current))
			goto done;
		directory = current.data;
	}
	size_t length = strlen(directory);
	while (length > 1 && (directory[length - 1] == '/' || directory[length - 1] == '\\') &&
	       directory[length - 2] != ':')
		length--;
	if (!text_append(&normalized, directory, length))
		goto done;
	directory = normalized.data;
	if (type < 0 || type >= 5 || !node_name_valid(name) || node_type(name) >= 0 ||
	    !file_directory(directory))
		goto done;
	for (size_t i = 0; i < sizeof(reserved) / sizeof(*reserved); i++)
		if (!name_available(reserved[i], (void *)name))
			goto done;
	if (type < 2) {
		/* SO names are unique across both branches and the host itself. */
		const char *base = directory + strlen(directory);
		while (base > directory && (base[-1] == '/' || base[-1] == '\\'))
			base--;
		const char *start = base;
		while (start > directory && start[-1] != '/' && start[-1] != '\\')
			start--;
		if ((size_t)(base - start) == strlen(name)) {
			struct text host_name = { 0 };
			int distinct = text_append(&host_name, start, (size_t)(base - start)) &&
				name_available(host_name.data, (void *)name);
			free(host_name.data);
			if (!distinct)
				goto done;
		}
		for (int i = 0; i < 2; i++) {
			branch.length = 0;
			if (!text_printf(&branch, "%s/so/%s", directory, types[i]) ||
			    !directory_visit(branch.data, name_available, (void *)name))
				goto done;
		}
		if (!text_printf(&root, "%s/so/%s/%s", directory, types[type], name) ||
		    !directory_create(root.data))
			goto done;
		if (!remember(&creation.directories, &creation.directory_count, root.data)) {
			directory_remove(root.data);
			goto done;
		}
		if (!text_printf(&parent, "../%s.h", types[type]))
			goto done;
		ok = create_node(&creation, root.data, name, parent.data, 0);
		goto done;
	}
	/* When DIRECTORY already names the category, do not duplicate it. */
	const char *base = directory;
	for (const char *p = directory; *p; p++)
		if ((*p == '/' || *p == '\\') && p[1])
			base = p + 1;
	if (!strcmp(base, types[type])) {
		if (!text_append(&parent, directory, strlen(directory)))
			goto done;
	} else if (!text_printf(&parent, "%s/%s", directory, types[type]) ||
		   !create_directory(&creation, parent.data)) {
		goto done;
	}
	if (!directory_visit(parent.data, name_available, (void *)name) ||
	    !text_printf(&root, "%s/%s", parent.data, name) ||
	    !directory_create(root.data))
		goto done;
	if (!remember(&creation.directories, &creation.directory_count, root.data)) {
		directory_remove(root.data);
		goto done;
	}
	if (!create_host(&creation, root.data) ||
	    !create_node(&creation, root.data, name, "ho.h", 0) ||
	    !create_branch(&creation, root.data, "se", "../ho.h") ||
	    !create_branch(&creation, root.data, "so", "../ho.h"))
		goto done;
	branch.length = 0;
	if (!text_printf(&branch, "%s/so", root.data) ||
	    !create_branch(&creation, branch.data, "ao", "../so.h") ||
	    !create_branch(&creation, branch.data, "go", "../so.h"))
		goto done;
	if (type == 4) {
		branch.length = 0;
		if (!text_printf(&branch, "%s/so/ao/Main", root.data) ||
		    !create_directory(&creation, branch.data) ||
		    !create_node(&creation, branch.data, "Main", "../ao.h", 1))
			goto done;
	}
	ok = 1;
done:
	creation_clear(&creation, !ok);
	free(root.data);
	free(parent.data);
	free(branch.data);
	free(current.data);
	free(normalized.data);
	if (!ok)
		fprintf(stderr, "%s: cannot create typed node (invalid/reserved/duplicate name, directory or write error)\n", name);
	return ok;
}
