#include <ncurses.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "kiuru.h"
#include "util.h"

static const char *help_text[] = { "Coming later" };
static const int help_line_count = sizeof(help_text) / sizeof(help_text[0]);

void show_help_page()
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

void open_man_page(struct editor *e)
{
	char *word = get_word_under_cursor(e);

	if (word) {
		/* Check if man page exists */
		char check_cmd[256];
		snprintf(check_cmd, sizeof(check_cmd),
			 "man -w %s > /dev/null 2>&1", word);

		if (system(check_cmd) != 0) {
			set_message(e, "No manual entry for '%s'", word);
			free(word);
			return;
		}

		/* Save terminal state */
		def_prog_mode();
		/* Temporarily exit ncurses, this causes a little flash, but its
		 * not significant enough. Technically, there could be a temp
		 * buffer before entering man page, but it might be unnecessary
		 */
		endwin();

		char run_cmd[256];
		snprintf(run_cmd, sizeof(run_cmd), "man %s", word);
		system(run_cmd);

		/* Restore terminal state */
		reset_prog_mode();
		refresh();

		free(word);
	} else {
		set_message(e, "Not a valid word");
	}
}
