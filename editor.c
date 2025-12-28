#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ncurses.h>
#include "editor.h"

const char *help_text[] = { "Coming later" };
int help_line_count = sizeof(help_text) / sizeof(help_text[0]);

void buffer_append_line(struct buffer *b, char *text, size_t capacity)
{
	struct line *node = malloc(sizeof(struct line));
	if (!node)
		return;

	node->data = text;
	node->size = (int)strlen(text);
	/* TODO: change capacity and size to size_t */
	node->capacity = (capacity > 0) ? (int)capacity : node->size;

	node->lineno = (b->tail) ? b->tail->lineno + 1 : 1;
	node->next = NULL;
	node->prev = b->tail;

	if (b->tail)
		b->tail->next = node;
	else
		b->head = node;

	b->tail = node;
	b->line_count++;

	if (!b->current)
		b->current = node;
}

/* Creates a empty buffer */
static struct buffer *buffer_new()
{
	struct buffer *buf = calloc(1, sizeof(struct buffer));
	buf->path[0] = '\0';

	buffer_append_line(buf, strdup(""), 0);

	return buf;
}

/* Free a specific buffer */
static void buffer_free(struct buffer *b)
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
static void set_active_buffer(struct editor *e, struct buffer *b)
{
	e->active_buf = b;
	e->mode = MODE_NORMAL;
}

/* Sets status bar message */
static void set_message(struct editor *e, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(e->message, sizeof(e->message), fmt, ap);
	va_end(ap);
}

static void quit_editor(struct editor *e, int status)
{
	endwin();
	/* Free all buffers */
	struct buffer *iter = e->buf_head;
	while (iter) {
		struct buffer *next = iter->next;
		buffer_free(iter);
		iter = next;
	}

	exit(status);
}

static void load_file(struct editor *e, const char *path)
{
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

static void save_file(struct editor *e)
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

/* Converts real mouse pos to rendered mouse pos */
static int cx_to_rx(struct line *line, int cx)
{
	if (!line)
		return 0;

	int rx = 0;
	for (int i = 0; i < cx && i < line->size; i++)
		/* Render TAB_WIDTH amount of spaces */
		if (line->data[i] == '\t')
			rx += TAB_WIDTH - (rx % TAB_WIDTH);
		else
			rx++;
	return rx;
}

static void draw_status_bar(struct editor *e)
{
	attron(A_REVERSE);
	int width = e->screen_cols;

	if (e->message[0] != '\0') {
		mvprintw(e->screen_rows - 1, 0, "%s", e->message);

		/* Clear the message immediately so it disappears on next render
		 */
		e->message[0] = '\0';
	} else {
		mvprintw(e->screen_rows - 1, 0,
			 " [%s] | %s | L: %d/%d C: %d-%d",
			 (e->mode == MODE_NORMAL) ? "NORMAL" : "INSERT",
			 (e->active_buf->path[0]) ? e->active_buf->path :
						    "[No Name]",
			 e->active_buf->cy + 1, e->active_buf->line_count,
			 e->active_buf->cx + 1,
			 cx_to_rx(e->active_buf->current, e->active_buf->cx) +
				 1);
	}

	/* Fill the rest of the line with whitespace */
	int current_x = getcurx(stdscr);
	while (current_x < width) {
		addch(' ');
		current_x++;
	}

	attroff(A_REVERSE);
}

static void update_gutter_width(struct editor *e)
{
	char buf[32];
	/* Chars needed for digits + 1 space padding */
	e->active_buf->gutter_w =
		snprintf(buf, sizeof(buf), "%d", e->active_buf->line_count) + 1;
}

static void draw_ui(struct editor *e)
{
	/* Sets the editor windown dimensions */
	getmaxyx(stdscr, e->screen_rows, e->screen_cols);

	update_gutter_width(e);

	/* Navigate to the line at row_offset to start drawing */
	struct line *iter = e->active_buf->head;
	int current_row = 0;

	/* Fast forward to row_offset */
	while (iter != NULL && current_row < e->active_buf->row_offset) {
		iter = iter->next;
		current_row++;
	}

	/* Draw file content on screen */
	for (int y = 0; y < e->screen_rows - 1; y++) {
		/* Move to start of the line and clear to the right */
		move(y, 0);
		clrtoeol();

		if (iter != NULL) {
			/* Draw gutter */
			attron(COLOR_PAIR(1));
			mvprintw(y, 0, "%*d ", e->active_buf->gutter_w - 1,
				 iter->lineno);
			attroff(COLOR_PAIR(1));

			/* Draw a line accounting for current editor column
			 * offset */
			if (iter->size > e->active_buf->col_offset) {
				int max_chars = e->screen_cols -
						e->active_buf->gutter_w;
				mvaddnstr(
					y, e->active_buf->gutter_w,
					&iter->data[e->active_buf->col_offset],
					max_chars);
			}
			iter = iter->next;
		} else {
			/* Add indicators to empty space */
			mvaddch(y, 0, '~');
		}
	}

	draw_status_bar(e);
}

static void move_cursor(struct editor *e, int key)
{
	/* Make sure that there is current line before trying to jump to another
	 * lines */
	if (!e->active_buf->current)
		return;

	/* Bounds */
	int row_len = e->active_buf->current->size;

	switch (key) {
	case KEY_LEFT:
	case 'h':
		if (e->active_buf->cx > 0)
			e->active_buf->cx--;
		break;
	case KEY_RIGHT:
	case 'l':
		if (e->active_buf->cx < row_len)
			e->active_buf->cx++;
		break;
	case KEY_UP:
	case 'k':
		if (e->active_buf->current->prev) {
			e->active_buf->cy--;
			e->active_buf->current = e->active_buf->current->prev;
		}
		break;
	case KEY_DOWN:
	case 'j':
	case KEY_RETURN: /* Keycode 10 and 13 */
		if (e->active_buf->current->next) {
			e->active_buf->cy++;
			e->active_buf->current = e->active_buf->current->next;
		}
		break;
	case KEY_PPAGE: /* Page up */
		/* Move up by the number of rows visible on screen, -1 due to
		 * status bar taking one space, TODO: optimize, use lineos */
		for (int i = 0; i < e->screen_rows - 1; i++) {
			if (e->active_buf->current->prev) {
				e->active_buf->cy--;
				e->active_buf->current =
					e->active_buf->current->prev;
			}
		}
		break;
	case KEY_NPAGE: /* Page down */
		/* Move down by the number of rows visible on screen, -1 due to
		 * status bar taking one space, TODO: optimize, use linenos */
		for (int i = 0; i < e->screen_rows - 1; i++) {
			if (e->active_buf->current->next) {
				e->active_buf->cy++;
				e->active_buf->current =
					e->active_buf->current->next;
			}
		}
		break;
	case 'g': /* Jump to head */
		/* Check for the second 'g' */
		if (getch() == 'g') {
			e->active_buf->current = e->active_buf->head;
			e->active_buf->cy = 0;
			e->active_buf->cx = 0;
		}
		break;

	case 'G': /* Jump to tail */
		e->active_buf->current = e->active_buf->tail;
		e->active_buf->cy = e->active_buf->line_count - 1;
		e->active_buf->cx = 0;
		break;
	}

	/* Snap cursor to shorter line length */
	row_len = e->active_buf->current->size;
	if (e->active_buf->cx > row_len)
		e->active_buf->cx = row_len;
}

static void insert_char(struct editor *e, int c)
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

static void insert_newline(struct editor *e)
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

static void delete_char(struct editor *e, int backspace)
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

static void show_help_page()
{
	int offset = 0; /* Scroll pos */
	int key;

	while (1) {
		erase();
		int rows, cols;
		getmaxyx(stdscr, rows, cols);

		/* Draw sticky title bar */
		attron(A_REVERSE);
		mvhline(0, 0, ' ', cols);
		char *title = "The manual";
		/* Center the title text */
		int title_x = (cols / 2) - (strlen(title) / 2);
		mvprintw(0, title_x, "%s", title);
		attroff(A_REVERSE);

		/* Draw scrollable content */
		/* We start drawing from row 1 to leave row 0 for the sticky
		 * title */
		for (int i = 0; i < rows - 1; i++) {
			int line_idx = i + offset;
			if (line_idx < help_line_count) {
				mvaddnstr(i + 1, 2, help_text[line_idx],
					  cols - 4);
			}
		}

		refresh();

		/* Handle input */
		key = getch();
		if (key == KEY_ESCAPE || key == 'q') {
			break;
		} else if (key == KEY_DOWN || key == 'j') {
			if (offset < help_line_count - (rows - 1))
				offset++;
		} else if (key == KEY_UP || key == 'k') {
			if (offset > 0)
				offset--;
		}
	}

	erase();
}

static void handle_input(struct editor *e)
{
	int c = getch();

	if (e->mode == MODE_NORMAL) {
		switch (c) {
		case 'i':
			e->mode = MODE_INSERT;
			break;
		case 'q':
			quit_editor(e, 0);
			break;
		case 'w':
			save_file(e);
			break;
		case 'H': /* TODO: make long command */
			show_help_page();
			break;
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		case 'h':
		case 'j':
		case 'k':
		case 'l':
		/* gg - Jump to head */
		case 'g':
		/* Jump to tail */
		case 'G':
		case KEY_RETURN:
		case KEY_PPAGE:
		case KEY_NPAGE:
			move_cursor(e, c);
			break;
		case ']': /* Next buffer */
			if (e->active_buf->next)
				set_active_buffer(e, e->active_buf->next);
			break;
		case '[': /* Prev buffer */
			if (e->active_buf->prev)
				set_active_buffer(e, e->active_buf->prev);
			break;
		}
	} else { /* MODE_INSERT */
		switch (c) {
		case KEY_ESCAPE:
			e->mode = MODE_NORMAL;
			break;
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_PPAGE:
		case KEY_NPAGE:
			move_cursor(e, c);
			break;
		case KEY_BACKSPACE:
			delete_char(e, 1); /* 1 means backspace */
			break;
		case KEY_DC:
			delete_char(e, 0); /* 0 means DEL */
			break;
		case KEY_RETURN:
			insert_newline(e);
			break;
		default:
			if (c >= 32 && c <= 126)
				insert_char(e, c);
			break;
		}
	}

	/* Reserve space for status bar */
	int h_limit = e->screen_rows - 1;

	/* Row scrolling */
	if (e->active_buf->cy < e->active_buf->row_offset)
		e->active_buf->row_offset = e->active_buf->cy;
	if (e->active_buf->cy >= e->active_buf->row_offset + h_limit)
		e->active_buf->row_offset = e->active_buf->cy - h_limit + 1;

	/*
	 * Col scrolling
	 * TODO: Some padding, like opt.scrolloff in vim
	 */
	int rx = cx_to_rx(e->active_buf->current, e->active_buf->cx);

	if (rx < e->active_buf->col_offset)
		e->active_buf->col_offset = rx;
	if (rx >= e->active_buf->col_offset + e->screen_cols)
		e->active_buf->col_offset = rx - e->screen_cols + 1;
}

static void init_ncurses(struct editor *e)
{
	/* Initialise ncurses */
	initscr();
	/* Switch terminal to raw mode so every character goes through
	 * uninterpreted, instead of generating signals */
	raw();
	/* Enable CR -> NL translation */
	nl();
	/* Enable capture of special keys (arrows, function keys), so
	 * handle_input() can read them. Does not include ESC so defined
	 * seperatly. This due to some historical stuff from Curses */
	keypad(stdscr, TRUE);
	/* Prevents ncurses from echoing typed keys, handled manually */
	noecho();
	/* By default Ncurses has delay for ESC. Leftovers from Curses as well
	 */
	set_escdelay(0);

	/* Check and init colors, using only 16 bit colors to cover the biggest
	 * range of terminal emulators. Maybe moving to 256 bit in the future or
	 * even to TrueColor? And keeping 16 as fallback ofcourse */
	if (has_colors()) {
		start_color();
		/* Gutter pair, gray */
		init_pair(1, COLOR_BRIGHT_BLACK, COLOR_BLACK);
	} else {
		/* Are we in the 1970s */
		set_message(e, "Warn: No terminal color support");
	}
}

int main(int argc, char *argv[])
{
	struct editor e = { 0 };
	e.mode = MODE_NORMAL;

	init_ncurses(&e);
	if (argc >= 2) {
		/* Load all provided files */
		for (int i = 1; i < argc; i++)
			load_file(&e, argv[i]);
	} else {
		/* No file provided? Create a "No Name" buffer */
		struct buffer *b = buffer_new();
		e.buf_head = b;
		set_active_buffer(&e, b);
	}

	while (1) {
		draw_ui(&e);

		int rx = cx_to_rx(e.active_buf->current, e.active_buf->cx);
		/* RX - rendered x, is like CX (cursor x pos), but it's only
		 * meant for rendering. For example it turns tab (cx = 1) to (rx
		 * = TAB_WIDTH). TAB_WIDTH is only 8 for now. */
		move(e.active_buf->cy - e.active_buf->row_offset,
		     (rx - e.active_buf->col_offset) + e.active_buf->gutter_w);
		refresh();
		handle_input(&e);
	}

	return 0;
}
