#ifndef STR_H
#define STR_H

#include <stddef.h>

typedef struct {
	size_t namelen;
	char *name;
	size_t contentlen;
	char *content;
	char *start;
	char *end;
} tag_t;

tag_t *create_tag();
void free_tag(tag_t *t);

int get_tag_pos(const char *s, size_t *len, char **off);
int get_tag_pos_n(const char *s, const char *name, size_t *len, char **off);

int scan_tag(const char *s, size_t n, tag_t *t);
int scan_tag_n(const char *s, size_t n, const char *name, tag_t *t);

char *match_pair(const char *s, const size_t n, const char match);
char *find_free_char(const char *s, const size_t n, const char c, const char *pairs);

size_t strip(const char *s, size_t n, char **o);
int replace(const char *s, const char *sold, const char *snew, char **o);

#endif
