#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#define LENGTH(X) (sizeof(X) / sizeof((X)[0]))

void *emalloc(size_t size);
void *erealloc(void *ptr, size_t size);
void die(const char *fmt, ...);
void pathcat(const char *p1, const char *p2, char **dest);

#endif /* UTIL_H */

