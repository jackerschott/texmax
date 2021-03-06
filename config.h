#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

static const char *output_dir = ".texmax";
static const char *fifo_name = "cmd";
static const char *latex_doc_name = "doc.tex";
static const char *latex_res_name = "res.tex";
static const char *log_name = "max.log";

static char *const maxima_args[] = {
	"maxima",
	"--quiet",
	"--init=/usr/local/share/texmax/init",
	NULL,
};

static const char *latex_preambel = \
	"\\documentclass[preview,border=5bp,fleqn]{standalone}\n" \
	"\\usepackage{amsmath}\n"
	"\\usepackage{graphicx}\n" \
	"\\setlength\\parindent{0pt}\n" \
	"\\setlength\\mathindent{0pt}\n";
static const char *latex_doc_env[] = {
	"\\begin{document}\n",
	"\\end{document}\n"
};
static const char *latex_include_res_env[] = {
	"\\input{", "}\n"
};
static const char *latex_doc_end = "\\vspace{\\baselineskip}\n";
static const char *latex_samepage_env[] = {
	"\\begin{samepage}\n",
	"\\end{samepage}\n"
};
static const char *latex_prompt_env[] = {
	"\\mbox{\\tt ", " \\black}"
};
static const char *latex_align_sep = "& ";
static const char *latex_cmd_env[] = {
	"\\mbox{\\tt ", "}"
};

static const char *latex_math_env[] = {
	"\\begin{align*}\n",
	"\\end{align*}\n"
};
static const char *latex_math_sep = " \\\\\n";
static const char *latex_text_env[] = {
	"\\begin{verbatim}\n",
	"\\end{verbatim}\n"
};
static const char *latex_plot_env[] = {
	"\\includegraphics[width=0.8\\textwidth]{", "}\n"
};

static const char *latex_cell_end = "\n";

static const char *eplot_filename_arg_env[] = {
	"[pdf_file, \"", "\"]"
};

#endif /* CONFIG_H */
