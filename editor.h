#ifndef EDITOR_H
#define EDITOR_H

#define KEY_ESCAPE 27
#define TAB_WIDTH  8

enum editor_mode {
	MODE_NORMAL,
	MODE_INSERT,
};

/*
 * Using doubly-linked lists as the main data structure for lines, same as GNU
 * Nano, due to its simplicity. Gap buffers and ropes are too complex and
 * unnecessary for small project like this. This method works well with small
 * files
 */
struct line {
	char *data;
	struct line *next;
	struct line *prev;
	int lineno;
};

struct editor {
	struct line *head; /* First line */
	struct line *tail; /* Last line */
	struct line *current; /* Line that cursor is on */

	int cx, cy; /* Cursor coords, cy for easy lineno */
	/*
	 * Offsets for rendering
	 */
	int row_offset;
	int col_offset;

	char filename[256]; /* 256 is usual limit for file systems */
	enum editor_mode mode;
};

#endif
