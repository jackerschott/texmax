#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "parse.h"
#include "str.h"
#include "util.h"

static int check_func(const char *cmd, const char *name, char **argbeg, char **argend)
{
	char *c = strstr(cmd, name);
	if (!c)
		return 1;

	char *o = c + strlen(name);
	if (*o != '(')
		return 2;

	char *_argbeg = o + 1;
	char *_argend = match_pair(o, strlen(cmd) - (o - cmd), ')');
	if (!_argend)
		return 3;

	if (argbeg)
		*argbeg = _argbeg;
	if (argend)
		*argend = _argend;
	return 0;
}
static void parse_func(const char *cmd, const char *name,
		const char *argbeg, const char *argend, func_t *f)
{
	size_t namelen = strlen(name);
	f->name = emalloc(namelen + 1);
	strcpy(f->name, name);

	size_t argstrlen = argend - argbeg;
	size_t nargsmax = (argstrlen + 1) / 2;
	f->args = emalloc(nargsmax * sizeof(char *));

	unsigned int nargs = 0;
	char *cs = (char *)argbeg;
	char *ce;
	size_t arglen;
	while ((ce = find_free_char(cs, argend - cs, ',', "()[]"))) {
		arglen = ce - cs;
		arglen = strip(cs, arglen, &cs);

		f->args[nargs] = emalloc(arglen + 1);
		strncpy(f->args[nargs], cs, arglen);
		f->args[nargs][arglen] = '\0';
		++nargs;

		cs = ce + 1;
	}
	arglen = argend - cs;
	arglen = strip(cs, arglen, &cs);

	f->args[nargs] = emalloc(arglen + 1);
	strncpy(f->args[nargs], cs, arglen);
	f->args[nargs][arglen] = '\0';

	f->nargs = nargs + 1;
	f->args = erealloc(f->args, nargs * sizeof(char *));
}
static void free_func(func_t *f)
{
	free(f->name);
	for (unsigned int i = 0; i < f->nargs; ++i) {
		free(f->args[i]);
	}
}
static void func_to_str(func_t *f, char *s, size_t *size)
{
	if (s == NULL) {
		int len = strlen(f->name) + 2;
		len += strlen(f->args[0]);
		for (int i = 1; i < f->nargs; ++i) {
			/* strlen(", ") = 2 */
			len += 2 + strlen(f->args[i]);
		}
		*size = len + 1;
		return;
	}

	s[0] = '\0';
	strcat(s, f->name);
	strcat(s, "(");
	strcat(s, f->args[0]);
	for (int i = 1; i < f->nargs; ++i) {
		strcat(s, ", ");
		strcat(s, f->args[i]);
	}
	strcat(s, ")");
}
int is_valid(const char *cmd)
{
	if (strncmp(cmd, "??", 2) == 0)
		return 0;

	char last = cmd[strlen(cmd) - 1];
	if (last != ';' && last != '$')
		return 0;

	return 1;
}
cmdtype_t preparse_cmd(const char *cmd, char **pcmd, size_t *pcmdsize)
{
	size_t cmdlen = strlen(cmd);

	func_t f;
	char *argbeg, *argend;
	if (!check_func(cmd, "eplot2d", &argbeg, &argend)) {
		parse_func(cmd, "eplot2d", argbeg, argend, &f);
		char *fbeg = argbeg - 1 - strlen(f.name);
		char *fend = argend + 1;

		const char newname[] = "plot2d";
		f.name = erealloc(f.name, sizeof(newname));
		strcpy(f.name, newname);

		size_t dn = LENGTH(plot_embed_args);
		f.nargs += dn;
		f.args = erealloc(f.args, f.nargs * sizeof(char *));
		for (int i = 0; i < dn; ++i) {
			int j = f.nargs - dn + i;
			f.args[j] = emalloc(strlen(plot_embed_args[i]) + 1);
			strcpy(f.args[j], plot_embed_args[i]);
		}

		size_t newfstrsize;
		func_to_str(&f, NULL, &newfstrsize);
		char *newfstr = emalloc(newfstrsize);
		func_to_str(&f, newfstr, NULL);
		free_func(&f);

		size_t newcmdsize = cmdlen + newfstrsize - (fend - fbeg);
		if (newcmdsize > *pcmdsize) {
			*pcmd = erealloc(*pcmd, newcmdsize);
			*pcmdsize = newcmdsize;
		}

		(*pcmd)[0] = '\0';
		strncat(*pcmd, cmd, fbeg - cmd);
		strcat(*pcmd, newfstr);
		strcat(*pcmd, fend);

		return CMD_EPLOT;
	}

	size_t newcmdsize = cmdlen + 1;
	if (newcmdsize > *pcmdsize) {
		*pcmd = erealloc(*pcmd, newcmdsize);
		*pcmdsize = newcmdsize;
	}
	strcpy(*pcmd, cmd);

	if (!check_func(cmd, "batch", NULL, NULL))
		return CMD_BATCH;
	return CMD_MATH;
}

int has_prompt(const char *out)
{
	return strstr(out, "</p>") != NULL;
}

maxout_t *alloc_maxima_out()
{
	maxout_t *out = malloc(sizeof(maxout_t));
	if (!out)
		return NULL;
	out->capchunks = CAPACITY_CHUNKS;
	out->nchunks = 0;
	out->chunks = malloc(out->capchunks * sizeof(chunk_t));
	if (!out->chunks)
		return NULL;
	return out;
}
void free_maxima_out(maxout_t *o)
{
	for (int i = 0; i < o->nchunks; ++i) {
		if (o->chunks[i].type == CHUNK_MSG) {
			msg_t *m = (msg_t *)(o->chunks[i].content);
			free(m->text);
			free(m);
		} else if (o->chunks[i].type == CHUNK_INPROMPT) {
			inprompt_t *p = (inprompt_t *)(o->chunks[i].content);
			free(p->text);
			free(p);
		} else if (o->chunks[i].type == CHUNK_RESULT) {
			result_t *r = (result_t *)(o->chunks[i].content);
			free(r->text);
			if (r->latex)
				free(r->latex);
			free(r);
		} else if (o->chunks[i].type == CHUNK_QUESTION) {
			question_t *q = (question_t *)(o->chunks[i].content);
			free(q->text);
			free(q->latex);
			if (q->answer)
				free(q->answer);
		} else if (o->chunks[i].type == CHUNK_ERROR) {
			error_t *e = (error_t *)(o->chunks[i].content);
			free(e->text);
			if (e->latex)
				free(e->latex);
		}
	}
	free(o->chunks);
	free(o);
}
int parse_maxima_out(maxout_t *out, const char *str, const size_t strlen)
{
	int retval = 0;

	regex_t outprompt;
	regcomp(&outprompt, "^\\(%(o|i)[0-9]+\\) ", REG_EXTENDED);

	char *cur = (char *)str;
	char *end = (char *)(str + strlen);
	tag_t tag;
	while (!scan_tag(cur, end - cur, &tag)) {
		if (out->nchunks >= out->capchunks) {
			out->capchunks += CAPACITY_CHUNKS;
			chunk_t *c = realloc(out->chunks, out->capchunks * sizeof(chunk_t));
			if (!c)
				return 1;
			out->chunks = c;
		}

		size_t msglen = strip(cur, tag.start - cur, &cur);
		if (msglen > 0) {
			/* Parse message before tag */
			msg_t *msg = malloc(sizeof(msg_t));
			if (!msg)
				return 1;

			msg->text = malloc(msglen + 1);
			if (!msg->text) {
				free(msg);
				return 1;
			}
			strncpy(msg->text, cur, msglen);
			msg->text[msglen] = '\0';

			out->chunks[out->nchunks].content = msg;
			out->chunks[out->nchunks].type = CHUNK_MSG;
			++out->nchunks;
		}

		if (strncmp(tag.name, "r", tag.namelen) == 0) {
			/* Parse result tag */
			tag_t subtag;
			char *content;
			size_t contentlen;
			scan_tag_n(tag.content, tag.contentlen, "t", &subtag);
			contentlen = strip(subtag.content, subtag.contentlen, &content);

			char *text = malloc(contentlen + 1);
			if (!text) {
				return 1;
			}
			strncpy(text, content, contentlen);
			text[contentlen] = '\0';

			char *latex = NULL;
			if (!scan_tag_n(tag.content, tag.contentlen, "l", &subtag)) {
				contentlen = strip(subtag.content, subtag.contentlen, &content);

				latex = malloc(contentlen + 1);
				if (!latex) {
					free(text);
					return 1;
				}
				strncpy(latex, content, contentlen);
				latex[contentlen] = '\0';
			}

			void *chunkcont = malloc(sizeof(result_t));
			if (!chunkcont) {
				free(text);
				if (latex)
					free(latex);
				return 1;
			}
			if (regexec(&outprompt, text, 0, NULL, 0) == 0) {
				result_t *res = chunkcont;
				res->text = text;
				res->latex = latex;
				out->chunks[out->nchunks].content = res;
				out->chunks[out->nchunks].type = CHUNK_RESULT;
			} else {
				error_t *err = chunkcont;
				err->text = text;
				err->latex = latex;
				out->chunks[out->nchunks].content = err;
				out->chunks[out->nchunks].type = CHUNK_ERROR;
			}
			++out->nchunks;
		} else if (strncmp(tag.name, "p", tag.namelen) == 0) {
			/* Parse prompt tag */
			tag_t subtag;
			char *content;
			size_t contentlen;
			if (!scan_tag_n(tag.content, tag.contentlen, "t", &subtag)) {
				/* Parse question */
				contentlen = strip(subtag.content, subtag.contentlen, &content);

				question_t *question = malloc(sizeof(question_t));
				if (!question)
					return 1;

				question->text = malloc(contentlen + 1);
				if (!question->text) {
					free(question);
					return 1;
				}
				strncpy(question->text, content, contentlen);
				question->text[contentlen] = '\0';

				scan_tag_n(tag.content, tag.contentlen, "l", &subtag);
				contentlen = strip(subtag.content, subtag.contentlen, &content);

				question->latex = malloc(contentlen + 1);
				if (!question->latex) {
					free(question->text);
					free(question);
					return 1;
				}
				strncpy(question->latex, content, contentlen);
				question->latex[contentlen] = '\0';

				question->answer = NULL;

				out->chunks[out->nchunks].content = question;
				out->chunks[out->nchunks].type = CHUNK_QUESTION;
				++out->nchunks;
			} else {
				/* Parse input prompt */
				contentlen = strip(tag.content, tag.contentlen, &content);

				inprompt_t *prompt = malloc(sizeof(inprompt_t));
				if (!prompt)
					return 1;

				prompt->text = malloc(contentlen + 1);
				if (!prompt->text) {
					free(prompt);
					return 1;
				}
				strncpy(prompt->text, content, contentlen);
				prompt->text[contentlen] = '\0';

				out->chunks[out->nchunks].content = prompt;
				out->chunks[out->nchunks].type = CHUNK_INPROMPT;
				++out->nchunks;
			}
			break;
		}

		cur = tag.end;
	}

	regfree(&outprompt);

	/* Testing */
	printf("pout:\n");
	printf("\tcapchunks: %lu\n", out->capchunks);
	printf("\tnchunks: %lu\n", out->nchunks);
	for (int i = 0; i < out->nchunks; ++i) {
		if (out->chunks[i].type == CHUNK_MSG) {
			printf("\tmsg:\n");
			msg_t *m = ((msg_t *)out->chunks[i].content);
			printf("\t\ttext: %s\n", m->text);
		} else if (out->chunks[i].type == CHUNK_INPROMPT) {
			printf("\tprompt:\n");
			inprompt_t *p = ((inprompt_t *)out->chunks[i].content);
			printf("\t\ttext: %s\n", p->text);
		} else if (out->chunks[i].type == CHUNK_RESULT) {
			printf("\tresult:\n");
			result_t *r = ((result_t *)out->chunks[i].content);
			printf("\t\ttext: %s\n", r->text);
			printf("\t\tlatex: %s\n", r->latex);
		} else if (out->chunks[i].type == CHUNK_QUESTION) {
			printf("\tquestion:\n");
			question_t *q = ((question_t *)out->chunks[i].content);
			printf("\t\ttext: %s\n", q->text);
			printf("\t\tlatex: %s\n", q->latex);
			printf("\t\tanswer: %s\n", q->answer);
		}
	}
	printf("\n");

	return 0;
}

int get_closing_prompt(const maxout_t *o, char **prompt, size_t *promptsize, prompttype_t *type)
{
	if (o->nchunks == 0)
		return 1;

	chunk_t chunk = o->chunks[o->nchunks - 1];
	char *text;
	if (chunk.type == CHUNK_INPROMPT) {
		inprompt_t *p = chunk.content;
		text = p->text;
		*type = PROMPT_INPUT;
	} else if (chunk.type == CHUNK_QUESTION) {
		question_t *q = chunk.content;
		text = q->text;
		*type = PROMPT_QUESTION;
	} else {
		return 2;
	}

	int newpromptsize = strlen(text) + 1;
	if (newpromptsize > *promptsize) {
		*promptsize = newpromptsize;
		char *p = realloc(*prompt, *promptsize);
		if (!*p)
			return 3;
		*prompt = p;
	}
	strcpy(*prompt, text);
	return 0;
}
int set_answer(maxout_t *o, const char *answer)
{
	if (o->nchunks == 0 || o->chunks[o->nchunks - 1].type != CHUNK_QUESTION)
		return 1;

	char *ans;
	size_t n = strip(answer, strlen(answer), &ans);

	question_t *q = o->chunks[o->nchunks - 1].content;
	q->answer = malloc(n + 1);
	if (!q->answer)
		return 2;
	strncpy(q->answer, ans, n);
	return 0;
}
