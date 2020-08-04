#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "util.h"

void create_tag(tag *t, size_t contsize)
{
	t->tagcont = emalloc(contsize);
}
void free_tag(tag *t)
{
	free(t->tagcont);
}

int get_tag_pos(const char *s, size_t *len, char **off)
{
	char name[TAGNAME_SIZE];
	regex_t otag;
	char ctag[TAGNAME_SIZE + 3];
	char *ctagp;
	int i, n;

	regcomp(&otag, "<[a-z]+>", REG_EXTENDED);
	regmatch_t rmoff;
	if (regexec(&otag, s, 1, &rmoff, 0))
		return 1;
	regfree(&otag);

	i = rmoff.rm_so + 1;
	n = rmoff.rm_eo - rmoff.rm_so - 2;
	strncpy(name, s + i, n);
	name[n] = '\0';

	sprintf(ctag, "</%s>", name);
	ctagp = strstr(s, ctag);
	if (!ctagp)
		return 1;

	*len = (ctagp - s) + strlen(ctag) - rmoff.rm_so;
	*off = (char *)(s + rmoff.rm_so);

	return 0;
}
int get_tag_pos_n(const char *s, const char *name, size_t *len, char **off)
{
	char tag[TAGNAME_SIZE + 3];
	char *otagp;
	char *ctagp;

	sprintf(tag, "<%s>", name);
	otagp = strstr(s, tag);
	if (!otagp)
		return 1;


	sprintf(tag, "</%s>", name);
	ctagp = strstr(s, tag);
	if (!otagp)
		return 1;

	*len = (ctagp - otagp) + strlen(tag);
	*off = otagp;
	return 0;
}

int scan_tag(const char *s, tag *t)
{
	regex_t otag;
	char ctag[TAGNAME_SIZE + 3];
	char *ctagp;
	int i, n;

	regcomp(&otag, "<[a-z]+>", REG_EXTENDED);
	regmatch_t off;
	if (regexec(&otag, s, 1, &off, 0))
		return 1;
	regfree(&otag);

	i = off.rm_so + 1;
	n = off.rm_eo - off.rm_so - 2;
	strncpy(t->name, s + i, n);
	t->name[n] = '\0';
	t->s = (char *)(s + off.rm_so);

	sprintf(ctag, "</%s>", t->name);
	ctagp = strstr(s, ctag);
	if (!ctagp)
		return 1;

	i += n + 1;
	n = ctagp - (s + i);
	strncpy(t->tagcont, s + i, n);
	t->tagcont[n] = '\0';
	t->e = ctagp + strlen(ctag);

	return 0;
}
int scan_tag_n(const char *s, const char *name, tag *t)
{
	char tag[TAGNAME_SIZE + 3];
	char *otagp;
	char *ctagp;
	int i, n;

	strcpy(t->name, name);

	sprintf(tag, "<%s>", t->name);
	otagp = strstr(s, tag);
	if (!otagp)
		return 1;

	t->s = otagp;

	sprintf(tag, "</%s>", t->name);
	ctagp = strstr(s, tag);
	if (!otagp)
		return 1;

	i = (otagp - s) + strlen(tag) - 1;
	n = (ctagp - s) - i;
	strncpy(t->tagcont, s + i, n);
	t->tagcont[n] = '\0';
	t->e = ctagp + strlen(tag);

	return 0;
}

char *match_pair(const char *s, const size_t n, const char match) {
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

int strip(const char *s, size_t n, char **o)
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
