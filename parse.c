#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "parse.h"
#include "str.h"
#include "util.h"

#define CAPACITY_CHUNKS 8

#define CMD_NONE 0
#define CMD_EPLOT 1
#define CMD_BATCH 2
#define CMD_MATH 3
#define CMD_INVALID -1

#define CHUNK_MSG 0
#define CHUNK_INPROMPT 1
#define CHUNK_RESULT 2
#define CHUNK_QUESTION 3

typedef int chunktype_t;

typedef struct msg_t msg_t;
typedef struct inprompt_t inprompt_t;
typedef struct result_t result_t;
typedef struct question_t question_t;
typedef struct chunk_t chunk_t;
typedef struct func_t func_t;

struct func_t {
	char *name;
	unsigned int nargs;
	char **args;
};

struct msg_t {
	char *text;
};
struct inprompt_t {
	char *text;
};
struct result_t {
	char *text;
	char *latex;
};
struct question_t {
	char *text;
	char *latex;
	char *answer;
};

struct chunk_t {
	void *content;
	chunktype_t type;
};
struct maxout_t {
	size_t capchunks;
	size_t nchunks;
	chunk_t *chunks;
};

static int check_func(const char *cmd, const char *name, char **argbeg, char **argend)
{
	char *c = strstr(cmd, name);
	if (!c)
		return 1;

	char *o = c + strlen(name);
	if (*o != '(')
		return 2;

	*argbeg = o + 1;
	*argend = match_pair(o, strlen(cmd) - (o - cmd), ')');
	if (!*argend)
		return 3;

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
cmdtype_t preparse_cmd(const char *cmd, char **pcmd, size_t *pcmdsize)
{
	size_t cmdlen = strlen(cmd);

	// TODO: Check for commands without ';' and '$'
	// TODO: Discard commands with '??' prefix

	char last = cmd[cmdlen - 1];
	if (last != ';' && last != '$')
		return CMD_INVALID;

	// TODO: Parse command
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
	} else {
		size_t newcmdsize = cmdlen + 1;
		if (newcmdsize > *pcmdsize) {
			*pcmd = erealloc(*pcmd, newcmdsize);
			*pcmdsize = newcmdsize;
		}

		strcpy(*pcmd, cmd);

		return CMD_MATH;
	}
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
			free(r->latex);
			free(r);
		} else if (o->chunks[i].type == CHUNK_QUESTION) {
			question_t *q = (question_t *)(o->chunks[i].content);
			free(q->text);
			free(q->latex);
			if (q->answer)
				free(q->answer);
		}
	}
	free(o->chunks);
	free(o);
}
int parse_maxima_out(maxout_t *out, const char *str, const size_t strlen)
{
	int retval = 0;

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
			result_t *res = malloc(sizeof(result_t));
			if (!res)
				return 1;

			tag_t subtag;
			char *content;
			size_t contentlen;
			scan_tag_n(tag.content, tag.contentlen, "t", &subtag);
			contentlen = strip(subtag.content, subtag.contentlen, &content);

			res->text = malloc(contentlen + 1);
			if (!res->text) {
				free(res);
				return 1;
			}
			strncpy(res->text, content, contentlen);
			res->text[contentlen] = '\0';

			scan_tag_n(tag.content, tag.contentlen, "l", &subtag);
			contentlen = strip(subtag.content, subtag.contentlen, &content);

			res->latex = malloc(contentlen + 1);
			if (!res->latex) {
				free(res->text);
				free(res);
				return 1;
			}
			strncpy(res->latex, content, contentlen);
			res->latex[contentlen] = '\0';

			out->chunks[out->nchunks].content = res;
			out->chunks[out->nchunks].type = CHUNK_RESULT;
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

static int escape_special_tex_chars(const char *text, char **esctext)
{
	const char chars[] = "&%$#_{}~^\\";
	const char escchars[][17] = {
		"\\&",
		"\\%",
		"\\$",
		"\\#",
		"\\_",
		"\\{",
		"\\}",
		"\\textasciitilde",
		"\\textasciicircum",
		"\\textbackslash"
	};

	int i = 0;
	char *end = (char *)(text + strlen(text));
	int repl = 0;
	for (char *cur = (char *)text; cur < end; ++cur) {
		repl = 0;
		for (int j = 0; j < LENGTH(chars) - 1; ++j) {
			if (*cur == chars[j]) {
				int n = strlen(escchars[j]);
				if (esctext)
					strncpy(*esctext + i, escchars[j], n);
				i += n;
				repl = 1;
			}
		}

		if (!repl) {
			if (esctext)
				(*esctext)[i] = *cur;
			++i;
		}
	}
	if (esctext)
		(*esctext)[i] = '\0';
	return i;
}
static void extract_plot_file(const char *restext, char **path)
{
	char *c1 = strchr(restext, ',');
	char *c2 = strchr(restext, ']');

	size_t n = c2 - c1 - 1;
	char *s = c1 + 1;
	n = strip(s, n, &s);
	++s;
	n -= 2;

	strncpy(*path, s, n);
	(*path)[n] = '\0';
}
static void write_eplot_latex(FILE *fout, const maxout_t *out)
{
	for (int i = 0; i < out->nchunks - 1; ++i) {
		if (out->chunks[i].type == CHUNK_MSG) {
			msg_t *msg = out->chunks[i].content;
			fwrite(latex_text_env[0], 1, strlen(latex_text_env[0]), fout);
			fwrite(msg->text, 1, strlen(msg->text), fout);
			fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);
		} else if (out->chunks[i].type == CHUNK_RESULT) {
			result_t *res = out->chunks[i].content;
			char *plotpath = malloc(strlen(res->text)); /* Is at least 1 char smaller than res->text */
			extract_plot_file(res->text, &plotpath);

			fwrite(latex_plot_env[0], 1, strlen(latex_plot_env[0]), fout);
			fwrite(plotpath, 1, strlen(plotpath), fout);
			fwrite(latex_plot_env[1], 1, strlen(latex_plot_env[1]), fout);
			free(plotpath);
		} else if (out->chunks[i].type == CHUNK_QUESTION) {
			question_t *question = out->chunks[i].content;
			fwrite(latex_text_env[0], 1, strlen(latex_text_env[0]), fout);
			fwrite(question->text, 1, strlen(question->text), fout);
			fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);
		}
	}
}
static void write_math_latex(FILE *fout, const maxout_t *out)
{
	for (int i = 0; i < out->nchunks - 1; ++i) {
		if (out->chunks[i].type == CHUNK_MSG) {
			msg_t *msg = out->chunks[i].content;
			fwrite(latex_text_env[0], 1, strlen(latex_text_env[0]), fout);
			fwrite(msg->text, 1, strlen(msg->text), fout);
			fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);
		} else if (out->chunks[i].type == CHUNK_RESULT) {
			result_t *res = out->chunks[i].content;
			fwrite(latex_math_env[0], 1, strlen(latex_math_env[0]), fout);
			fwrite(res->latex, 1, strlen(res->latex), fout);
			fwrite(latex_math_env[1], 1, strlen(latex_math_env[1]), fout);
		} else if (out->chunks[i].type == CHUNK_QUESTION) {
			question_t *question = out->chunks[i].content;
			fwrite(latex_text_env[0], 1, strlen(latex_text_env[0]), fout);
			fwrite(question->text, 1, strlen(question->text), fout);
			fputc(' ', fout);
			fwrite(question->answer, 1, strlen(question->answer), fout);
			fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);
		}
	}
}
int create_latex_doc(const char *path, const char *relrespath)
{
	FILE *fdoc = fopen(path, "w");
	if (!fdoc)
		return 1;

	fwrite(latex_doc_env[0], 1, strlen(latex_doc_env[0]), fdoc);
	fwrite(latex_include_res_env[0], 1, strlen(latex_include_res_env[0]), fdoc);
	fwrite(relrespath, 1, strlen(relrespath), fdoc);
	fwrite(latex_include_res_env[1], 1, strlen(latex_include_res_env[1]), fdoc);
	fwrite(latex_doc_env[1], 1, strlen(latex_doc_env[1]), fdoc);

	fclose(fdoc);
	return 0;
}
int write_latex(const char *path, const maxout_t *out,
		const char *prompt, const char *cmd, cmdtype_t cmdtype)
{
	if (out->nchunks == 0 || out->chunks[out->nchunks - 1].type != CHUNK_INPROMPT)
		return 1;

	FILE *fout = fopen(path, "a");
	if (!fout)
		return 2;

	int size = escape_special_tex_chars(prompt, NULL);
	char *pmttex = emalloc(size + 1);
	escape_special_tex_chars(prompt, &pmttex);

	fwrite(latex_prompt_env[0], 1, strlen(latex_prompt_env[0]), fout);
	fwrite(pmttex, 1, strlen(pmttex), fout);
	fwrite(latex_prompt_env[1], 1, strlen(latex_prompt_env[1]), fout);
	free(pmttex);

	size = escape_special_tex_chars(cmd, NULL);
	char *cmdtex = emalloc(size + 1);
	escape_special_tex_chars(cmd, &cmdtex);

	fwrite(latex_cmd_env[0], 1, strlen(latex_cmd_env[0]), fout);
	fwrite(cmdtex, 1, strlen(cmdtex), fout);
	fwrite(latex_cmd_env[1], 1, strlen(latex_cmd_env[1]), fout);
	free(cmdtex);

	if (cmdtype == CMD_EPLOT)
		write_eplot_latex(fout, out);
	else if (cmdtype == CMD_BATCH)
		write_math_latex(fout, out);
	else if (cmdtype == CMD_MATH)
		write_math_latex(fout, out);

	fclose(fout);
	return 0;
}
int write_log(const char *path, const maxout_t *out, const char *prompt, const char *cmd)
{
	if (out->nchunks == 0 || out->chunks[out->nchunks - 1].type != CHUNK_INPROMPT)
		return 1;

	FILE *flog = NULL;
	flog = fopen(path, "a");
	if (!flog)
		return 2;

	fwrite(prompt, 1, strlen(prompt), flog);
	fwrite(cmd, 1, strlen(cmd), flog);
	fputc('\n', flog);
	for (int i = 0; i < out->nchunks - 1; ++i) {
		if (out->chunks[i].type == CHUNK_MSG) {
			msg_t *m = out->chunks[i].content;
			fwrite(m->text, 1, strlen(m->text), flog);
		} else if (out->chunks[i].type == CHUNK_INPROMPT) {
			inprompt_t *r = out->chunks[i].content;
			fwrite(r->text, 1, strlen(r->text), flog);
		} else if (out->chunks[i].type == CHUNK_RESULT) {
			result_t *r = out->chunks[i].content;
			fwrite(r->text, 1, strlen(r->text), flog);
		} else if (out->chunks[i].type == CHUNK_QUESTION) {
			question_t *content = out->chunks[i].content;
			fwrite(content->text, 1, strlen(content->text), flog);
			fputc(' ', flog);
			fwrite(content->answer, 1, strlen(content->answer), flog);
		}
		fputc('\n', flog);
	}
	fputc('\n', flog);
	fclose(flog);
	return 0;
}
