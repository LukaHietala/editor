#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "kiuru.h"
#include "util.h"

/* Converts real mouse pos to rendered mouse pos */
int cx_to_rx(struct line *line, int cx)
{
	if (!line)
		return 0;
	int rx = 0;
	for (int i = 0; i < cx && i < line->size; i++)
		if (line->data[i] == '\t')
			rx += TAB_WIDTH - (rx % TAB_WIDTH);
		else
			rx++;
	return rx;
}

/* Sets status bar message */
void set_message(struct editor *e, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(e->message, sizeof(e->message), fmt, ap);
	va_end(ap);
}

static int is_word_char(char c)
{
	return isalnum(c) || c == '_';
}

char *get_word_under_cursor(struct editor *e)
{
	struct line *l = e->active_buf->current;
	if (!l || l->size == 0)
		return NULL;

	int cx = e->active_buf->cx;
	/* If cursor is at the end of the line (on the \0), move back one */
	if (cx >= l->size && cx > 0)
		cx--;

	/* Find start of word */
	int start = cx;
	while (start > 0 && is_word_char(l->data[start - 1]))
		start--;

	/* Find end of word */
	int end = cx;
	while (end < l->size && is_word_char(l->data[end]))
		end++;

	int len = end - start;
	if (len <= 0)
		return NULL;

	char *word = xmalloc(len + 1);
	memcpy(word, &l->data[start], len);
	word[len] = '\0';
	return word;
}

void die(const char *err, ...)
{
	char msg[4096];
	va_list params;
	va_start(params, err);
	vsnprintf(msg, sizeof(msg), err, params);
	fprintf(stderr, "%s\n", msg);
	va_end(params);
	exit(1);
}

void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size > 0)
		die("Out of memory (malloc failed for %zu bytes)\n", size);
	return ptr;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);
	if (ptr == NULL && nmemb > 0 && size > 0)
		die("Out of memory (calloc failed)\n");
	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	void *new_ptr = realloc(ptr, size);
	if (new_ptr == NULL && size > 0)
		die("Out of memory (realloc failed for %zu bytes)\n", size);
	return new_ptr;
}

char *xstrdup(const char *s)
{
	if (!s)
		return NULL;
	char *new_str = strdup(s);
	if (!new_str)
		die("Out of memory (strdup failed)\n");
	return new_str;
}
