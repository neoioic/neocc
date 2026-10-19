/* SPDX-License-Identifier: MIT */
#include "project.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int identifier_valid(const char *name)
{
	const unsigned char *p = (const unsigned char *)name;
	if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')))
		return 0;
	for (p++; *p; p++)
		if (!isalnum(*p) || *p >= 128) {
			if (*p != '_')
				return 0;
		}
	return 1;
}

int node_name_valid(const char *name)
{
	const char *root = "NEOIOIC";
	if (!identifier_valid(name))
		return 0;
	while (*name && *root && toupper((unsigned char)*name) == *root) {
		name++;
		root++;
	}
	return *name || *root;
}

int node_diagnostic(const struct node *node, size_t offset, const char *reason)
{
	size_t line = 1, column = 1, i;
	for (i = 0; i < offset && i < node->source.length; i++) {
		if (node->source.data[i] == '\n') {
			line++;
			column = 1;
		} else {
			column++;
		}
	}
	fprintf(stderr, "%s:%zu:%zu: %s\n", node->path, line, column, reason);
	return 0;
}

static int parse_bases(struct node *node, const char *first, const char *last)
{
	while (first < last) {
		const char *end;
		char *name;
		char **grown;
		while (first < last && isspace((unsigned char)*first))
			first++;
		if (first == last)
			break;
		end = first;
		while (end < last && (isalnum((unsigned char)*end) || *end == '_'))
			end++;
		if (end == first)
			return node_diagnostic(node, (size_t)(first - node->source.data), "expected base name");
		name = malloc((size_t)(end - first) + 1);
		if (!name)
			return 0;
		memcpy(name, first, (size_t)(end - first));
		name[end - first] = 0;
		if (!identifier_valid(name)) {
			free(name);
			return node_diagnostic(node, 0, "invalid base name");
		}
		grown = realloc(node->base_names, (node->direct_count + 1) * sizeof(*grown));
		if (!grown) {
			free(name);
			return 0;
		}
		node->base_names = grown;
		node->base_names[node->direct_count++] = name;
		first = end;
		while (first < last && isspace((unsigned char)*first))
			first++;
		if (first == last)
			break;
		if (*first++ != ',')
			return node_diagnostic(node, 0, "bases must be comma separated");
		const char *next = first;
		while (next < last && isspace((unsigned char)*next))
			next++;
		if (next == last)
			return node_diagnostic(node, 0, "trailing comma in bases");
	}
	return node->direct_count != 0 || node_diagnostic(node, 0, "at least one base is required (use NEOioic)");
}

/* Markers must occur in comments. Replacement boundaries are whole standalone
 * block comments so inserting C cannot accidentally leave it inside a comment. */
static int comment_tags(struct node *node, size_t start, size_t end,
			int block, int *state)
{
	const char *data = node->source.data;
	size_t i;
	for (i = start + 2; i + 1 < end; i++) {
		const char *tags[] = { "<oicc>", "<ibn>", "</ibn>", "<crt>", "</crt>", "</oicc>" };
		size_t k;
		for (k = 0; k < 6; k++) {
			size_t length = strlen(tags[k]);
			if (i + length > end || memcmp(data + i, tags[k], length))
				continue;
			if ((int)k != *state)
				return node_diagnostic(node, i, "duplicate, misplaced or unclosed marker");
			if (k == 1) {
				const char *close = strstr(data + i + length, "</ibn>");
				if (!close || close >= data + end)
					return node_diagnostic(node, i, "ibn must close in the same comment");
				if (!parse_bases(node, data + i + length, close))
					return 0;
			}
			if (k == 3 || k == 4) {
				size_t p;
				if (!block)
					return node_diagnostic(node, i, "crt requires standalone block comments");
				for (p = start + 2; p < i; p++)
					if (!isspace((unsigned char)data[p]))
						return node_diagnostic(node, i, "crt requires standalone block comments");
				for (p = i + length; p < end - 2; p++)
					if (!isspace((unsigned char)data[p]))
						return node_diagnostic(node, i, "crt requires standalone block comments");
				if (k == 3)
					node->begin = end;
				else
					node->end = start;
			}
			(*state)++;
			i += length - 1;
			break;
		}
	}
	return 1;
}

static int source_valid(struct node *node)
{
	/* Validate bytes before lexing: NUL and C line splicing must not hide
	 * delimiters inside comments or literals. UTF-8 is the source contract. */
	for (size_t i = 0; i < node->source.length; i++) {
		unsigned char byte = (unsigned char)node->source.data[i];
		if (!byte)
			return node_diagnostic(node, i, "embedded NUL byte");
		if (byte == '\r' && (i + 1 == node->source.length ||
				    node->source.data[i + 1] != '\n'))
			return node_diagnostic(node, i, "bare CR newline is unsupported");
		if (i + 2 < node->source.length && byte == '?' &&
		    node->source.data[i + 1] == '?' &&
		    strchr("=/'()!<>-", node->source.data[i + 2]))
			return node_diagnostic(node, i, "C trigraphs are unsupported");
		if (i + 1 < node->source.length && byte == '%' &&
		    node->source.data[i + 1] == ':')
			return node_diagnostic(node, i, "digraph preprocessing is unsupported");
		if (byte == '\\' && i + 1 < node->source.length &&
		    (node->source.data[i + 1] == '\n' || node->source.data[i + 1] == '\r'))
			return node_diagnostic(node, i, "line splicing is unsupported");
		if (byte < 128)
			continue;
		size_t extra = byte >= 0xc2 && byte <= 0xdf ? 1 :
			byte >= 0xe0 && byte <= 0xef ? 2 :
			byte >= 0xf0 && byte <= 0xf4 ? 3 : 0;
		if (!extra || extra >= node->source.length - i)
			return node_diagnostic(node, i, "invalid UTF-8");
		unsigned char next = (unsigned char)node->source.data[i + 1];
		if ((byte == 0xe0 && next < 0xa0) || (byte == 0xed && next >= 0xa0) ||
		    (byte == 0xf0 && next < 0x90) || (byte == 0xf4 && next >= 0x90))
			return node_diagnostic(node, i, "invalid UTF-8");
		for (size_t j = 1; j <= extra; j++)
			if (((unsigned char)node->source.data[i + j] & 0xc0) != 0x80)
				return node_diagnostic(node, i, "invalid UTF-8");
		i += extra;
	}
	return 1;
}

int source_mask(struct node *node, struct text *mask, int markers, int *marked)
{
	size_t i = 0;
	int state = 0;
	const char *data = node->source.data;
	if (!source_valid(node) || !text_append(mask, data, node->source.length))
		return 0;
	if (mask->length >= 3 && !memcmp(mask->data, "\xef\xbb\xbf", 3))
		memset(mask->data, ' ', 3);
	while (i < mask->length) {
		size_t start = i, end;
		char quote = data[i];
		if (!data[i])
			return node_diagnostic(node, i, "embedded NUL byte");
		if (data[i] == '\\' && i + 1 < mask->length &&
		    (data[i + 1] == '\n' || data[i + 1] == '\r'))
			return node_diagnostic(node, i, "line splicing is unsupported");
		if (i + 1 < mask->length && data[i] == '/' &&
		    (data[i + 1] == '*' || data[i + 1] == '/')) {
			int block = data[i + 1] == '*';
			i += 2;
			if (block) {
				const char *close = strstr(data + i, "*/");
				if (!close)
					return node_diagnostic(node, start, "unclosed C comment");
				i = (size_t)(close - data) + 2;
			} else {
				while (i < mask->length && data[i] != '\n')
					i++;
			}
			if (marked && !*marked)
				for (size_t pos = start + 2; i - pos >= 6; pos++)
					if (!memcmp(data + pos, "<oicc>", 6)) {
						*marked = 1;
						break;
					}
			if (markers && !comment_tags(node, start, i, block, &state))
				return 0;
		} else if (quote == '"' || quote == '\'') {
			i++;
			while (i < mask->length && data[i] != quote && data[i] != '\n') {
				if (data[i] == '\\')
					i++;
				i++;
			}
			if (i >= mask->length || data[i] != quote)
				return node_diagnostic(node, start, "unclosed C literal");
			i++;
		} else {
			i++;
			continue;
		}
		end = i;
		while (start < end) {
			if (mask->data[start] != '\n' && mask->data[start] != '\r')
				mask->data[start] = ' ';
			start++;
		}
	}
	if (markers && state != 6)
		return node_diagnostic(node, i, "expected one complete oicc/ibn/crt region");
	return 1;
}

static const char *skip_space(const char *p)
{
	while (isspace((unsigned char)*p))
		p++;
	return p;
}

static int member_definitions(const char *data, const char *name)
{
	struct text token = { 0 };
	int count = 0;
	size_t depth = 0;
	const char *p = data;
	if (!text_printf(&token, "em_%s", name))
		return -1;
	for (; *p; p++) {
		if (*p == '{')
			depth++;
		if (*p == '}' && depth)
			depth--;
		if (strncmp(p, "struct", 6))
			continue;
		const char *q = skip_space(p + 6);
		if ((p == data || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
		    q != p + 6 && !strncmp(q, token.data, token.length) &&
		    *skip_space(q + token.length) == '{') {
			if (depth) {
				free(token.data);
				return -1;
			}
			count++;
		}
	}
	free(token.data);
	return count;
}

static int member_token(const char *data, const char *name)
{
	struct text token = { 0 };
	const char *p = data;
	int found = 0;
	if (!text_printf(&token, "em_%s", name))
		return -1;
	while ((p = strstr(p, token.data))) {
		if ((p == data || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
		    !(isalnum((unsigned char)p[token.length]) || p[token.length] == '_')) {
			found = 1;
			break;
		}
		p += token.length;
	}
	free(token.data);
	return found;
}

static int member_macro(char *data, const char *name)
{
	char *p = data;
	while ((p = strchr(p, '#'))) {
		const char *directive = skip_space(p + 1);
		if (!strncmp(directive, "define", 6) || !strncmp(directive, "undef", 5)) {
			char *end = strchr(p, '\n');
			char saved = end ? *end : 0;
			if (end)
				*end = 0;
			int found = member_token(p, name);
			if (end)
				*end = saved;
			if (found)
				return found;
		}
		p++;
	}
	return 0;
}

int node_parse(struct node *node)
{
	const char *basename = node->path, *p;
	size_t length;
	struct text mask = { 0 }, header_path = { 0 }, header_mask = { 0 };
	struct node header = { 0 };
	int ok = 0, definitions;
	for (p = node->path; *p; p++)
		if (*p == '/' || *p == '\\')
			basename = p + 1;
	length = strlen(basename);
	if (length < 3 || strcmp(basename + length - 2, ".c"))
		return node_diagnostic(node, 0, "node input must be a named .c file");
	node->name = malloc(length - 1);
	if (!node->name)
		return 0;
	memcpy(node->name, basename, length - 2);
	node->name[length - 2] = 0;
	if (!node_name_valid(node->name))
		return node_diagnostic(node, 0, "invalid or reserved node name");
	if (!node->source.data && !file_read(node->path, &node->source))
		return node_diagnostic(node, 0, "cannot read regular source file");
	if (!source_mask(node, &mask, 1, NULL))
		goto done;
	memset(mask.data + node->begin, ' ', node->end - node->begin);
	/* Constructors are file-scope definitions. Never accept boundaries inside
	 * a business function or aggregate, even if the tags themselves are valid. */
	size_t depth = 0;
	for (size_t i = 0; i < mask.length; i++) {
		if ((i == node->begin || i == node->end) && depth) {
			node_diagnostic(node, i, "constructor region must be at file scope");
			goto done;
		}
		if (mask.data[i] == '{')
			depth++;
		if (mask.data[i] == '}') {
			if (!depth) {
				node_diagnostic(node, i, "unbalanced C braces");
				goto done;
			}
			depth--;
		}
	}
	if (depth) {
		node_diagnostic(node, mask.length, "unbalanced C braces");
		goto done;
	}
	for (p = mask.data; (p = strchr(p, '#')); p++) {
		const char *directive = skip_space(p + 1);
		if (!strncmp(directive, "if", 2) || !strncmp(directive, "else", 4) ||
		    !strncmp(directive, "elif", 4) || !strncmp(directive, "endif", 5)) {
			node_diagnostic(node, (size_t)(p - mask.data), "conditional source preprocessing is unsupported");
			goto done;
		}
	}
	int bindings = 0;
	for (p = mask.data; (p = strstr(p, "tog_")); p += 4) {
		const char *suffix = p + 4;
		size_t name_length = strlen(node->name), k;
		for (k = 0; k < name_length && suffix[k]; k++)
			if (tolower((unsigned char)suffix[k]) != tolower((unsigned char)node->name[k]))
				break;
		if (k != name_length || isalnum((unsigned char)suffix[k]) || suffix[k] == '_')
			continue;
		const char *line = p;
		while (line > mask.data && line[-1] != '\n')
			line--;
		if (memchr(line, '#', (size_t)(p - line))) {
			node_diagnostic(node, (size_t)(p - mask.data), "macro binding declarations are unsupported");
			goto done;
		}
		const char *q = skip_space(suffix + name_length);
		if (*q != '(' || (p > mask.data && (isalnum((unsigned char)p[-1]) || p[-1] == '_')))
			continue;
		const char *type = p;
		while (type > mask.data && isspace((unsigned char)type[-1]))
			type--;
		if (type - mask.data < 4 || strncmp(type - 4, "void", 4) ||
		    (type - mask.data > 4 &&
		     (isalnum((unsigned char)type[-5]) || type[-5] == '_')))
			continue;
		q = strchr(q, ')');
		if (q && *skip_space(q + 1) == '{') {
			bindings++;
			memcpy(node->name, suffix, name_length);
		}
	}
	if (bindings != 1) {
		node_diagnostic(node, 0, "expected exactly one tog binding definition");
		goto done;
	}
	definitions = member_definitions(mask.data, node->name);
	if (definitions < 0 || definitions > 1) {
		node_diagnostic(node, 0, "duplicate or unsupported member declaration");
		goto done;
	}
	int mentioned = member_token(mask.data, node->name);
	if (member_macro(mask.data, node->name)) {
		node_diagnostic(node, 0, "macro member declarations are unsupported");
		goto done;
	}
	if (!text_append(&header_path, node->path, strlen(node->path) - 1) ||
	    !text_append(&header_path, "h", 1))
		goto done;
	header.path = header_path.data;
	if (file_regular(header.path)) {
		if (!file_read(header.path, &header.source) ||
		    !source_mask(&header, &header_mask, 0, NULL))
			goto done;
		/* Only a conventional outer include guard is supported in headers. */
		int conditionals = 0, endings = 0;
		const char *first = skip_space(header_mask.data);
		const char *guard = NULL;
		size_t guard_length = 0;
		for (p = header_mask.data; (p = strchr(p, '#')); p++) {
			const char *directive = skip_space(p + 1);
			if (!strncmp(directive, "if", 2)) {
				conditionals++;
				if (conditionals != 1 || p != first || strncmp(directive, "ifndef", 6))
					goto ambiguous;
				guard = skip_space(directive + 6);
				const char *end = guard;
				while (isalnum((unsigned char)*end) || *end == '_')
					end++;
				guard_length = (size_t)(end - guard);
				const char *define = skip_space(end);
				if (!guard_length || *define != '#')
					goto ambiguous;
				define = skip_space(define + 1);
				if (strncmp(define, "define", 6))
					goto ambiguous;
				define = skip_space(define + 6);
				if (strncmp(define, guard, guard_length) ||
				    !isspace((unsigned char)define[guard_length]))
					goto ambiguous;
			}
			if (!strncmp(directive, "endif", 5)) {
				endings++;
				if (conditionals != 1 || endings != 1 || *skip_space(directive + 5))
					goto ambiguous;
			}
			if (!strncmp(directive, "else", 4) || !strncmp(directive, "elif", 4))
				goto ambiguous;
		}
		if (conditionals != endings)
			goto ambiguous;
		int header_definitions = member_definitions(header_mask.data, node->name);
		if (header_definitions < 0) {
			node_diagnostic(node, 0, "unsupported member declaration");
			goto done;
		}
		definitions += header_definitions;
		if (member_macro(header_mask.data, node->name))
			goto ambiguous;
		int header_mentioned = member_token(header_mask.data, node->name);
		if (header_mentioned < 0)
			goto done;
		mentioned |= header_mentioned;
	}
	if (mentioned < 0 || definitions < 0 || definitions > 1 || (!definitions && mentioned)) {
		node_diagnostic(node, 0, "duplicate or unsupported member declaration");
		goto done;
	}
	node->has_members = definitions != 0;
	ok = 1;
	goto done;
ambiguous:
	node_diagnostic(node, 0, "conditional member header is unsupported");
done:
	free(mask.data);
	free(header_path.data);
	free(header.source.data);
	free(header_mask.data);
	return ok;
}
