#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#define LENGTH(X) (sizeof(X) / sizeof((X)[0]))

#define BUG() do { \
	fprintf(stderr, "BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	exit(-1); \
	} while(0)

void pathcat(const char *p1, const char *p2, char *dest);

#endif /* UTIL_H */

