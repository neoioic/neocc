/* SPDX-License-Identifier: MIT */
#include "project.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *space(const char *p)
{
	while (isspace((unsigned char)*p))
		p++;
	return p;
}

/* The source has already been masked, so delimiters in comments/literals
 * cannot enter this scan. Return NULL on unmatched delimiters. */
static const char *balanced(const char *p, char open, char close)
{
	size_t depth = 0;
	for (; *p; p++) {
		if (*p == open)
			depth++;
		if (*p == close && !--depth)
			return p + 1;
	}
	return NULL;
}

int host_targets(struct em_project *project, struct node *host, unsigned char *targets)
{
	struct text mask = { 0 };
	const char *body = NULL, *end = NULL, *p;
	int definitions = 0, ok = 0;
	if (!source_mask(host, &mask, 0, NULL))
		goto done;
	for (p = mask.data; *p;) {
		const char *start = p;
		if (*p == '#') {
			const char *directive = space(p + 1);
			if (!strncmp(directive, "if", 2) || !strncmp(directive, "else", 4) ||
			    !strncmp(directive, "elif", 4) || !strncmp(directive, "endif", 5))
				goto ambiguous;
			const char *line_end = strchr(directive, '\n');
			if (!line_end)
				line_end = directive + strlen(directive);
			const char *constructor = strstr(directive, "crt_");
			const char *creator = strstr(directive, "host_create_object");
			if ((constructor && constructor < line_end) || (creator && creator < line_end))
				goto ambiguous;
			p = line_end;
			continue;
		}
		if (!(isalpha((unsigned char)*p) || *p == '_')) {
			p++;
			continue;
		}
		while (isalnum((unsigned char)*p) || *p == '_')
			p++;
		if ((size_t)(p - start) != strlen("host_create_object") ||
		    strncmp(start, "host_create_object", (size_t)(p - start)))
			continue;
		const char *parameters = space(p);
		if (*parameters != '(')
			continue;
		const char *after = balanced(parameters, '(', ')');
		if (!after || *space(after) != '{')
			continue;
		body = space(after);
		end = balanced(body, '{', '}');
		if (!end)
			goto ambiguous;
		definitions++;
		p = end;
	}
	if (definitions != 1) {
		node_diagnostic(host, 0, "expected exactly one host_create_object definition");
		goto done;
	}
	for (p = body + 1; p < end - 1;) {
		const char *start = p;
		if (*p == '#')
			goto ambiguous;
		if (!(isalpha((unsigned char)*p) || *p == '_')) {
			p++;
			continue;
		}
		while (isalnum((unsigned char)*p) || *p == '_')
			p++;
		if (p - start <= 4 || strncmp(start, "crt_", 4))
			continue;
		const char *parameters = space(p);
		if (*parameters != '(')
			goto ambiguous;
		const char *after = balanced(parameters, '(', ')');
		if (!after || after > end)
			goto ambiguous;
		/* Ignore a local extern declaration, not a constructor invocation. */
		const char *before = start;
		while (before > body && isspace((unsigned char)before[-1]))
			before--;
		const char *word = before;
		while (word > body && (isalnum((unsigned char)word[-1]) || word[-1] == '_'))
			word--;
		if (before - word == 3 && !strncmp(word, "int", 3)) {
			p = after;
			continue;
		}
		size_t i;
		for (i = 0; i < project->count; i++)
			if (strlen(project->nodes[i].name) == (size_t)(p - start - 4) &&
			    !strncmp(project->nodes[i].name, start + 4, (size_t)(p - start - 4)))
				break;
		if (i == project->count) {
			node_diagnostic(host, (size_t)(start - mask.data), "constructor call refers to an unknown node");
			goto done;
		}
		targets[i] = 1;
	}
	ok = 1;
	goto done;
ambiguous:
	node_diagnostic(host, 0, "unsupported or ambiguous host creation function");
done:
	free(mask.data);
	return ok;
}
