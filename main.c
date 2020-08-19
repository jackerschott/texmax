#include <ctype.h>
#include <errno.h>
#include <fcntl.h> 
#include <poll.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "parse.h"
#include "str.h"
#include "util.h"
#include "write.h"

#define READSIZE_OUT 16384
#define BUFSIZE_IN 512

#define BUFSIZE_ANS 16

static char *fifo_path;
static char *latex_doc_path;
static char *latex_res_path;
static char *log_path;

static pid_t maxpid;
static int fmaxcmd;
static int fmaxout;

static char *outstr;
static size_t outstrlen;
static size_t outstrsize;

static char *inprompt;
static size_t inpromptsize;
static char *parsedcmd;
static size_t parsedcmdsize;

static pid_t start_process(const char *file, char *const argv[], int fd[2])
{
	// TODO: error handling

	int pipein[2];
	int pipeout[2];

	if (pipe(pipein))
		die(-2, "pipe:");
	if (pipe(pipeout))
		die(-2, "pipe:");
	pid_t pid = fork();
	if (pid == -1) {
		die(-2, "fork:");
	} else if (pid == 0) {
		close(pipein[1]);
		close(pipeout[0]);

		dup2(pipein[0], STDIN_FILENO);
		dup2(pipeout[1], STDOUT_FILENO);
		close(pipein[0]);
		close(pipeout[1]);

		execvp(file, argv);
		die(-2, "execvp: could not execute `%s':", file);
	} else {
		close(pipein[0]);
		close(pipeout[1]);

		fd[0] = pipeout[0];
		fd[1] = pipein[1];
	}

	return pid;
}

static void get_answer(const char* question, char **answer, size_t *answersize)
{
	char *const qst = emalloc(strlen(question) + 1);
	strcpy(qst, question);
	char *const argv[] = { "dmenu", "-p", qst, NULL };

	int p[2];
	pid_t pid = start_process("dmenu", argv, p);
	close(p[1]);
	waitpid(pid, NULL, 0);
	free(qst);

	char *ans = malloc(*answersize);
	size_t anssize = *answersize;

	size_t len = 0;
	size_t remsize = anssize;
	size_t readlen;
	while ((readlen = read(p[0], ans + len, remsize)) == remsize) {
		len += remsize;
		remsize = anssize;
		ans = erealloc(ans, len + remsize);
	}
	if (readlen == -1)
		die(-2, "get_answer:");
	else if (readlen == 0)
		die(-2, "Unexpected EOF encountered");

	len += readlen;

	// TODO: Does not strip '   positive   '
	// TODO: Problem with 'positive;' and probably other invalid answers

	len = strip(ans, len, &ans);
	if (len + 2 > *answersize) {
		*answersize = len + 2;
		*answer = realloc(*answer, *answersize);
	}
	strncpy(*answer, ans, len);
	(*answer)[len] = ';';
	(*answer)[len + 1] = '\0';
}

static void send_maxima_cmd(const char *cmd)
{
	write(fmaxcmd, parsedcmd, strlen(parsedcmd));
}
static size_t read_maxima_out(char **buf, size_t *size)
{
	size_t len = 0;
	size_t remsize = *size;
	struct pollfd pfdin = { .fd = fmaxout, .events = POLLIN };
	do {
		int res = poll(&pfdin, 1, 100);
		if (res == -1) {
			die(-2, "poll:");
		} else if (!res) {
			die(1, "maxima timed out before delivering new input prompt");
		}

		size_t readlen = read(fmaxout, *buf + len, remsize);
		if (readlen == -1) {
			die(-2, "read_maxima_out:");
		} else if (readlen == 0) {
			die(-2, "unexpected EOF encountered");
		} else if (readlen == remsize) {
			len += remsize;
			remsize = READSIZE_OUT;
			*buf = erealloc(*buf, len + remsize);
			continue;
		}

		len += readlen;
		remsize -= readlen;
		/* No segfault because before decrement remsize > readlen holds.
		   This is necessary for prompt check */
		(*buf)[len] = '\0';
	} while (!has_prompt(*buf));

	*size = len + remsize;
	return len;
}
static void process_maxima_out(const int cmdtype, const char *cmd)
{
	maxout_t *out = alloc_maxima_out();
	if (!out)
		die(-1, "alloc_maxima_out:");

	size_t promptsize = inpromptsize;
	char *prompt = malloc(promptsize);
	prompttype_t prompttype;
	int err;

	/* Parse first output */
	outstrlen = read_maxima_out(&outstr, &outstrsize);

	err = parse_maxima_out(out, outstr, outstrlen);
	if (err)
		die(-1, "parse_maxima_out:");

	err = get_closing_prompt(out, &prompt, &promptsize, &prompttype);
	if (err == 1 || err == 2)
		die(1, "get_closing_prompt: no prompt in maxima output");
	else if (err)
		die(-1, "get_closing_prompt:");

	/* Write answers to maxima and parse additional output, as long as closing prompt is a question */
	size_t anssize = BUFSIZE_ANS;
	char *ans = emalloc(anssize);
	while (prompttype == PROMPT_QUESTION) {
		get_answer(prompt, &ans, &anssize);
		write(fmaxcmd, ans, strlen(ans));
		err = set_answer(out, ans);
		if (err == 1)
			die(2, "set_answer: no question to answer");
		else if (err)
			die(-1, "set_answer:");

		outstrlen = read_maxima_out(&outstr, &outstrsize);

		err = parse_maxima_out(out, outstr, outstrlen);
		if (err)
			die(-1, "parse_maxima_out:");

		err = get_closing_prompt(out, &prompt, &promptsize, &prompttype);
		if (err == 1 || err == 2)
			die(1, "get_closing_prompt: no prompt in maxima output");
		else if (err)
			die(-1, "get_closing_prompt:");
	}

	/* Write output to files */
	err = write_latex(latex_res_path, out, inprompt, cmd, cmdtype);
	if (err == 1)
		die(3, "write_latex: invalid maxima output");
	else if (err == 2)
		die(-2, "write_latex: could not write to `%s':", latex_res_path);
	else if (err)
		die(-1, "write_latex:");

	err = write_log(log_path, out, inprompt, parsedcmd);
	if (err == 1)
		die(3, "write_log: invalid maxima output");
	else if (err)
		die(-2, "write_log: could not write to `%s':", log_path);

	/* Update input prompt string */
	if (promptsize > inpromptsize) {
		inpromptsize = promptsize;
		char *p = realloc(inprompt, promptsize);
		if (!p)
			die(-1, "realloc:");
		inprompt = p;
	}
	strcpy(inprompt, prompt);

	free(ans);
	free(prompt);

	free_maxima_out(out);
}

static void start_maxima()
{
	int linkcmd[2];
	int linkout[2];
	
	pipe(linkcmd);
	pipe(linkout);
	pid_t pid = fork();
	if (pid == -1) {
		die(-2, "Could not fork\n");
	} else if (pid == 0) {
		/* maxima session */
		close(linkcmd[1]);
		close(linkout[0]);

		dup2(linkcmd[0], STDIN_FILENO);
		dup2(linkout[1], STDOUT_FILENO);
		close(linkcmd[0]);
		close(linkout[1]);

		execl("/usr/bin/maxima", "/usr/bin/maxima",
				"--quiet", "--init-lisp=init.lisp", NULL);
		die(-2, "execl: could not start maxima:");
	} else {
		/* parent */
		close(linkcmd[0]);
		close(linkout[1]);

		maxpid = pid;
		fmaxcmd = linkcmd[1];
		fmaxout = linkout[0];
	}
}
static void stop_maxima()
{
	close(fmaxout);
	close(fmaxcmd);

	if (waitpid(maxpid, NULL, 0) == -1)
		die(-2, "waitpid:");
}
void split_cmd(char* cmd, char *action, char *arg)
{
	char *s;
	int n = strip(cmd, strlen(cmd), &s);
	s[n] = '\0';

	char *d = strchr(s, '\n');
	if (!d) {
		strcpy(action, s);
		arg[0] = '\0';
	}
	else {
		*d = '\0';

		strcpy(action, s);
		strcpy(arg, d + 1);
	}
}
static int handle_com(const char *arg)
{
	if (!is_valid(arg)) {
		printf("invalid maxima command\n");
		return 1;
	}
	cmdtype_t cmdtype = preparse_cmd(arg, &parsedcmd, &parsedcmdsize);
	send_maxima_cmd(parsedcmd);
	process_maxima_out(cmdtype, arg);
	return 0;
}
static int handle_bat(const char *arg)
{
	FILE *fbat = fopen(arg, "r");
	if (!fbat) {
		printf("could not open batch file `%s'\n", arg);
		return 1;
	}

	ssize_t len;
	size_t size;
	char *line = emalloc(BUFSIZE_IN);
	int retval = 0;
	while ((len = getline(&line, &size, fbat)) != EOF) {
		char *s;
		int n = strip(line, len, &s);
		s[n] = '\0';

		if (!is_valid(s)) {
			printf("invalid maxima command\n");
			retval = 1;
			break;
		}
		cmdtype_t cmdtype = preparse_cmd(s, &parsedcmd, &parsedcmdsize);
		send_maxima_cmd(parsedcmd);
		process_maxima_out(cmdtype, s);
	}
	free(line);

	fclose(fbat);
	return retval;
}
static int handle_rst(const char *arg)
{
	stop_maxima();
	start_maxima();
	return 0;
}

static int npipe_read_in(char *action, char *arg)
{
	int fpipe = open(fifo_path, O_RDONLY);
	if (fpipe < 0)
		return 1;

	char *inbuf = emalloc(BUFSIZE_IN);
	size_t inlen = read(fpipe, inbuf, BUFSIZE_IN);
	if (inlen == -1)
		return 1;
	inbuf[inlen] = '\0';

	split_cmd(inbuf, action, arg);
	free(inbuf);

	if (close(fpipe) < 0)
		return 1;
	return 0;
}
static int npipe_write_err(int err)
{
	int fpipe = open(fifo_path, O_WRONLY);
	if (fpipe < 0)
		return 1;

	char serr[4];
	sprintf(serr, "%i", err);
	if (write(fpipe, serr, strlen(serr)) < 0)
		return 1;

	if (close(fpipe) < 0)
		return 1;
	return 0;
}
static void mainloop()
{
	char *action = emalloc(BUFSIZE_IN);
	char *arg = emalloc(BUFSIZE_IN);
	int err;
	int quit = 0;
	while (!quit) {
		if (npipe_read_in(action, arg))
			die(-2, "npipe_read_in:");

		if (strcmp(action, "com") == 0) {
			err = handle_com(arg);
		} else if (strcmp(action, "bat") == 0) {
			err = handle_bat(arg);
		} else if (strcmp(action, "rst") == 0) {
			err = handle_rst(arg);
		} else if (strcmp(action, "end") == 0) {
			err = 0;
			quit = 1;
		} else {
			printf("texmax command `%s' not recognized\n", action);
		}

		if (npipe_write_err(err))
			die(-2, "npipe_write_err:");
	}
	free(arg);
	free(action);
}
int main(int argc, char* argv[])
{
	/* start maxima */
	int p[2];
	start_process("maxima", maxima_args, p);
	fmaxcmd = p[1];
	fmaxout = p[0];

	/* read initial output */
	parsedcmdsize = BUFSIZE_IN;
	parsedcmd = emalloc(parsedcmdsize);
	parsedcmd[0] = '\0';

	outstrsize = READSIZE_OUT;
	outstr = emalloc(outstrsize);
	outstrlen = read_maxima_out(&outstr, &outstrsize);

	/* extract initial prompt */
	inpromptsize = outstrlen + 1;
	inprompt = emalloc(inpromptsize);

	maxout_t *out = alloc_maxima_out();
	if (!out)
		die(-1, "alloc_maxima_out:");

	int err = parse_maxima_out(out, outstr, outstrlen);
	if (err)
		die(-1, "parse_maxima_out:");

	prompttype_t pmttype;
	err = get_closing_prompt(out, &inprompt, &inpromptsize, &pmttype);
	if (err == 1)
		die(1, "get_closing_prompt: no output");
	else if (err == 2)
		die(1, "get_closing_prompt: no prompt in output");
	else if (err == 3)
		die(-1, "get_closing_prompt:");
	free_maxima_out(out);

	/* create input/output files */
	int cdirlen = strlen(cache_dir);
	fifo_path = emalloc(cdirlen + strlen(fifo_name) + 2);
	pathcat(cache_dir, fifo_name, &fifo_path);

	latex_doc_path = emalloc(cdirlen + strlen(latex_doc_name) + 2);
	pathcat(cache_dir, latex_doc_name, &latex_doc_path);

	latex_res_path = emalloc(cdirlen + strlen(latex_doc_name) + 2);
	pathcat(cache_dir, latex_res_name, &latex_res_path);

	log_path = emalloc(cdirlen + strlen(log_name) + 2);
	pathcat(cache_dir, log_name, &log_path);

	struct stat sb;
	if (stat(cache_dir, &sb) || !S_ISDIR(sb.st_mode)) {
		if (mkdir(cache_dir, 0755) < 0)
			die(-2, "Could not create `%s'\n", cache_dir);
	}
	if (create_latex_doc(latex_doc_path, latex_res_name)) {
		die(-2, "Could not create `%s'", latex_doc_path);
	}

	if (stat(fifo_path, &sb) || !S_ISFIFO(sb.st_mode)) {
		if (mkfifo(fifo_path, 0644) < 0)
			die(-2, "Could not create `%s'\n", fifo_name);
	}

	/* start accepting commands */
	mainloop();

	/* cleanup */
	if (!stat(fifo_path, &sb) && S_ISFIFO(sb.st_mode)) {
		if (unlink(fifo_path))
			die(-2, "Could not remove `%s'\n", fifo_path);
	}

	free(log_path);
	free(latex_res_path);
	free(latex_doc_path);
	free(fifo_path);

	free(inprompt);
	free(parsedcmd);
	free(outstr);
	return 0;
}
