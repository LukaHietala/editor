#include <ncurses.h>
#include "editor.h"

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

void handle_input(struct editor *e)
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
