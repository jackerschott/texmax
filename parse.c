#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "parse.h"
#include "str.h"
#include "util.h"


typedef struct {
	char *name;
	unsigned int nargs;
	char **args;
} func;

typedef struct {
	char *text;
	char *latex;
} result;
struct maxout {
	size_t chunkarrsize;
	size_t chunksize;
	size_t nchunks;
	result *res;
	char *log;
	char *newprompt;
	int question;
	int err;
	char *errmsg;
};

int get_error(const maxout *const o)
{
	return o->err;
}
int has_closing_question(const maxout *const o)
{
	return o->question;
}
void get_closing_prompt(const maxout *const o, char *prompt, size_t *promptsize)
{
	int newpromptsize = strlen(o->newprompt) + 1;
	if (newpromptsize > *promptsize) {
		*promptsize = newpromptsize;
		prompt = erealloc(prompt, *promptsize);
	}
	strcpy(prompt, o->newprompt);
}

int check_func(const char *cmd, const char *name, char **argbeg, char **argend)
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
void parse_func(const char *cmd, const char *name,
		const char *argbeg, const char *argend, func *f)
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
void free_func(func *f)
{
	free(f->name);
	for (unsigned int i = 0; i < f->nargs; ++i) {
		free(f->args[i]);
	}
}
void func_to_str(func *f, char *s, size_t *size)
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
cmdtype_t preparse_cmd(const char *cmd, char *pcmd, size_t *pcmdsize)
{
	size_t cmdlen = strlen(cmd);

	// TODO: Check for commands without ';' and '$'
	// TODO: Discard commands with '??' prefix

	char last = cmd[cmdlen - 1];
	if (last != ';' && last != '$')
		return CMDTYPE_INVALID;

	// TODO: Parse command
	func f;
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
			pcmd = erealloc(pcmd, newcmdsize);
			*pcmdsize = newcmdsize;
		}

		pcmd[0] = '\0';
		strncat(pcmd, cmd, fbeg - cmd);
		strcat(pcmd, newfstr);
		strcat(pcmd, fend);

		return CMDTYPE_EPLOT;
	} else {
		size_t newcmdsize = cmdlen + 1;
		if (newcmdsize > *pcmdsize) {
			pcmd = erealloc(pcmd, newcmdsize);
			*pcmdsize = newcmdsize;
		}

		strcpy(pcmd, cmd);

		return CMDTYPE_MATH;
	}
}
int raw_out_has_prompt(const char *out)
{
	return strstr(out, "</prompt>") != NULL;
}

static int is_error(char *bare)
{
	return 0;
}
static int is_question(char *prompt)
{
	return 0;
}

void comp_chunk_dims(const char *outbuf, size_t outlen,
		size_t *_chunkarrsize, size_t *_chunksize)
{
	size_t chunksize = 0;
	size_t chunkarrsize = 0;
	char *cur = (char *)outbuf;
	char *tpos;
	size_t tlen;
	size_t barelen;
	/* Skip non printable characters between chunks, such that
	   these are not counted as bare chunk (without case
	   distinction or assuming to much). That non printable 
	   characters are not counted for chunksize is actually not
	   that important */
	while (cur - outbuf < outlen && !get_tag_pos(cur, &tlen, &tpos)) {
		if (tlen > chunksize)
			chunksize = tlen;
		++chunkarrsize;

		barelen = strip(cur, tpos - cur, &cur);
		if (barelen > chunksize)
			chunksize = barelen;
		if (barelen > 0)
			++chunkarrsize;

		cur = tpos + tlen;
	}
	barelen = strip(cur, outbuf + outlen - cur, &cur);
	if (barelen > chunksize)
		chunksize = barelen;
	if (barelen > 0)
		++chunkarrsize;

	*_chunksize = chunksize;
	*_chunkarrsize = chunkarrsize;
}
maxout *parse_maxima_out(const char *outbuf, const size_t outlen, const cmdtype_t cmdtype)
{
	size_t chunkarrsize, chunksize;
	comp_chunk_dims(outbuf, outlen, &chunkarrsize, &chunksize);

	maxout *out = emalloc(sizeof(maxout));
	out->chunkarrsize = chunkarrsize;
	out->chunksize = chunksize;
	out->res = emalloc(chunkarrsize * sizeof(result));
	for (int i = 0; i < chunkarrsize; ++i) {
		out->res[i].latex = emalloc(chunksize);
		out->res[i].text = emalloc(chunksize);
	}
	out->log = emalloc(chunksize * chunkarrsize + 1);
	out->newprompt = emalloc(chunksize);
	out->errmsg = emalloc(chunksize);
	
	tag t, s;
	create_tag(&t, out->chunksize);
	create_tag(&s, out->chunksize);
	char *bare = malloc(out->chunksize + 1);
	out->log[0] = '\0';
	out->err = 0;
	out->question = 0;

	char *cur = (char *)outbuf;
	int n;
	for (n = 0; n < out->chunkarrsize; ++n) {
		scan_tag(cur, &t);

		size_t barelen = strip(cur, t.s - cur, &cur);
		if (barelen > 0) {
			strncpy(bare, cur, barelen);
			bare[barelen] = '\0';

			strcat(out->log, bare);
			strcat(out->log, "\n");

			out->err = is_error(bare);
			if (out->err) {
				out->errmsg = bare;
				break;
			}
		}

		if (strcmp(t.name, "result") == 0) {
			scan_tag_n(t.tagcont, "latex", &s);
			strcpy(out->res[n].latex, s.tagcont);

			scan_tag_n(t.tagcont, "text", &s);
			strcpy(out->res[n].text, s.tagcont);

			strcat(out->log, s.tagcont);
		} else if (strcmp(t.name, "prompt") == 0) {
			strcpy(out->newprompt, t.tagcont);
			out->question = is_question(t.tagcont);
			break;
		}

		cur = t.e;
	}
	free(bare);
	free_tag(&s);
	free_tag(&t);

	out->nchunks = n;

	/* Testing */
	printf("pout:\n");
	printf("\tchunkarrsize: %lu\n", out->chunkarrsize);
	printf("\tchunksize: %lu\n", out->chunksize);
	printf("\tnchunks: %lu\n", out->nchunks);
	printf("\tres:\n");
	for (int i = 0; i < out->nchunks; ++i) {
		printf("\t\ttext: %s\n", out->res[i].text);
		printf("\t\tlatex: %s\n", out->res[i].latex);
	}
	printf("\tlog: %s\n", out->log);
	printf("\tnewprompt: %s\n", out->newprompt);
	printf("\tquestion: %i\n", out->question);
	printf("\terr: %i\n", out->err);
	if (out->err)
		printf("\terrmsg: %s\n", out->errmsg);
	printf("\n");

	return out;
}
void free_maxima_out(maxout *o)
{
	free(o->errmsg);
	free(o->newprompt);
	free(o->log);
	for (int i = 0; i < o->chunkarrsize; ++i) {
		free(o->res[i].latex);
		free(o->res[i].text);
	}
	free(o->res);
	free(o);
}

int escape_special_chars(const char *text, char **o)
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
	char *e = (char *)(text + strlen(text));
	int repl = 0;
	for (char *c = (char *)text; c < e; ++c) {
		repl = 0;
		for (int j = 0; j < LENGTH(chars) - 1; ++j) {
			if (*c == chars[j]) {
				int n = strlen(escchars[j]);
				if (o)
					strncpy(*o + i, escchars[j], n);
				i += n;
				repl = 1;
			}
		}

		if (!repl) {
			if (o)
				(*o)[i] = *c;
			++i;
		}
	}
	if (o)
		(*o)[i] = '\0';
	return i;
}
void extract_plot_file(const char *restext, char **path)
{
	char *c1 = strchr(restext, ',');
	char *c2 = strchr(restext, ']');

	int n = c2 - c1 - 1;
	char *s = c1 + 1;
	n = strip(s, n, &s);
	++s;
	n -= 2;

	strncpy(*path, s, n);
	(*path)[n] = '\0';
}
int create_latex_doc(const char *path, const char *respath)
{
	FILE *fdoc = fopen(path, "w");
	if (!fdoc) {
		fclose(fdoc);
		return 1;
	}

	fwrite(latex_doc_env[0], 1, strlen(latex_doc_env[0]), fdoc);
	fwrite(latex_include_res_env[0], 1, strlen(latex_include_res_env[0]), fdoc);
	fwrite(respath, 1, strlen(respath), fdoc);
	fwrite(latex_include_res_env[1], 1, strlen(latex_include_res_env[1]), fdoc);
	fwrite(latex_doc_env[1], 1, strlen(latex_doc_env[1]), fdoc);

	fclose(fdoc);
	return 0;
}
int write_latex_res(const char *path, const maxout *const pout,
		const char *cmd, const char *prompt, const cmdtype_t cmdtype)
{
	if (pout->nchunks == 0)
		return 0;

	FILE *fres = fopen(path, "a");
	if (!fres)
		return 1;

	int size = escape_special_chars(prompt, NULL);
	char *pmttex = emalloc(size + 1);
	escape_special_chars(prompt, &pmttex);

	fwrite(latex_prompt_env[0], 1, strlen(latex_prompt_env[0]), fres);
	fwrite(pmttex, 1, strlen(pmttex), fres);
	fwrite(latex_prompt_env[1], 1, strlen(latex_prompt_env[1]), fres);
	free(pmttex);

	size = escape_special_chars(cmd, NULL);
	char *cmdtex = emalloc(size + 1);
	escape_special_chars(cmd, &cmdtex);

	fwrite(latex_cmd_env[0], 1, strlen(latex_cmd_env[0]), fres);
	fwrite(cmdtex, 1, strlen(cmdtex), fres);
	fwrite(latex_cmd_env[1], 1, strlen(latex_cmd_env[1]), fres);
	free(cmdtex);

	switch (cmdtype) {
	case CMDTYPE_MATH: {
		for (int i = 0; i < pout->nchunks; ++i) {
			char *content = pout->res[i].latex;
			fwrite(latex_math_env[0], 1, strlen(latex_math_env[0]), fres);
			fwrite(content, 1, strlen(content), fres);
			fwrite(latex_math_env[1], 1, strlen(latex_math_env[1]), fres);
		}
		break;
	}
	case CMDTYPE_EPLOT: {
	   	for (int i = 0; i < pout->nchunks; ++i) {
			char *text = pout->res[i].text;
			char *plotpath = emalloc(strlen(text));
			extract_plot_file(text, &plotpath);
			fwrite(latex_plot_env[0], 1, strlen(latex_plot_env[0]), fres);
			fwrite(plotpath, 1, strlen(plotpath), fres);
			fwrite(latex_plot_env[1], 1, strlen(latex_plot_env[1]), fres);
			free(plotpath);
	   	}
		break;
	}
	}

	fclose(fres);
	return 0;
}
int write_log(const char *path, const maxout *const pout, const char *cmd, const char *prompt)
{
	FILE *flog = NULL;
	flog = fopen(path, "w+");
	if (!flog)
		return 1;

	fwrite(prompt, 1, strlen(prompt), flog);
	fwrite(cmd, 1, strlen(cmd), flog);
	fputc('\n', flog);
	fwrite(pout->log, 1, strlen(pout->log), flog);

	fclose(flog);

	/* Testing */
	// fwrite(prompt, 1, strlen(prompt), stdout);
	// fwrite(cmd, 1, strlen(cmd), stdout);
	// fputc('\n', stdout);
	// fwrite(pout->log, 1, strlen(pout->log), stdout);
	// fflush(stdout);

	return 0;
}
