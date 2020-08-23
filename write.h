#ifndef WRITE_H
#define WRITE_H

#include "parse.h"

int create_latex_doc(const char *path, const char *relrespath);
int write_latex(const char *path, const maxout_t *out,
		const char *prompt, const char *cmd, cmdtype_t cmdtype);
int clear_latex(const char *path);
int write_log(const char *path, const maxout_t *out, const char *prompt, const char *cmd);

#endif /* WRITE_H */
