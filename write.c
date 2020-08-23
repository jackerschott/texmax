#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "str.h"
#include "util.h"
#include "write.h"

static int escape_special_tex_chars(const char *text, char *esctext)
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
					strncpy(esctext + i, escchars[j], n);
				i += n;
				repl = 1;
			}
		}

		if (!repl) {
			if (esctext)
				esctext[i] = *cur;
			++i;
		}
	}
	if (esctext)
		esctext[i] = '\0';
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

static int write_latex_msg(FILE *fout, msg_t *msg)
{
	fwrite(msg->text, 1, strlen(msg->text), fout);
	return 0;
}
static int write_latex_question(FILE *fout, question_t *question)
{
	fwrite(question->text, 1, strlen(question->text), fout);
	fputc(' ', fout);
	fwrite(question->answer, 1, strlen(question->answer), fout);
	return 0;
}
static int write_latex_error(FILE *fout, error_t *err)
{
	fwrite(err->text, 1, strlen(err->text), fout);
	return 0;
}
static int write_latex_math(FILE *fout, result_t *res)
{
	if (!res->latex)
		BUG();

	regex_t texoutprompt;
	int reti = regcomp(&texoutprompt,
			"^\\\\mbox\\{\\\\tt\\\\red\\(\\\\%o[0-9]+\\) \\\\black\\}",
			REG_EXTENDED);
	if (reti)
		return 1;

	regmatch_t off;
	reti = regexec(&texoutprompt, res->latex, 1, &off, 0);
	regfree(&texoutprompt);
	if (reti != 0)
		BUG();

	char *mathtex = malloc(strlen(res->latex + 1));
	if (!mathtex)
		return 1;

	fwrite(res->latex, 1, off.rm_eo, fout);
	fwrite(latex_align_sep, 1, strlen(latex_align_sep), fout);
	fwrite(res->latex + off.rm_eo, 1, strlen(res->latex + off.rm_eo), fout);
	return 0;
}
static int write_latex_eplot(FILE *fout, result_t *res)
{
	char *plotpath = malloc(strlen(res->text)); /* Is at least 1 char smaller than res->text */
	if (!plotpath)
		return 1;
	extract_plot_file(res->text, &plotpath);

	fwrite(latex_plot_env[0], 1, strlen(latex_plot_env[0]), fout);
	fwrite(plotpath, 1, strlen(plotpath), fout);
	fwrite(latex_plot_env[1], 1, strlen(latex_plot_env[1]), fout);
	free(plotpath);
	return 0;
}
static int write_latex_batch_cmd(FILE *fout, result_t *res)
{
	regex_t inprompt;
	int reti = regcomp(&inprompt, "^\\(%i[0-9]+\\) ", REG_EXTENDED);
	if (reti)
		return 1;

	regmatch_t off;
	reti = regexec(&inprompt, res->text, 1, &off, 0);
	regfree(&inprompt);
	if (reti != 0)
		return 2;

	char *pmt = malloc(off.rm_eo);
	if (!pmt)
		return 1;
	strncpy(pmt, res->text, off.rm_eo);
	pmt[off.rm_eo] = '\0';

	char *pmttex = malloc(off.rm_eo + 1);
	if (!pmttex) {
		free(pmt);
		return 1;
	}
	escape_special_tex_chars(pmt, pmttex);

	fwrite(latex_prompt_env[0], 1, strlen(latex_prompt_env[0]), fout);
	fwrite(pmttex, 1, strlen(pmttex), fout);
	fwrite(latex_prompt_env[1], 1, strlen(latex_prompt_env[1]), fout);
	free(pmttex);
	free(pmt);

	fwrite(latex_align_sep, 1, strlen(latex_align_sep), fout);

	char *cmd = malloc(strlen(res->text) - off.rm_eo);
	if (!cmd)
		return 1;
	strcpy(cmd, res->text + off.rm_eo);

	size_t len = escape_special_tex_chars(cmd, NULL);
	char *cmdtex = malloc(len + 1);
	if (!cmdtex) {
		free(cmd);
		return 1;
	}
	escape_special_tex_chars(cmd, cmdtex);

	fwrite(latex_cmd_env[0], 1, strlen(latex_cmd_env[0]), fout);
	fwrite(cmdtex, 1, strlen(cmdtex), fout);
	fwrite(latex_cmd_env[1], 1, strlen(latex_cmd_env[1]), fout);
	free(cmdtex);
	free(cmd);

	return 0;
}

static int write_latex_math_out(FILE *fout, const maxout_t *out)
{
	int mathenv = 1;
	int err = 0;
	for (int i = 0; i < out->nchunks - 1; ++i) {
		if (out->chunks[i].type == CHUNK_RESULT) {
			if (mathenv) {
				fputs(latex_math_sep, fout);
			} else {
				fputc('\n', fout);
				fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);
				fwrite(latex_math_env[0], 1, strlen(latex_math_env[0]), fout);
				mathenv = 1;
			}
			err = write_latex_math(fout, out->chunks[i].content);
		} else {
			if (mathenv) {
				fputc('\n', fout);
				fwrite(latex_math_env[1], 1, strlen(latex_math_env[1]), fout);
				fwrite(latex_text_env[0], 1, strlen(latex_text_env[0]), fout);
				mathenv = 0;
			} else {
				fputc('\n', fout);
			}

			if (out->chunks[i].type == CHUNK_MSG) {
				err = write_latex_msg(fout, out->chunks[i].content);
			} else if (out->chunks[i].type == CHUNK_QUESTION) {
				err = write_latex_question(fout, out->chunks[i].content);
			} else if (out->chunks[i].type == CHUNK_ERROR) {
				err = write_latex_error(fout, out->chunks[i].content);
			}
		}

		if (err)
			return err;
	}
	fputc('\n', fout);
	if (mathenv)
		fwrite(latex_math_env[1], 1, strlen(latex_math_env[1]), fout);
	else
		fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);

	return 0;
}
static int write_latex_eplot_out(FILE *fout, const maxout_t *out)
{
	fputc('\n', fout);
	fwrite(latex_math_env[1], 1, strlen(latex_math_env[1]), fout);

	int textenv = 0;
	int err = 0;
	for (int i = 0; i < out->nchunks - 1; ++i) {
		if (out->chunks[i].type == CHUNK_RESULT) {
			if (textenv) {
				fputc('\n', fout);
				fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);
				textenv = 0;
			}

			err = write_latex_eplot(fout, out->chunks[i].content);
		} else {
			if (textenv) {
				fputc('\n', fout);
			} else {
				fwrite(latex_text_env[0], 1, strlen(latex_text_env[0]), fout);
				textenv = 1;
			}

			if (out->chunks[i].type == CHUNK_MSG) {
				err = write_latex_msg(fout, out->chunks[i].content);
			} else if (out->chunks[i].type == CHUNK_QUESTION) {
				err = write_latex_question(fout, out->chunks[i].content);
			} else if (out->chunks[i].type == CHUNK_ERROR) {
				err = write_latex_error(fout, out->chunks[i].content);
			}
		}

		if (err)
			return err;
	}
	if (textenv) {
		fputc('\n', fout);
		fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);
	}

	return 0;
}
static int write_latex_batch_out(FILE *fout, const maxout_t *out)
{
	int mathenv = 1;
	int err = 0;
	for (int i = 0; i < out->nchunks - 1; ++i) {
		if (out->chunks[i].type == CHUNK_RESULT) {
			if (mathenv) {
				fputs(latex_math_sep, fout);
			} else {
				fputc('\n', fout);
				fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);
				fwrite(latex_math_env[0], 1, strlen(latex_math_env[0]), fout);
				mathenv = 1;
			}

			result_t *res = out->chunks[i].content;
			if (res->latex)
				err = write_latex_math(fout, res);
			else
				err = write_latex_batch_cmd(fout, res);
		} else {
			if (mathenv) {
				fputc('\n', fout);
				fwrite(latex_math_env[1], 1, strlen(latex_math_env[1]), fout);
				fwrite(latex_text_env[0], 1, strlen(latex_text_env[0]), fout);
				mathenv = 0;
			} else {
				fputc('\n', fout);
			}

			if (out->chunks[i].type == CHUNK_MSG) {
				err = write_latex_msg(fout, out->chunks[i].content);
			} else if (out->chunks[i].type == CHUNK_QUESTION) {
				err = write_latex_question(fout, out->chunks[i].content);
			} else if (out->chunks[i].type == CHUNK_ERROR) {
				err = write_latex_error(fout, out->chunks[i].content);
			}
		}

		if (err)
			return err;
	}
	fputc('\n', fout);
	if (mathenv)
		fwrite(latex_math_env[1], 1, strlen(latex_math_env[1]), fout);
	else
		fwrite(latex_text_env[1], 1, strlen(latex_text_env[1]), fout);

	return 0;
}

int create_latex_doc(const char *path, const char *relrespath)
{
	FILE *fdoc = fopen(path, "w");
	if (!fdoc)
		return 1;

	fwrite(latex_preambel, 1, strlen(latex_preambel), fdoc);
	fwrite(latex_doc_env[0], 1, strlen(latex_doc_env[0]), fdoc);
	fwrite(latex_include_res_env[0], 1, strlen(latex_include_res_env[0]), fdoc);
	fwrite(relrespath, 1, strlen(relrespath), fdoc);
	fwrite(latex_include_res_env[1], 1, strlen(latex_include_res_env[1]), fdoc);
	fwrite(latex_doc_end, 1, strlen(latex_doc_end), fdoc);
	fwrite(latex_doc_env[1], 1, strlen(latex_doc_env[1]), fdoc);

	fclose(fdoc);
	return 0;
}
int write_latex(const char *path, const maxout_t *out,
		const char *prompt, const char *cmd, cmdtype_t cmdtype)
{
	if (out->nchunks == 0 || out->chunks[out->nchunks - 1].type != CHUNK_INPROMPT)
		return -1;

	FILE *fout = fopen(path, "a");
	if (!fout)
		return -1;

	fwrite(latex_math_env[0], 1, strlen(latex_math_env[0]), fout);

	size_t len = escape_special_tex_chars(prompt, NULL);
	char *pmttex = malloc(len + 1);
	if (!pmttex) {
		fclose(fout);
		return -1;
	}
	escape_special_tex_chars(prompt, pmttex);

	fwrite(latex_prompt_env[0], 1, strlen(latex_prompt_env[0]), fout);
	fwrite(pmttex, 1, strlen(pmttex), fout);
	fwrite(latex_prompt_env[1], 1, strlen(latex_prompt_env[1]), fout);
	free(pmttex);

	fwrite(latex_align_sep, 1, strlen(latex_align_sep), fout);

	len = escape_special_tex_chars(cmd, NULL);
	char *cmdtex = malloc(len + 1);
	if (!cmdtex) {
		fclose(fout);
		return -1;
	}
	escape_special_tex_chars(cmd, cmdtex);

	fwrite(latex_cmd_env[0], 1, strlen(latex_cmd_env[0]), fout);
	fwrite(cmdtex, 1, strlen(cmdtex), fout);
	fwrite(latex_cmd_env[1], 1, strlen(latex_cmd_env[1]), fout);
	free(cmdtex);

	int err = 0;
	if (cmdtype == CMD_MATH)
		err = write_latex_math_out(fout, out);
	else if (cmdtype == CMD_EPLOT)
		err = write_latex_eplot_out(fout, out);
	else if (cmdtype == CMD_BATCH)
		err = write_latex_batch_out(fout, out);

	fwrite(latex_cell_end, 1, strlen(latex_cell_end), fout);

	if (fclose(fout))
		return -1;

	if (err)
		return -1;
	return 0;
}
int clear_latex(const char *path)
{
	FILE *fout = fopen(path, "w");
	if (!fout)
		return -1;

	if (fclose(fout) == EOF)
		return -1;
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
