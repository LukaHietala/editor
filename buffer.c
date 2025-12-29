#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "editor.h"

static void buffer_append_line(struct buffer *b, char *text, size_t capacity)
{
	struct line *l = malloc(sizeof(struct line));
	if (!l)
		return;

	l->data = text;
	l->size = (int)strlen(text);
	/* TODO: change capacity and size to size_t */
	l->capacity = (capacity > 0) ? (int)capacity : l->size;

	l->lineno = (b->tail) ? b->tail->lineno + 1 : 1;
	l->next = NULL;
	l->prev = b->tail;

	if (b->tail)
		b->tail->next = l;
	else
		b->head = l;

	b->tail = l;
	b->line_count++;

	if (!b->current)
		b->current = l;
}

/* Creates a empty buffer */
struct buffer *buffer_new()
{
	struct buffer *buf = calloc(1, sizeof(struct buffer));
	buf->path[0] = '\0';

	buffer_append_line(buf, strdup(""), 0);

	return buf;
}

/* Free a specific buffer */
void buffer_free(struct buffer *b)
{
	struct line *iter = b->head;
	while (iter) {
		struct line *next = iter->next;
		if (iter->data)
			free(iter->data);
		free(iter);
		iter = next;
	}
	free(b);
}

/* Switch active buffer */
void set_active_buffer(struct editor *e, struct buffer *b)
{
	e->active_buf = b;
	e->mode = MODE_NORMAL;
}

void load_file(struct editor *e, const char *path)
{
	/* Check if the buffer already exist */
	struct buffer *existing = e->buf_head;
	while (existing) {
		if (strcmp(existing->path, path) == 0) {
			set_active_buffer(e, existing);
			return;
		}
		existing = existing->next;
	}
	/* Create new buffer */
	struct buffer *b = buffer_new();
	strncpy(b->path, path, sizeof(b->path) - 1);

	FILE *f = fopen(path, "r");
	if (f) {
		/* Clear the default empty line we created in buffer_new */
		free(b->head->data);
		free(b->head);
		b->head = NULL;
		b->tail = NULL;
		b->current = NULL;
		b->line_count = 0;

		char *line = NULL;
		size_t cap = 0;
		ssize_t len;

		while ((len = getline(&line, &cap, f)) != -1) {
			line[strcspn(line, "\r\n")] = '\0';

			buffer_append_line(b, line, cap);

			line = NULL;
			cap = 0;
		}
		free(line);
		fclose(f);
	}

	/* Fallback if file empty */
	if (b->head == NULL) {
		struct line *l = malloc(sizeof(struct line));
		l->data = strdup("");
		l->size = 0;
		l->capacity = 0;
		l->lineno = 1;
		l->next = NULL;
		l->prev = NULL;
		b->head = l;
		b->tail = l;
		b->current = l;
		b->line_count = 1;
	}

	/* Link into editor list */
	if (e->buf_head == NULL) {
		e->buf_head = b;
	} else {
		struct buffer *iter = e->buf_head;
		while (iter->next)
			iter = iter->next;
		iter->next = b;
		b->prev = iter;
	}

	/* Set as active */
	set_active_buffer(e, b);
}

void save_file(struct editor *e)
{
	if (!e->active_buf->path[0]) {
		/* TODO: Handle "No Name" files later */
		return;
	}

	FILE *f = fopen(e->active_buf->path, "w");
	if (!f) {
		/* Failed to open file (permissions, etc) */
		set_message(e, "Err: %s", strerror(errno));
		return;
	}

	struct line *curr = e->active_buf->head;
	long bytes_written = 0;
	while (curr) {
		if (curr->data) {
			fprintf(f, "%s", curr->data);
			bytes_written += curr->size;
		}

		/* * Write a newline
		 * Note: load_file strips newlines, so we MUST restore them
		 * fprintf(..., "\n") ensures POSIX compliant line ending,
		 * LF. If file had CRLF file endings (Windows), the they will be
		 * converted to LF. This is intentional for now. Maybe something
		 * like FORMAT enum on editor should be added, so they would't
		 * change?
		 */
		fprintf(f, "\n");
		bytes_written++;

		curr = curr->next;
	}

	fclose(f);
	set_message(e, "\"%s\" %ldL, %ldB written", e->active_buf->path,
		    e->active_buf->line_count, bytes_written);
}

void insert_char(struct editor *e, int c)
{
	struct line *l = e->active_buf->current;

	/* Check line capacity and grow it (*2) if necessary */
	if (l->size >= l->capacity) {
		l->capacity = (l->capacity == 0) ? 8 : l->capacity * 2;
		l->data = realloc(l->data, l->capacity + 1);
	}

	/* Move text; if cursor is in middle, shift rest of line right,
	 * memmove(dest, src, n) */
	memmove(&l->data[e->active_buf->cx + 1], &l->data[e->active_buf->cx],
		l->size - e->active_buf->cx);

	/* Insert char and update size */
	l->data[e->active_buf->cx] = c;
	l->size++;
	l->data[l->size] = '\0';

	/* Move cursor */
	e->active_buf->cx++;
}

void insert_newline(struct editor *e)
{
	struct line *l = e->active_buf->current;
	if (!l)
		return;

	/* Allocate space for new line */
	struct line *new_line = malloc(sizeof(struct line));
	if (!new_line)
		return;

	/* Handle the text split */
	/* Everything from cursor to end of string goes to the new line */
	char *split_data = &l->data[e->active_buf->cx];
	new_line->data = strdup(split_data);
	new_line->size = strlen(new_line->data);
	new_line->capacity = new_line->size;

	/* Truncate the current line at the cursor position */
	l->data[e->active_buf->cx] = '\0';
	l->size = e->active_buf->cx;

	/* Shrink memory of the old line to fit the new shorter string, this is
	 * not really necessary but optimizes mem usage a little */
	char *tmp = realloc(l->data, l->size + 1);
	if (tmp) {
		l->data = tmp;
		l->capacity = l->size;
	}

	/* Insert new_line into the doubly linked list */
	new_line->prev = l;
	new_line->next = l->next;

	if (l->next) {
		l->next->prev = new_line;
	} else {
		/* If we were at the end of the file, new_line is the new tail
		 */
		e->active_buf->tail = new_line;
	}
	l->next = new_line;

	/* Update Editor State */
	e->active_buf->current = new_line;
	e->active_buf->cy++;
	e->active_buf->cx = 0;
	e->active_buf->line_count++;

	/* Iterate and update lineos after newline*/
	struct line *iter = new_line;
	while (iter) {
		iter->lineno = iter->prev->lineno + 1;
		iter = iter->next;
	}
}

void delete_char(struct editor *e, int backspace)
{
	struct line *l = e->active_buf->current;

	/* Move cursor left first, then delete */
	if (backspace) {
		/* If at the very start of the file, do nothing */
		if (e->active_buf->cx == 0 && e->active_buf->cy == 0)
			return;

		if (e->active_buf->cx > 0) {
			e->active_buf->cx--;
		} else {
			/* Join with previous line */
			struct line *prev = l->prev;
			/* Save old prev line size */
			int old_prev_size = prev->size;
			/* Reallocate previous line to fit current line's data
			 */
			prev->data =
				realloc(prev->data, prev->size + l->size + 1);
			memcpy(&prev->data[prev->size], l->data, l->size);

			/* Make sure prev line is updated */
			prev->size += l->size;
			prev->data[prev->size] = '\0';
			prev->capacity = prev->size;

			/* Update linked list pointers */
			prev->next = l->next;
			if (l->next)
				l->next->prev = prev;
			else
				e->active_buf->tail = prev;

			/* Update editor */
			e->active_buf->current = prev;
			e->active_buf->cy--;
			e->active_buf->cx = old_prev_size;
			e->active_buf->line_count--;

			free(l->data);
			free(l);

			/* Update all linenos */
			struct line *iter = prev->next;
			while (iter) {
				iter->lineno--;
				iter = iter->next;
			}

			return;
		}
	}

	/* If cursor is at the very end of the line, there is nothing to delete
	 */
	if (e->active_buf->cx == l->size) {
		/* Join current line with next */
		if (l->next) {
			struct line *next = l->next;

			/* Create space for merged line */
			l->data = realloc(l->data, l->size + next->size + 1);
			memcpy(&l->data[l->size], next->data, next->size);
			l->size += next->size;
			l->data[l->size] = '\0';
			l->capacity = l->size;

			/* Update linked list */
			l->next = next->next;
			if (next->next)
				next->next->prev = l;
			else
				e->active_buf->tail = l;

			e->active_buf->line_count--;

			free(next->data);
			free(next);

			/* Update lineos */
			struct line *iter = l->next;
			while (iter) {
				iter->lineno--;
				iter = iter->next;
			}
		}
		return;
	}

	/* Shift everything after the cursor one position to the left */
	memmove(&l->data[e->active_buf->cx], &l->data[e->active_buf->cx + 1],
		l->size - e->active_buf->cx);

	l->size--;
	l->data[l->size] = '\0';
}
