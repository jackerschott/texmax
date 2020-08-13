#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

static const char *cache_dir = ".vimax";
static const char *fifo_name = "cmd";
static const char *latex_doc_name = "doc.tex";
static const char *latex_res_name = "res.tex";
static const char *log_name = "max.log";

static char *const maxima_args[] = {
	"maxima",
	"--quiet",
	"--init-lisp=init.lisp",
	NULL,
};

static const char *latex_doc_env[] = {
	"\\documentclass{minimal}\n" \
	"\\usepackage{graphicx}\n" \
	"\\setlength\\parindent{0pt}\n" \
	"\\begin{document}\n",
	"\\end{document}"
};
static const char *latex_include_res_env[] = {
	"\\input{", "}\n"
};
static const char *latex_prompt_env[] = {
	"\\begin{samepage}\n" \
	"\\texttt{", "} "
};
static const char *latex_cmd_env[] = {
	"\\texttt{", "} \\\\\n"
};

static const char *latex_text_env[] = {
	"\\texttt{", "} \\\\\n"
};
static const char *latex_math_env[] = {
	"\\begin{flalign}\n",
	"\n\\end{flalign}\n" \
	"\\end{samepage}\n" \
	"\\bigskip\n\n"
};
static const char *latex_plot_env[] = {
	"\\includegraphics[width=0.8\\textwidth]{", "}\n" \
	"\\end{samepage}\n" \
	"\\bigskip\n\n"
};
static const char *latex_err_env[] = {
	"\\texttt{", "}\n" \
	"\\end{samepage}\n" \
	"\\bigskip\n\n"
};

static const char *plot_embed_args[] = {
	"[gnuplot_term, png]",
	"[gnuplot_out_file, \"test.png\"]"
};

#endif /* CONFIG_H */
