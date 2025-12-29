#include <ctype.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include "editor.h"

static int is_word_char(char c)
{
	return isalnum(c) || c == '_';
}

static char *get_word_under_cursor(struct editor *e)
{
	struct line *l = e->active_buf->current;
	if (!l || l->size == 0)
		return NULL;

	int cx = e->active_buf->cx;
	/* If cursor is at the end of the line (on the \0), move back one */
	if (cx >= l->size && cx > 0)
		cx--;

	/* Find start of word */
	int start = cx;
	while (start > 0 && is_word_char(l->data[start - 1]))
		start--;

	/* Find end of word */
	int end = cx;
	while (end < l->size && is_word_char(l->data[end]))
		end++;

	int len = end - start;
	if (len <= 0)
		return NULL;

	char *word = malloc(len + 1);
	if (word) {
		memcpy(word, &l->data[start], len);
		word[len] = '\0';
	}
	return word;
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
