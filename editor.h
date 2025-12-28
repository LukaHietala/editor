#ifndef EDITOR_H
#define EDITOR_H

#include <limits.h>
#include <stdarg.h>

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

/* Keys that are not provided by Ncurses */
#define KEY_ESCAPE 27
#define KEY_RETURN                                                            \
	10 /* using LF, ncurses automatically translates 13 to 10. Using nl() \
	    */

#define TAB_WIDTH 8

/* Colors,
 * https://wiki.gentoo.org/wiki/Terminal_emulator/Colors */

/* Gray */
#define COLOR_BRIGHT_BLACK 8

enum editor_mode {
	MODE_NORMAL,
	MODE_INSERT,
};

struct line {
	char *data;
	/* Line text */
	int size;
	/* Line size */
	int capacity;
	/* Max line capacity, grown if necessary */
	int lineno;
	/* Line nro */
	struct line *next;
	struct line *prev;
};

struct buffer {
	/* Path to file */
	char path[PATH_MAX];

	/* First line */
	struct line *head;
	/* Last line */
	struct line *tail;
	/* Current line (where cursor sits) */
	struct line *current;
	int line_count;

	/* Cursor location */
	int cx, cy;
	/* Offset from head line, for vertical scroll */
	int row_offset;
	/* Offset from beginning of a line, for horizontal scroll */
	int col_offset;
	/* Line gutter width */
	int gutter_w;

	struct buffer *next;
	struct buffer *prev;
};

struct editor {
	/* Start of buffer list */
	struct buffer *buf_head;
	/* Currently active buffer */
	struct buffer *active_buf;

	/* Terminal width */
	int screen_rows;
	/* Terminal height */
	int screen_cols;

	/* Status bar message, 80 bytes is resonable for most terminals */
	char message[80];

	enum editor_mode mode;
};

#endif
