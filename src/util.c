#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void die(const char *err, ...)
{
	char msg[4096];
	va_list params;
	va_start(params, err);
	vsnprintf(msg, sizeof(msg), err, params);
	fprintf(stderr, "%s\n", msg);
	va_end(params);
	exit(1);
}

void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size > 0)
		die("Out of memory (malloc failed for %zu bytes)\n", size);
	return ptr;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);
	if (ptr == NULL && nmemb > 0 && size > 0)
		die("Out of memory (calloc failed)\n");
	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	void *new_ptr = realloc(ptr, size);
	if (new_ptr == NULL && size > 0)
		die("Out of memory (realloc failed for %zu bytes)\n", size);
	return new_ptr;
}

char *safe_strdup(const char *s)
{
	if (!s)
		return NULL;
	char *new_str = strdup(s);
	if (!new_str)
		die("Out of memory (strdup failed)\n");
	return new_str;
}
