#include <ncurses.h>
#include <string.h>
#include "editor.h"

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
