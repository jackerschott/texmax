#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void pathcat(const char *p1, const char *p2, char *dest)
{
	strcpy(dest, p1);
	int n = strlen(p1);
	if (p1[n - 1] != '/') {
		dest[n] = '/';
		++n;
	}
	strcpy(dest + n, p2);
}
