#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#define CMD_NONE 0
#define CMD_MATH 1
#define CMD_EPLOT 2
#define CMD_BATCH 3
#define CMD_INVALID -1

#define PROMPT_INPUT 0
#define PROMPT_QUESTION 1

#define CAPACITY_CHUNKS 8

#define CHUNK_MSG 0
#define CHUNK_INPROMPT 1
#define CHUNK_RESULT 2
#define CHUNK_QUESTION 3
#define CHUNK_ERROR 4

typedef int cmdtype_t;
typedef int prompttype_t;

typedef int chunktype_t;

typedef struct func_t func_t;

typedef struct msg_t msg_t;
typedef struct inprompt_t inprompt_t;
typedef struct result_t result_t;
typedef struct question_t question_t;
typedef struct error_t error_t;

typedef struct chunk_t chunk_t;
typedef struct maxout_t maxout_t;

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
struct error_t {
	char *text;
	char *latex;
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

int is_valid(const char *cmd);
cmdtype_t preparse_cmd(const char *cmd, char **pcmd, size_t *pcmdsize);
int has_prompt(const char *out);

maxout_t *alloc_maxima_out();
void free_maxima_out(maxout_t *o);
int parse_maxima_out(maxout_t *out, const char *str, const size_t strlen);

int get_closing_prompt(const maxout_t *o, char **prompt, size_t *promptsize, prompttype_t *type);
int set_answer(maxout_t *o, const char *answer);

#endif /* PARSE_H */
