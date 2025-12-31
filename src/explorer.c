#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "editor.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

/* Frees file list */
static void free_file_list(struct editor *e)
{
	if (e->file_list) {
		for (int i = 0; i < e->file_count; i++)
			free(e->file_list[i]);
		free(e->file_list);
		e->file_list = NULL;
	}
	e->file_count = 0;
}

static int file_select(const struct dirent *entry)
{
	/* Filter out current dir '.' but keep '..' */
	return (strcmp(entry->d_name, ".") != 0);
}

void open_explorer(struct editor *e, const char *path)
{
	/* If path is provided, change dir, otherwise refresh current */
	if (path) {
		if (chdir(path) != 0) {
			set_message(e, "Err: Cannot access %s", path);
			return;
		}
	}

	/* Update CWD string */
	if (!getcwd(e->cwd, sizeof(e->cwd)))
		strcpy(e->cwd, "Unknown");

	free_file_list(e);

	/* Scan directory
	 * alphasort comes with dirent.h usually, otherwise use NULL for no sort
	 */
	/* TODO: Make sort custom */
	e->file_count = scandir(".", &e->file_list, file_select, alphasort);

	if (e->file_count < 0) {
		set_message(e, "Err: Failed to read dir");
		e->file_count = 0;
	}

	/* Hide cursor */
	curs_set(0);

	e->mode = MODE_EXPLORER;
	e->expl_cy = 0;
	e->expl_offset = 0;
}

void draw_explorer(struct editor *e)
{
	erase();

	int rows = e->screen_rows;
	int cols = e->screen_cols;

	/* Draw title */
	attron(A_REVERSE | A_BOLD);
	mvprintw(0, 0, " File Explorer: %s ", e->cwd);
	/* Fill rest of line */
	for (int i = getcurx(stdscr); i < cols; i++)
		addch(' ');
	attroff(A_REVERSE | A_BOLD);

	/* Draw files */
	for (int y = 1; y < rows - 1; y++) {
		int list_idx = (y - 1) + e->expl_offset;

		move(y, 0);
		clrtoeol();

		if (list_idx < e->file_count) {
			struct dirent *dp = e->file_list[list_idx];
			int is_selected = (list_idx == e->expl_cy);

			/* Check if directory */
			struct stat sb;
			stat(dp->d_name, &sb);
			int is_dir = S_ISDIR(sb.st_mode);

			if (is_selected)
				attron(A_REVERSE);

			/* Add slash to dirs */
			if (is_dir)
				attron(COLOR_PAIR(1) | A_BOLD);

			mvprintw(y, 1, "%s%s", dp->d_name, is_dir ? "/" : "");

			if (is_dir)
				attroff(COLOR_PAIR(1) | A_BOLD);
			if (is_selected)
				attroff(A_REVERSE);
		}
	}
}

void handle_explorer_input(struct editor *e)
{
	int c = getch();

	switch (c) {
	case 'q': /* Quit explorer, return to normal if possible */
		e->mode = MODE_NORMAL;
		curs_set(1);
		break;

	case 'j':
	case KEY_DOWN:
		if (e->expl_cy < e->file_count - 1) {
			e->expl_cy++;
			/* Scroll down if needed */
			if (e->expl_cy - e->expl_offset >= e->screen_rows - 2)
				e->expl_offset++;
		}
		break;

	case 'k':
	case KEY_UP:
		if (e->expl_cy > 0) {
			e->expl_cy--;
			/* Scroll up if needed */
			if (e->expl_cy < e->expl_offset)
				e->expl_offset--;
		}
		break;

	case KEY_RETURN: {
		if (e->file_count == 0)
			break;

		struct dirent *dp = e->file_list[e->expl_cy];
		struct stat sb;

		if (stat(dp->d_name, &sb) == 0 && S_ISDIR(sb.st_mode)) {
			/* It's a directory, enter it */
			open_explorer(e, dp->d_name);
		} else {
			/* It's a file, open it */
			char full_path[PATH_MAX];

			/* Construct absolute path */
			snprintf(full_path, sizeof(full_path), "%s/%s", e->cwd,
				 dp->d_name);

			load_file(e, full_path);
			e->mode = MODE_NORMAL;
			/* Set cursor back to normal */
			curs_set(1);

			/* Clean up */
			free_file_list(e);
		}
		break;
	}
	}
}
