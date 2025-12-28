#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ncurses.h>
#include "editor.h"

const char *help_text[] = { "Coming later" };
int help_line_count = sizeof(help_text) / sizeof(help_text[0]);

/* Sets status bar message */
static void set_message(struct editor *e, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(e->message, sizeof(e->message), fmt, ap);
	va_end(ap);
}

static void free_editor(struct editor *e)
{
	struct line *curr = e->head;
	/* Frees every line */
	while (curr) {
		struct line *next = curr->next;
		if (curr->data)
			free(curr->data);
		free(curr);
		curr = next;
	}
}

static void quit_editor(struct editor *e, int status)
{
	endwin();
	free_editor(e);
	exit(status);
}

static void append_line(struct editor *e, char *text)
{
	struct line *node = malloc(sizeof(struct line));

	node->data = text;
	node->size = strlen(text);
	node->capacity = node->size;
	node->lineno = (e->tail) ? e->tail->lineno + 1 : 1;
	node->next = NULL;
	node->prev = e->tail;

	if (e->tail)
		e->tail->next = node;
	else
		e->head = node;

	e->tail = node;
	e->line_count++;

	/* If this is the first line, set current to it */
	if (!e->current)
		e->current = node;
}

static void load_file(struct editor *e, const char *path)
{
	strncpy(e->path, path, sizeof(e->path) - 1);
	FILE *f = fopen(path, "r");

	e->head = NULL;
	e->tail = NULL;
	e->current = NULL;
	e->line_count = 0;

	if (f) {
		char *line = NULL;
		size_t cap = 0;
		ssize_t len;

		while ((len = getline(&line, &cap, f)) != -1) {
			/* Strip newline/CR */
			line[strcspn(line, "\r\n")] = '\0';
			append_line(e, line);

			line = NULL;
			cap = 0;
		}
		free(line);
		fclose(f);
	}

	/* Make sure editor is never empty */
	if (e->head == NULL)
		append_line(e, strdup(""));

	/* Ensure current and cursor is set to start */
	e->current = e->head;
	e->cx = 0;
	e->cy = 0;
}

static void save_file(struct editor *e)
{
	if (!e->path[0]) {
		/* TODO: Handle "No Name" files later */
		return;
	}

	FILE *f = fopen(e->path, "w");
	if (!f) {
		/* Failed to open file (permissions, etc) */
		set_message(e, "Err: %s", strerror(errno));
		return;
	}

	struct line *curr = e->head;
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
	set_message(e, "\"%s\" %ldL, %ldB written", e->path, e->line_count,
		    bytes_written);
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
			 (e->path[0]) ? e->path : "[No Name]", e->cy + 1,
			 e->line_count, e->cx + 1,
			 cx_to_rx(e->current, e->cx) + 1);
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
	e->gutter_w = snprintf(buf, sizeof(buf), "%d", e->line_count) + 1;
}

static void draw_ui(struct editor *e)
{
	/* Sets the editor windown dimensions */
	getmaxyx(stdscr, e->screen_rows, e->screen_cols);

	update_gutter_width(e);

	/* Navigate to the line at row_offset to start drawing */
	struct line *iter = e->head;
	int current_row = 0;

	/* Fast forward to row_offset */
	while (iter != NULL && current_row < e->row_offset) {
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
			mvprintw(y, 0, "%*d ", e->gutter_w - 1, iter->lineno);
			attroff(COLOR_PAIR(1));

			/* Draw a line accounting for current editor column
			 * offset */
			if (iter->size > e->col_offset) {
				int max_chars = e->screen_cols - e->gutter_w;
				mvaddnstr(y, e->gutter_w,
					  &iter->data[e->col_offset],
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
	if (!e->current)
		return;

	/* Bounds */
	int row_len = e->current->size;

	switch (key) {
	case KEY_LEFT:
	case 'h':
		if (e->cx > 0)
			e->cx--;
		break;
	case KEY_RIGHT:
	case 'l':
		if (e->cx < row_len)
			e->cx++;
		break;
	case KEY_UP:
	case 'k':
		if (e->current->prev) {
			e->cy--;
			e->current = e->current->prev;
		}
		break;
	case KEY_DOWN:
	case 'j':
	case KEY_RETURN: /* Keycode 10 and 13 */
		if (e->current->next) {
			e->cy++;
			e->current = e->current->next;
		}
		break;
	case KEY_PPAGE: /* Page up */
		/* Move up by the number of rows visible on screen, -1 due to
		 * status bar taking one space, TODO: optimize, use lineos */
		for (int i = 0; i < e->screen_rows - 1; i++) {
			if (e->current->prev) {
				e->cy--;
				e->current = e->current->prev;
			}
		}
		break;
	case KEY_NPAGE: /* Page down */
		/* Move down by the number of rows visible on screen, -1 due to
		 * status bar taking one space, TODO: optimize, use linenos */
		for (int i = 0; i < e->screen_rows - 1; i++) {
			if (e->current->next) {
				e->cy++;
				e->current = e->current->next;
			}
		}
		break;
	case 'g': /* Jump to head */
		/* Check for the second 'g' */
		if (getch() == 'g') {
			e->current = e->head;
			e->cy = 0;
			e->cx = 0;
		}
		break;

	case 'G': /* Jump to tail */
		e->current = e->tail;
		e->cy = e->line_count - 1;
		e->cx = 0;
		break;
	}

	/* Snap cursor to shorter line length */
	row_len = e->current->size;
	if (e->cx > row_len)
		e->cx = row_len;
}

static void insert_char(struct editor *e, int c)
{
	struct line *l = e->current;

	/* Check line capacity and grow it (*2) if necessary */
	if (l->size >= l->capacity) {
		l->capacity = (l->capacity == 0) ? 8 : l->capacity * 2;
		l->data = realloc(l->data, l->capacity + 1);
	}

	/* Move text; if cursor is in middle, shift rest of line right,
	 * memmove(dest, src, n) */
	memmove(&l->data[e->cx + 1], &l->data[e->cx], l->size - e->cx);

	/* Insert char and update size */
	l->data[e->cx] = c;
	l->size++;
	l->data[l->size] = '\0';

	/* Move cursor */
	e->cx++;
}

static void delete_char(struct editor *e, int backspace)
{
	struct line *l = e->current;

	/* Move cursor left first, then delete */
	if (backspace) {
		if (e->cx > 0) {
			e->cx--;
		} else {
			/* TODO: Handle joining with previous line */
			return;
		}
	}

	/* If cursor is at the very end of the line, there is nothing to delete
	 */
	if (e->cx == l->size) {
		/* TODO: Handle joining with next line */
		return;
	}

	/* Shift everything after the cursor one position to the left */
	memmove(&l->data[e->cx], &l->data[e->cx + 1], l->size - e->cx);

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
		default:
			if (c >= 32 && c <= 126)
				insert_char(e, c);
			break;
		}
	}

	/* Reserve space for status bar */
	int h_limit = e->screen_rows - 1;

	/* Row scrolling */
	if (e->cy < e->row_offset)
		e->row_offset = e->cy;
	if (e->cy >= e->row_offset + h_limit)
		e->row_offset = e->cy - h_limit + 1;

	/*
	 * Col scrolling
	 * TODO: Some padding, like opt.scrolloff in vim
	 */
	int rx = cx_to_rx(e->current, e->cx);

	if (rx < e->col_offset)
		e->col_offset = rx;
	if (rx >= e->col_offset + e->screen_cols)
		e->col_offset = rx - e->screen_cols + 1;
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
	/*
	 * TODO: If not provided open a empty buffer and when writing take the
	 * filename as input.
	 */
	if (argc < 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	struct editor e = { 0 };
	e.mode = MODE_NORMAL;

	init_ncurses(&e);
	load_file(&e, argv[1]);

	while (1) {
		draw_ui(&e);

		int rx = cx_to_rx(e.current, e.cx);
		/* RX - rendered x, is like CX (cursor x pos), but it's only
		 * meant for rendering. For example it turns tab (cx = 1) to (rx
		 * = TAB_WIDTH). TAB_WIDTH is only 8 for now. */
		move(e.cy - e.row_offset, (rx - e.col_offset) + e.gutter_w);
		refresh();
		handle_input(&e);
	}

	return 0;
}
