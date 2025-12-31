#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "editor.h"
#include "util.h"

/* Recalculate line numbers starting from a specific line */
static void recalculate_linenos(struct line *start_node, int start_num)
{
	struct line *iter = start_node;
	int num = start_num;
	while (iter) {
		iter->lineno = num++;
		iter = iter->next;
	}
}

/* Free a single line and its data */
static void line_free(struct line *l)
{
	if (l) {
		if (l->data)
			free(l->data);
		free(l);
	}
}

/* Creates a empty buffer */
struct buffer *buffer_new(void)
{
	struct buffer *buf = xcalloc(1, sizeof(struct buffer));

	buf->path[0] = '\0';

	/* Initialize with one empty line */
	struct line *l = xcalloc(1, sizeof(struct line));
	l->data = safe_strdup("");
	l->size = 0;
	l->capacity = 0;
	l->lineno = 1;

	buf->head = buf->tail = buf->current = l;
	buf->line_count = 1;

	return buf;
}

/* Frees buffer and it's lines */
void buffer_free(struct buffer *b)
{
	if (!b)
		return;
	struct line *iter = b->head;
	while (iter) {
		struct line *next = iter->next;
		line_free(iter);
		iter = next;
	}
	free(b);
}

/* Sets active buffer */
void set_active_buffer(struct editor *e, struct buffer *b)
{
	e->active_buf = b;
	e->mode = MODE_NORMAL;
}

/* Loads file to buffer */
void load_file(struct editor *e, const char *path)
{
	/* Check if buffer alreadt exists with the same path, if so set the
	 * active buffer to it. TODO: Canonicalise the path */
	struct buffer *iter = e->buf_head;
	while (iter) {
		if (strcmp(iter->path, path) == 0) {
			set_active_buffer(e, iter);
			return;
		}
		iter = iter->next;
	}

	/* Create new buffer */
	struct buffer *b = buffer_new();
	strncpy(b->path, path, sizeof(b->path) - 1);

	FILE *f = fopen(path, "r");
	if (f) {
		/* Clear the default empty line created in buffer_new */
		line_free(b->head);
		b->head = b->tail = b->current = NULL;
		b->line_count = 0;

		char *line_buf = NULL;
		size_t cap = 0;
		ssize_t len;

		while ((len = getline(&line_buf, &cap, f)) != -1) {
			/* Strip newline logic */
			while (len > 0 && (line_buf[len - 1] == '\n' ||
					   line_buf[len - 1] == '\r')) {
				line_buf[--len] = '\0';
			}

			/* Create new line node */
			struct line *l = xcalloc(1, sizeof(struct line));
			l->data = safe_strdup(
				line_buf); /* safe_strdup is safer than taking
					      ownership of getline buffer */
			l->size = (size_t)len;
			l->capacity = l->size;
			l->lineno = ++b->line_count;
			l->prev = b->tail;

			if (b->tail)
				b->tail->next = l;
			else
				b->head = l;

			b->tail = l;
		}
		free(line_buf);
		fclose(f);
	}

	/* If file was empty or couldn't open, ensure at least one
	 * line */
	if (b->line_count == 0) {
		/* buffer_new logic handles the struct, but we cleared it above,
		 * so re-add */
		struct line *l = xcalloc(1, sizeof(struct line));
		l->data = safe_strdup("");
		l->lineno = 1;
		b->head = b->tail = l;
		b->line_count = 1;
	}

	/* Reset the cursor */
	b->current = b->head;
	b->cx = 0;
	b->cy = 0;

	/* Link to editor */
	if (!e->buf_head) {
		e->buf_head = b;
	} else {
		struct buffer *last = e->buf_head;
		while (last->next)
			last = last->next;
		last->next = b;
		b->prev = last;
	}

	set_active_buffer(e, b);
}

void save_file(struct editor *e)
{
	struct buffer *b = e->active_buf;
	if (!b->path[0])
		return; /* Ignore no name, TODO: ask for filename and the save
			 */

	FILE *f = fopen(b->path, "w");
	if (!f) {
		set_message(e, "Err: %s", strerror(errno));
		return;
	}

	struct line *curr = b->head;
	long bytes = 0;
	while (curr) {
		if (curr->data) {
			fprintf(f, "%s", curr->data);
			bytes += curr->size;
		}
		/* Always write newline (POSIX standard). TODO: Support CRLF for
		 * windows */
		fprintf(f, "\n");
		bytes++;
		curr = curr->next;
	}

	fclose(f);
	set_message(e, "\"%s\" %ldL, %ldB written", b->path, b->line_count,
		    bytes);
}

/* Insert new char to cursor pos */
void insert_char(struct editor *e, int c)
{
	struct line *l = e->active_buf->current;
	if (!l)
		return;

	/* Grow capacity if needed */
	if (l->size + 1 >= l->capacity) {
		size_t new_cap = (l->capacity == 0) ? 16 : l->capacity * 2;
		l->data = xrealloc(l->data, new_cap);
		l->capacity = new_cap;
	}

	/* Shift text right to make room */
	memmove(&l->data[e->active_buf->cx + 1], &l->data[e->active_buf->cx],
		l->size - e->active_buf->cx);

	l->data[e->active_buf->cx] = (char)c;
	l->size++;
	l->data[l->size] = '\0';

	e->active_buf->cx++;
}

/* Creates empty line with newline */
void insert_newline(struct editor *e)
{
	struct buffer *b = e->active_buf;
	struct line *l = b->current;
	if (!l)
		return;

	struct line *new_line = xcalloc(1, sizeof(struct line));

	/* Split text: Copy from cursor to end into new line */
	new_line->data = safe_strdup(&l->data[b->cx]);
	new_line->size = strlen(new_line->data);
	new_line->capacity = new_line->size + 1; /* +1 for safety/null */

	/* Truncate current line */
	l->data[b->cx] = '\0';
	l->size = b->cx;

	/* Link new node */
	new_line->next = l->next;
	new_line->prev = l;
	if (l->next)
		l->next->prev = new_line;
	else
		b->tail = new_line;
	l->next = new_line;

	/* Update buffer state */
	b->line_count++;
	b->current = new_line;
	b->cx = 0;
	b->cy++;

	/* Fix line numbers from here downwards */
	recalculate_linenos(new_line, l->lineno + 1);
}

/* Deletes a character on line on cursor pos. Backspace/DEL */
void delete_char(struct editor *e, int backspace)
{
	struct buffer *b = e->active_buf;
	struct line *l = b->current;
	if (!l)
		return;

	/* If and backspace at start of line (merge with previous) */
	if (backspace && b->cx == 0) {
		if (!l->prev)
			return; /* Ignore if on head*/

		struct line *prev = l->prev;
		size_t old_prev_len = prev->size;

		/* Grow prev buffer to hold current line's data */
		prev->data = xrealloc(prev->data, prev->size + l->size + 1);
		prev->capacity = prev->size + l->size + 1;

		/* Append current line data to prev */
		memcpy(&prev->data[prev->size], l->data, l->size);
		prev->size += l->size;
		prev->data[prev->size] = '\0';

		/* Unlink 'l' (the line is now deleted/empty) */
		prev->next = l->next;
		if (l->next)
			l->next->prev = prev;
		else
			b->tail = prev;

		line_free(l);

		/* Update buffer state */
		b->current = prev;
		b->cx = old_prev_len;
		b->cy--;
		b->line_count--;

		recalculate_linenos(prev->next, prev->lineno + 1);
		return;
	}

	/* If delete and at end of line (merge with next) */
	if (!backspace && b->cx == l->size) {
		if (!l->next)
			return; /*Ignrore if tail */

		struct line *next = l->next;

		/* Grow current buffer to hold next line's data */
		char *new_data = xrealloc(l->data, l->size + next->size + 1);
		l->data = new_data;
		l->capacity = l->size + next->size + 1;

		/* Append next line data to current */
		memcpy(&l->data[l->size], next->data, next->size);
		l->size += next->size;
		l->data[l->size] = '\0';

		/* Unlink 'next' */
		l->next = next->next;
		if (next->next)
			next->next->prev = l;
		else
			b->tail = l;

		line_free(next);

		/* Update buffer */
		b->line_count--;

		recalculate_linenos(l->next, l->lineno + 1);
		return;
	}

	/* Standard char deletion (middle of line) */
	size_t char_pos = backspace ? b->cx - 1 : b->cx;

	/* Boundary check */
	if (backspace && b->cx == 0)
		return;
	if (!backspace && b->cx >= l->size)
		return;

	/* Shift left */
	memmove(&l->data[char_pos], &l->data[char_pos + 1], l->size - char_pos);

	l->size--;
	l->data[l->size] = '\0';

	if (backspace)
		b->cx--;
}
