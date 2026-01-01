#ifndef UTIL_H
#define UTIL_H

struct editor;
struct line;

void die(const char *err, ...);
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

int cx_to_rx(struct line *line, int cx);
void set_message(struct editor *e, const char *fmt, ...);
char *get_word_under_cursor(struct editor *e);

#endif