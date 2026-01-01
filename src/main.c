#include <ncurses.h>
#include <stdlib.h>
#include "kiuru.h"

void quit_editor(struct editor *e, int status)
{
	endwin();
	struct buffer *iter = e->buf_head;
	while (iter) {
		struct buffer *next = iter->next;
		buffer_free(iter);
		iter = next;
	}

	exit(status);
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
		/* Ensure screen_x accounts for gutter and scroll, but never
		 * enters gutter space */
		int screen_x = (rx - e.active_buf->col_offset) +
			       e.active_buf->gutter_w;
		move(e.active_buf->cy - e.active_buf->row_offset, screen_x);
		refresh();
		handle_input(&e);
	}

	return 0;
}
