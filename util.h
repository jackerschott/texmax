#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#define LENGTH(X) (sizeof(X) / sizeof((X)[0]))

#define BUG() do { \
	die(1, "BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	} while(0)

void *emalloc(size_t size);
void *erealloc(void *ptr, size_t size);
void die(int err, const char *fmt, ...);
void pathcat(const char *p1, const char *p2, char **dest);

#endif /* UTIL_H */

