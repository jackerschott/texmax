#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void die(int err, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(err);
}

void *emalloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		die(-1, "malloc:");
	return p;
}
void *erealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (!p)
		die(-1, "realloc:");
	return p;
}

void pathcat(const char *p1, const char *p2, char **dest)
{
	strcpy(*dest, p1);
	int n = strlen(p1);
	if (p1[n - 1] != '/') {
		(*dest)[n] = '/';
		++n;
	}
	strcpy(*dest + n, p2);
}
