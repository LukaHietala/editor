#include <ncurses.h>
#include <stdlib.h>
#include "editor.h"

void quit_editor(struct editor *e, int status)
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
