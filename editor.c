#include <stdio.h>
#include <ncurses.h>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	initscr();
	raw();
	keypad(stdscr, TRUE);
	noecho();
	set_escdelay(0);
	printw("Hello!");
	refresh();
	getch();
	endwin();

	return 0;
}
