/* SPDX-License-Identifier: MIT */
#ifndef OPENOICC_PROJECT_H
#define OPENOICC_PROJECT_H
#include "../support/text.h"
#include "object.h"
struct node {
	char *name;
	const char *path;
	struct text source;
	size_t begin, end;
	char **base_names;
	size_t *direct;
	size_t direct_count;
	int has_members;
	struct text output;
	char *temporary;
};
struct em_project {
	struct node *nodes;
	size_t count;
	unsigned char *reach;
};
/* Borrowed native request, valid only for the synchronous vo_interact2 call. */
struct project_request {
	int count;
	char **paths;
	int write;
	const char *directory;
};
int project_execute(struct em_project *project, const struct project_request *request);
void project_clear(struct em_project *project);
int node_parse(struct node *node);
int graph_validate(struct em_project *project);
size_t graph_order(const struct em_project *project, size_t index, size_t *order);
int node_generate(struct em_project *project, size_t index);
int node_erase(struct node *node);
int source_mask(struct node *node, struct text *mask, int markers, int *marked);
int host_targets(struct em_project *project, struct node *host, unsigned char *targets);
int project_collect(struct em_project *project, const char *directory);
int node_type(const char *name);
int project_create(int type, const char *name, const char *directory);
int node_create(const char *name, const char *path, const char *bases);
int node_scaffold(struct text *source, const char *name, const char *bases,
	const char *header);
int node_diagnostic(const struct node *node, size_t offset, const char *reason);
int identifier_valid(const char *name);
int node_name_valid(const char *name);
void tog_project(struct object *object);
#endif
