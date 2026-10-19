/* SPDX-License-Identifier: MIT */
#include "project.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int same_model_name(const char *a, const char *b)
{
	while (*a && *b) {
		if (toupper((unsigned char)*a++) != toupper((unsigned char)*b++))
			return 0;
	}
	return *a == *b;
}

int graph_validate(struct em_project *project)
{
	size_t n = project->count, i, j, k, complete = 1;
	unsigned char *done;
	if (!n || n >= USHRT_MAX || n + 1 > SIZE_MAX / (n + 1))
		return 0;
	for (i = 0; i < n; i++) {
		struct node *node = project->nodes + i;
		for (j = 0; j < i; j++)
			if (same_model_name(node->name, project->nodes[j].name))
				return node_diagnostic(node, 0, "duplicate node/model name");
		if (node->direct_count > USHRT_MAX)
			return node_diagnostic(node, 0, "too many direct bases");
		node->direct = malloc(node->direct_count * sizeof(*node->direct));
		if (!node->direct)
			return 0;
		for (j = 0; j < node->direct_count; j++) {
			const char *name = node->base_names[j];
			if (!strcmp(name, "NEOioic")) {
				node->direct[j] = n;
			} else {
				for (k = 0; k < n; k++)
					if (!strcmp(name, project->nodes[k].name))
						break;
				if (k == n)
					return node_diagnostic(node, 0, "unknown base node");
				node->direct[j] = k;
			}
			for (k = 0; k < j; k++)
				if (node->direct[k] == node->direct[j])
					return node_diagnostic(node, 0, "duplicate direct base");
		}
	}
	project->reach = calloc((n + 1) * (n + 1), 1);
	done = calloc(n + 1, 1);
	if (!project->reach || !done) {
		free(done);
		return 0;
	}
	done[n] = 1;
	while (complete < n + 1) {
		size_t before = complete;
		for (i = 0; i < n; i++) {
			struct node *node = project->nodes + i;
			unsigned char *row = project->reach + i * (n + 1);
			if (done[i])
				continue;
			for (j = 0; j < node->direct_count; j++)
				if (!done[node->direct[j]])
					break;
			if (j != node->direct_count)
				continue;
			for (j = 0; j < node->direct_count; j++) {
				size_t base = node->direct[j];
				row[base] = 1;
				for (k = 0; k < n + 1; k++)
					row[k] |= project->reach[base * (n + 1) + k];
			}
			done[i] = 1;
			complete++;
		}
		if (before == complete) {
			for (i = 0; done[i]; i++)
				;
			free(done);
			return node_diagnostic(project->nodes + i, 0, "inheritance cycle");
		}
	}
	free(done);
	return 1;
}

/* Iterative preorder avoids a C stack limit on deep valid inheritance graphs. */
size_t graph_order(const struct em_project *project, size_t index, size_t *order)
{
	struct frame { size_t node, next; };
	size_t n = project->count + 1, depth = 1, count = 1;
	struct frame *stack = calloc(n, sizeof(*stack));
	unsigned char *seen = calloc(n, 1);
	if (!stack || !seen) {
		free(stack);
		free(seen);
		return 0;
	}
	order[0] = index;
	seen[index] = 1;
	stack[0].node = index;
	while (depth) {
		struct frame *frame = stack + depth - 1;
		if (frame->node == project->count ||
		    frame->next == project->nodes[frame->node].direct_count) {
			depth--;
			continue;
		}
		size_t base = project->nodes[frame->node].direct[frame->next++];
		if (seen[base])
			continue;
		seen[base] = 1;
		order[count++] = base;
		stack[depth++] = (struct frame){ base, 0 };
	}
	free(stack);
	free(seen);
	return count;
}
