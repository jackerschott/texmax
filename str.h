#ifndef STR_H
#define STR_H

#include <stddef.h>

#define TAGNAME_SIZE 16

typedef struct {
	char name[TAGNAME_SIZE];
	char *tagcont;
	char *s;
	char *e;
} tag;

void create_tag(tag *t, size_t contsize);
void free_tag(tag *t);

int get_tag_pos(const char *s, size_t *len, char **off);
int get_tag_pos_n(const char *s, const char *name, size_t *len, char **off);

int scan_tag(const char *s, tag *t);
int scan_tag_n(const char *s, const char *name, tag *t);

char *match_pair(const char *s, const size_t n, const char match);
char *find_free_char(const char *s, const size_t n, const char c, const char *pairs);

int strip(const char *s, size_t n, char **o);
int replace(const char *s, const char *sold, const char *snew, char **o);

#endif
