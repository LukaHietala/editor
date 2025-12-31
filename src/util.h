#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void die(const char *err, ...);
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
char *safe_strdup(const char *s);

#endif