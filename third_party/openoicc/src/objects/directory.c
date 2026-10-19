/* SPDX-License-Identifier: MIT */
#include "project.h"
#include <stdlib.h>
#include <string.h>

static int collect_source(const char *path, void *context)
{
	struct em_project *project = context;
	struct node node = { 0 };
	struct text owned_path = { 0 }, mask = { 0 };
	size_t length = strlen(path);
	int marked = 0, ok;
	if (file_directory(path))
		return directory_visit(path, collect_source, project);
	if (length < 2 || strcmp(path + length - 2, ".c"))
		return 1;
	node.path = path;
	ok = file_read(path, &node.source) && source_mask(&node, &mask, 0, &marked);
	free(mask.data);
	if (!ok || !marked) {
		free(node.source.data);
		return ok;
	}
	if (!text_append(&owned_path, path, length)) {
		free(node.source.data);
		return 0;
	}
	struct node *nodes = NULL;
	if (project->count < SIZE_MAX / sizeof(*nodes))
		nodes = realloc(project->nodes, (project->count + 1) * sizeof(*nodes));
	if (!nodes) {
		free(node.source.data);
		free(owned_path.data);
		return 0;
	}
	node.path = owned_path.data;
	project->nodes = nodes;
	project->nodes[project->count++] = node;
	return 1;
}

static int source_order(const void *left, const void *right)
{
	const struct node *a = left, *b = right;
	return strcmp(a->path, b->path);
}

int project_collect(struct em_project *project, const char *directory)
{
	if (!file_directory(directory) || !directory_visit(directory, collect_source, project))
		return 0;
	if (!project->count)
		return 0;
	qsort(project->nodes, project->count, sizeof(*project->nodes), source_order);
	return 1;
}
