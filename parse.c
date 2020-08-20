#include <ctype.h>
#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

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
static int parse_func(const char *cmd, const char *name,
		const char *argbeg, const char *argend, func_t *f)
{
	size_t namelen = strlen(name);
	f->name = malloc(namelen + 1);
	if (!f->name)
		return 1;
	strcpy(f->name, name);

	size_t argstrlen = argend - argbeg;
	size_t nargsmax = (argstrlen + 1) / 2;
	f->args = malloc(nargsmax * sizeof(char *));
	if (!f->args) {
		free(f->name);
		return 1;
	}

	unsigned int nargs = 0;
	char *cs = (char *)argbeg;
	char *ce;
	size_t arglen;
	while ((ce = find_free_char(cs, argend - cs, ',', "()[]"))) {
		arglen = ce - cs;
		arglen = strip(cs, arglen, &cs);

		f->args[nargs] = malloc(arglen + 1);
		if (!f->args[nargs])
			goto out_alloc_err;
		strncpy(f->args[nargs], cs, arglen);
		f->args[nargs][arglen] = '\0';
		++nargs;

		cs = ce + 1;
	}
	arglen = argend - cs;
	arglen = strip(cs, arglen, &cs);

	f->args[nargs] = malloc(arglen + 1);
	if (!f->args[nargs])
		goto out_alloc_err;
	strncpy(f->args[nargs], cs, arglen);
	f->args[nargs][arglen] = '\0';

	f->nargs = nargs + 1;
	char **newargs = realloc(f->args, nargs * sizeof(char *));
	if (!newargs)
		goto out_alloc_err;
	f->args = newargs;
	return 0;

out_alloc_err:
	for (unsigned int i = 0; i < nargs; ++i) {
		free(f->args[i]);
	}
	free(f->args);
	free(f->name);
	return 1;
}
static void free_func(func_t *f)
{
	free(f->name);
	for (unsigned int i = 0; i < f->nargs; ++i) {
		free(f->args[i]);
	}
	free(f->args);
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
int preparse_cmd(const char *cmd, char **pcmd, size_t *pcmdsize, cmdtype_t *type)
{
	size_t cmdlen = strlen(cmd);

	func_t f;
	char *argbeg, *argend;
	if (!check_func(cmd, "eplot2d", &argbeg, &argend)) {
		if (parse_func(cmd, "eplot2d", argbeg, argend, &f))
			return -1;
		char *fbeg = argbeg - 1 - strlen(f.name);
		char *fend = argend + 1;

		/* Set name and arguments for new function */
		const char newname[] = "plot2d";
		char *p = realloc(f.name, sizeof(newname));
		if (!p) {
			free_func(&f);
			return -1;
		}
		f.name = p;
		strcpy(f.name, newname);

		/* Set argument which determines filename of output file */
		const char *fplotname = "plot_XXXXXX.pdf";
		char *fplotpath = malloc(strlen(output_dir) + strlen(fplotname) + 1);
		if (!fplotpath) {
			free_func(&f);
			return -1;
		}

		pathcat(output_dir, fplotname, fplotpath);
		int fplot = mkstemps(fplotpath, 4);
		if (fplot == -1) {
			free(fplotpath);
			free_func(&f);
			return -1;
		}
		close(fplot);

		size_t fnamearglen = strlen(eplot_filename_arg_env[0])
			+ strlen(fplotpath)
			+ strlen(eplot_filename_arg_env[1]);
		char *fnamearg = malloc(fnamearglen + 1);
		if (!fnamearg) {
			free(fplotpath);
			free_func(&f);
			return -1;
		}
		fnamearg[0] = '\0';
		strcat(fnamearg, eplot_filename_arg_env[0]);
		strcat(fnamearg, fplotpath);
		strcat(fnamearg, eplot_filename_arg_env[1]);
		free(fplotpath);

		/* Append new argument to function */
		char **newargs = realloc(f.args, (f.nargs + 1) * sizeof(char *));
		if (!newargs) {
			free(fnamearg);
			free_func(&f);
			return -1;
		}
		f.args = newargs;

		f.args[f.nargs] = malloc(strlen(fnamearg) + 1);
		if (!f.args[f.nargs]) {
			free(fnamearg);
			free_func(&f);
			return -1;
		}
		strcpy(f.args[f.nargs], fnamearg);
		f.nargs += 1;
		free(fnamearg);

		/* Convert function back to string */
		size_t newfstrsize;
		func_to_str(&f, NULL, &newfstrsize);
		char *newfstr = malloc(newfstrsize);
		if (!newfstr) {
			free_func(&f);
			return -1;
		}
		func_to_str(&f, newfstr, NULL);
		free_func(&f);

		/* Replace function with new function in command string */
		size_t newpcmdsize = cmdlen + newfstrsize - (fend - fbeg) + 2;
		if (newpcmdsize > *pcmdsize) {
			char *p = realloc(*pcmd, newpcmdsize);
			if (!p) {
				free(newfstr);
				return -1;
			}
			*pcmd = p;
			*pcmdsize = newpcmdsize;
		}

		(*pcmd)[0] = '\0';
		strncat(*pcmd, cmd, fbeg - cmd);
		strcat(*pcmd, newfstr);
		strcat(*pcmd, fend);
		strcat(*pcmd, "\n");

		*type = CMD_EPLOT;
		return 0;
	}

	size_t newcmdsize = cmdlen + 2;
	if (newcmdsize > *pcmdsize) {
		char *p = realloc(*pcmd, newcmdsize);
		if (!p)
			return -1;
		*pcmd = p;
		*pcmdsize = newcmdsize;
	}
	strcpy(*pcmd, cmd);
	strcat(*pcmd, "\n");

	if (!check_func(cmd, "batch", NULL, NULL)) {
		*type = CMD_BATCH;
		return 0;
	}

	*type = CMD_MATH;
	return 0;
}
int remove_plot_files()
{
	regex_t fplotname;
	int reti = regcomp(&fplotname, "plot_[a-zA-Z0-9]{6}.pdf", REG_EXTENDED);
	if (reti) {
		regfree(&fplotname);
		return 1;
	}

	DIR *d = opendir(output_dir);
	if (!d) {
		regfree(&fplotname);
		return -1;
	}

	struct dirent *e;
	int retval = 0;
	errno = 0;
	while ((e = readdir(d))) {
		if (e->d_type != DT_REG)
			continue;

		if (regexec(&fplotname, e->d_name, 0, NULL, 0) == 0) {
			char *name = malloc(strlen(output_dir) + strlen(e->d_name) + 1);
			if (!name) {
				retval = -1;
				goto out_free;
			}
			pathcat(output_dir, e->d_name, name);

			if (unlink(name) == -1) {
				free(name);
				retval = -1;
				goto out_free;
			}
			free(name);
		}
	}
	if (errno) {
		retval = -1;
		goto out_free;
	}

out_free:
	regfree(&fplotname);
	if (!closedir(d)) {
		if (retval)
			return retval;
		return -1;
	}
	return retval;
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

	question_t *q = o->chunks[o->nchunks - 1].content;
	q->answer = malloc(strlen(answer) + 1);
	if (!q->answer)
		return 2;
	strcpy(q->answer, answer);
	return 0;
}
