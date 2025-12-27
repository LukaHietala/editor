#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include "editor.h"

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

	/*
	 * Very much like neovim's but on one line
	 * [NORMAL] | <path> | L: <current_line>/<last_line> C:
	 * <cursor_x>-<rendered_x>
	 */
	mvprintw(e->screen_rows - 1, 0, " [%s] | %s | L: %d/%d C: %d-%d",
		 (e->mode == MODE_NORMAL) ? "NORMAL" : "INSERT", e->path,
		 e->cy + 1, e->line_count, e->cx + 1,
		 cx_to_rx(e->current, e->cx) + 1);

	/* Fill the rest with spaces */
	int current_x = getcurx(stdscr);
	for (; current_x < e->screen_cols; current_x++)
		addch(' ');

	attroff(A_REVERSE);
}

static void draw_ui(struct editor *e)
{
	/* Sets the editor windown dimensions */
	getmaxyx(stdscr, e->screen_rows, e->screen_cols);

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
			int len = iter->size;
			/* Draw a line accounting for current editor column
			 * offset */
			if (len > e->col_offset) {
				mvaddnstr(y, 0, &iter->data[e->col_offset],
					  e->screen_cols);
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
	case KEY_RETURN:
		if (e->current->next) {
			e->cy++;
			e->current = e->current->next;
		}
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
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
		case 'h':
		case 'j':
		case 'k':
		case 'l':
		case KEY_RETURN:
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
			move_cursor(e, c);
			break;
		case KEY_BACKSPACE:
		case KEY_DL:
			/* TODO: Char deletion */
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

static void init_ncurses()
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

	init_ncurses();
	load_file(&e, argv[1]);

	while (1) {
		draw_ui(&e);

		int rx = cx_to_rx(e.current, e.cx);
		/* RX - rendered x, is like CX (cursor x pos), but it's only
		 * meant for rendering. For example it turns tab (cx = 1) to (rx
		 * = TAB_WIDTH). TAB_WIDTH is only 8 for now. */
		move(e.cy - e.row_offset, rx - e.col_offset);

		refresh();
		handle_input(&e);
	}

	return 0;
}
