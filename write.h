#ifndef WRITE_H
#define WRITE_H

#include "parse.h"

int create_latex_doc(const char *path, const char *relrespath);
int create_latex_res(const char *path);
int write_latex_res(const char *path, const maxout_t *out,
		const char *prompt, const char *cmd, cmdtype_t cmdtype);
int write_log(const char *path, const maxout_t *out, const char *prompt, const char *cmd);

#endif /* WRITE_H */
