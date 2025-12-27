#ifndef EDITOR_H
#define EDITOR_H

#define KEY_ESCAPE 27
#define TAB_WIDTH  8
#define PATH_MAX   4096 /* PATH_MAX from linux/limits.h*/

enum editor_mode {
	MODE_NORMAL,
	MODE_INSERT,
};

/*
 * Using simple linked lists for lines, not fancy gap buffers like Emacs or
 * ropes like VS Code
 */
struct line {
	char *data;
	int size;
	int capacity;
	struct line *next;
	struct line *prev;
	int lineno;
};

struct editor {
	/*
	 * TODO: Create buffer system
	 */
	struct line *head;
	struct line *tail;
	struct line *current;

	int line_count;

	/*
	 * Cursor coords
	 */
	int cx, cy;
	/*
	 * Offsets for rendering
	 */
	int row_offset;
	int col_offset;
	/*
	 * Terminal properties
	 */
	int screen_rows; /* x, getmaxyx */
	int screen_cols; /* y, getmaxyx */

	char path[PATH_MAX];
	enum editor_mode mode;
};

#endif
