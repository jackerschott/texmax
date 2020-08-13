#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "util.h"

#define BUFSIZE_NAME 16
#define BUFSIZE_CONTENT 128

char *strnchr(const char *s, size_t n, char c)
{
	while (n--) {
		if (*s == c)
			return (char *)s;
		if (*s++ == '\0')
			break;
	}
	return NULL;
}

char *match_pair(const char *s, const size_t n, const char match)
{
	char c = s[0];
	size_t depth = 1;
	char *cur;
	for (cur = (char *)(s + 1); cur < s + n; ++cur) {
		if (*cur == match)
			--depth;
		else if (*cur == c)
			++depth;

		if (depth == 0)
			return cur;
	}
	return NULL;
}
char *find_free_char(const char *s, const size_t n, const char c, const char *pairs)
{
	char *cur = (char *)s;
	size_t npairs = strlen(pairs) / 2;
	while (cur < s + n) {
		if (*cur == c) {
			return cur;
		}

		size_t i;
		for (i = 0; i < npairs; ++i) {
			if (*cur == pairs[2 * i])
				break;
		}
		if (i < npairs) {
			cur = match_pair(cur, n - (cur - s), pairs[2 * i + 1]);
			if (!cur)
				return NULL;
			else
				continue;
		}

		++cur;
	}
	return NULL;
}

size_t strip(const char *s, size_t n, char **o)
{
	if (n == 0) {
		*o = (char *)s;
		return 0;
	}

	char *c = (char *)(s + n - 1);
	while (c - s > 0 && !isprint(*c))
		--c;
	int ns = (c - s) + 1;

	c = (char *)s;
	while (c - s < ns && !isprint(*c))
		++c;
	if (o)
		*o = c;
	return ns - (c - s);
}
int replace(const char *s, const char *sold, const char *snew, char **o)
{
	char *c = strstr(s, sold);
	if (!c)
		return 1;
	int n = c - s;
	int lold = strlen(sold);
	int lnew = strlen(snew);

	strncpy(*o, s, n);
	strcpy(*o + n, snew);
	strcpy(*o + n + lnew, s + n + lold);
	(*o)[strlen(s) + lnew - lold] = '\0';
	return 0;
}

tag_t *create_tag()
{
	tag_t *t = malloc(sizeof(tag_t));
	if (!t)
		return NULL;
	t->namelen = BUFSIZE_NAME;
	t->name = malloc(BUFSIZE_NAME);
	if (!t->name) {
		free(t);
		return NULL;
	}
	t->contentlen = BUFSIZE_CONTENT;
	t->content = malloc(BUFSIZE_CONTENT);
	if (!t->content) {
		free(t->name);
		free(t);
		return NULL;
	}
	return t;
}
void free_tag(tag_t *t)
{
	free(t->content);
	free(t->name);
}

int get_tag_pos(const char *s, size_t *len, char **off)
{
	regex_t otag;
	regcomp(&otag, "<[a-z]+>", REG_EXTENDED);
	regmatch_t rmoff;
	if (regexec(&otag, s, 1, &rmoff, 0))
		return 1;
	regfree(&otag);

	size_t i = rmoff.rm_so + 1;
	size_t n = rmoff.rm_eo - rmoff.rm_so - 2;

	char *ctag = malloc(n + 4);
	ctag[0] = '\0';
	strcat(ctag, "</");
	strncat(ctag, s + i, n);
	strcat(ctag, ">");
	char *ctagp = strstr(s, ctag);
	if (!ctagp)
		return 1;

	*len = (ctagp - s) + strlen(ctag) - rmoff.rm_so;
	*off = (char *)(s + rmoff.rm_so);
	free(ctag);

	return 0;
}
int get_tag_pos_n(const char *s, const char *name, size_t *len, char **off)
{
	char *tag = malloc(strlen(name) + 4);
	sprintf(tag, "<%s>", name);
	char *otagp = strstr(s, tag);
	if (!otagp)
		return 1;

	sprintf(tag, "</%s>", name);
	char *ctagp = strstr(s, tag);
	if (!otagp)
		return 1;

	*len = (ctagp - otagp) + strlen(tag);
	*off = otagp;
	free(tag);
	return 0;
}

int scan_tag_old(const char *s, tag_t *t)
{
	regex_t otag;
	char *ctagp;
	int i, n;

	regcomp(&otag, "<[a-z]+>", REG_EXTENDED);
	regmatch_t off;
	if (regexec(&otag, s, 1, &off, 0))
		return 1;
	regfree(&otag);
	t->start = (char *)(s + off.rm_so);

	i = off.rm_so + 1;
	n = off.rm_eo - off.rm_so - 2;
	if (n + 1 > t->namelen) {
		t->namelen = n + 1;
		t->name = realloc(t->name, t->namelen);
	}
	strncpy(t->name, s + i, n);
	t->name[n] = '\0';

	char *ctag = malloc(strlen(t->name) + 4);
	sprintf(ctag, "</%s>", t->name);
	ctagp = strstr(s, ctag);
	if (!ctagp)
		return 1;
	t->end = ctagp + strlen(ctag);
	free(ctag);

	i += n + 1;
	n = ctagp - (s + i);
	if (n + 1 > t->contentlen) {
		t->contentlen = n + 1;
		t->content = realloc(t->content, t->contentlen);
	}
	strncpy(t->content, s + i, n);
	t->content[n] = '\0';

	return 0;
}
int scan_tag_n_old(const char *s, const char *name, tag_t *t)
{
	char *otagp;
	char *ctagp;
	int i, n;

	int namesize = strlen(name) + 1;
	if (namesize > t->namelen) {
		t->namelen = namesize;
		t->name = realloc(t->name, t->namelen);
	}
	strcpy(t->name, name);

	char *tag = malloc(strlen(t->name) + 4);
	sprintf(tag, "<%s>", t->name);
	otagp = strstr(s, tag);
	if (!otagp)
		return 1;
	t->start = otagp;

	sprintf(tag, "</%s>", t->name);
	ctagp = strstr(s, tag);
	if (!otagp)
		return 1;
	t->end = ctagp + strlen(tag);

	i = (otagp - s) + strlen(tag) - 1;
	n = (ctagp - s) - i;
	if (n + 1 > t->contentlen) {
		t->contentlen = n + 1;
		t->content = realloc(t->content, t->contentlen);
	}
	strncpy(t->content, s + i, n);
	t->content[n] = '\0';

	return 0;
}
int scan_tag(const char *s, size_t n, tag_t *t)
{
	/* find name */
	s = strnchr(s, n, '<');
	if (!s)
		return 1;
	t->start = (char *)s;
	t->name = (char *)++s;

	s = strnchr(s, n, '>');
	if (!s)
		return 2;
	t->namelen = s - t->name;

	/* find content */
	t->content = (char *)(s + 1);
	while ((s = strnchr(s, n, '<'))) {
		if (*(++s) == '/'
				&& !strncmp(++s, t->name, t->namelen)
				&& *(s + t->namelen) == '>')
			break;
	}
	if (!s)
		return 3;
	t->contentlen = s - t->content - 2;
	t->end = (char *)(s + t->namelen + 1);

	return 0;
}
int scan_tag_n(const char *s, size_t n, const char *name, tag_t *t)
{

	/* find name */
	t->namelen = strlen(name);
	while ((s = strnchr(s, n, '<'))) {
		if (!strncmp(++s, name, t->namelen)
				&& *(s + t->namelen) == '>')
			break;
	}
	if (!s)
		return 1;
	t->start = (char *)(s - 1);
	t->name = (char *)s;

	/* find content */
	t->content = (char *)(s + t->namelen + 1);
	while ((s = strnchr(s, n, '<'))) {
		if (*(++s) == '/'
				&& !strncmp(++s, t->name, t->namelen)
				&& *(s + t->namelen) == '>')
			break;
	}
	if (!s)
		return 3;
	t->contentlen = s - t->content - 2;
	t->end = (char *)(s + t->namelen + 1);

	return 0;
}
