#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#define PROMPT_INPUT 0
#define PROMPT_QUESTION 1

typedef struct maxout_t maxout_t;
struct maxout_t;

typedef int cmdtype_t;
typedef int prompttype_t;

cmdtype_t preparse_cmd(const char *cmd, char **pcmd, size_t *pcmdsize);
int has_prompt(const char *out);

maxout_t *alloc_maxima_out();
void free_maxima_out(maxout_t *o);
int parse_maxima_out(maxout_t *out, const char *str, const size_t strlen);

int get_closing_prompt(const maxout_t *o, char **prompt, size_t *promptsize, prompttype_t *type);
int set_answer(maxout_t *o, const char *answer);

int create_latex_doc(const char *path, const char *respath);
int write_latex(const char *path, const maxout_t *out,
		const char *prompt, const char *cmd, cmdtype_t cmdtype);
int write_log(const char *path, const maxout_t *out, const char *prompt, const char *cmd);

#endif /* PARSE_H */
