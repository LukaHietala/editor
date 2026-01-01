#include <ncurses.h>
#include <stdio.h>
#include "kiuru.h"

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

void draw_ui(struct editor *e)
{
	/* Sets the editor windown dimensions */
	getmaxyx(stdscr, e->screen_rows, e->screen_cols);

	if (e->mode == MODE_EXPLORER) {
		draw_explorer(e);
		return;
	}

	update_gutter_width(e);

	/* Navigate to the line at row_offset to start drawing */
	struct line *iter = e->active_buf->head;

	/* Fast forward to row_offset */
	for (int i = 0; i < e->active_buf->row_offset && iter; i++)
		iter = iter->next;

	/* Draw file content on screen */
	for (int y = 0; y < e->screen_rows - 1; y++) {
		/* Move to start of the line and clear to the right */
		move(y, 0);
		clrtoeol();

		/* Add indicators to empty space */
		if (!iter) {
			mvaddch(y, 0, '~');
			continue;
		}

		/* Draw gutter */
		attron(COLOR_PAIR(1));
		mvprintw(y, 0, "%*d ", e->active_buf->gutter_w - 1,
			 iter->lineno);
		attroff(COLOR_PAIR(1));

		/*  Draw text */
		int cur_rx = 0;
		for (int i = 0; i < iter->size; i++) {
			int is_tab = (iter->data[i] == '\t');
			int char_w =
				is_tab ? (TAB_WIDTH - (cur_rx % TAB_WIDTH)) : 1;
			/* Screen width, gutter_w + text with col offset
			 * accounted for */
			int sx = e->active_buf->gutter_w +
				 (cur_rx - e->active_buf->col_offset);

			/* Calculate width first, then skip if off-screen */
			cur_rx += char_w;
			if (sx < e->active_buf->gutter_w)
				continue;
			if (sx >= e->screen_cols)
				break;

			if (is_tab)
				for (int s = 0;
				     s < char_w && (sx + s) < e->screen_cols;
				     s++)
					mvaddch(y, sx + s, ' ');
			else
				mvaddch(y, sx, iter->data[i]);
		}
		iter = iter->next;
	}
	draw_status_bar(e);
}

void init_ncurses(struct editor *e)
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
		use_default_colors();
		/* Gutter pair, gray */
		init_pair(1, COLOR_BRIGHT_BLACK, COLOR_BLACK);
	} else {
		set_message(e, "Warn: No terminal color support");
	}
}