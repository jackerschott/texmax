#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#define CMDTYPE_INIT_PROMPT 0
#define CMDTYPE_META 1
#define CMDTYPE_MATH 2
#define CMDTYPE_EPLOT 3
#define CMDTYPE_INVALID -1

#define PARSE_MATH 	0

#define DRAW_MATH 	0
#define DRAW_PLOT 	1
#define DRAW_TEXT 	2
#define DRAW_BATCH 	3

struct maxout;
typedef struct maxout maxout;

typedef int cmdtype_t;
typedef int parsetype_t;
typedef int drawtype_t;

int get_error(const maxout *const o);
int has_closing_question(const maxout *const o);
void get_closing_prompt(const maxout *const o, char *prompt, size_t *promptsize);

cmdtype_t preparse_cmd(const char *cmd, char *pcmd, size_t *pcmdsize);
int raw_out_has_prompt(const char *out);

maxout *parse_maxima_out(const char *outbuf, size_t outlen, cmdtype_t cmdtype);
void free_maxima_out(maxout *o);

int write_latex_res(const char *path, const maxout *const pout,
		const char *cmd, const char *prompt, const int cmdtype);
int create_latex_doc(const char *path, const char *respath);
int write_log(const char *path, const maxout *const pout, const char *cmd, const char *prompt);

#endif /* PARSE_H */
