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

#define BUFSIZE_PROMPT 512
#define BUFSIZE_IN 512
#define READSIZE_OUT 16384

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

static int remove_iofiles()
{
	int retval = 0;
	remove_plot_files();

	unlink(fifo_path);
	unlink(latex_doc_path);
	unlink(latex_res_path);
	unlink(log_path);
	rmdir(output_dir);

	free(log_path);
	free(latex_res_path);
	free(latex_doc_path);
	free(fifo_path);
	return retval;
}
static int create_iofiles()
{
	/* Do not free or remove files on error,
	   this is done later with a call to cleanup().
	   Especially do not free, as this leads to a double free! */

	/* create path strings*/
	int cdirlen = strlen(output_dir);
	fifo_path = malloc(cdirlen + strlen(fifo_name) + 2);
	if (!fifo_path)
		goto err_free;
	pathcat(output_dir, fifo_name, fifo_path);

	latex_doc_path = malloc(cdirlen + strlen(latex_doc_name) + 2);
	if (!latex_doc_path)
		goto err_free;
	pathcat(output_dir, latex_doc_name, latex_doc_path);

	latex_res_path = malloc(cdirlen + strlen(latex_doc_name) + 2);
	if (!latex_res_path)
		goto err_free;
	pathcat(output_dir, latex_res_name, latex_res_path);

	log_path = malloc(cdirlen + strlen(log_name) + 2);
	if (!log_path)
		goto err_free;
	pathcat(output_dir, log_name, log_path);

	/* create files */
	struct stat sb;
	if (mkdir(output_dir, 0755) == -1) {
		if (errno == EEXIST)
			fprintf(stderr, "Name `%s' of output directory does already exist\n", output_dir);
		return 1;
	}

	if (create_latex_doc(latex_doc_path, latex_res_path))
		goto err_del_files;

	if (create_latex_res(latex_res_path))
		goto err_del_files;

	if (mkfifo(fifo_path, 0644) == -1)
		goto err_del_files;

	return 0;

err_del_files:
	remove_iofiles();
	return -1;

err_free:
	free(log_path);
	free(latex_res_path);
	free(latex_doc_path);
	free(fifo_path);
	return -1;
}
static int cleanup()
{
	free(inprompt);
	free(parsedcmd);
	free(outstr);

	if (remove_iofiles())
		return 1;
	return 0;
}
static void handle_signal(int signo)
{
	if (signo == SIGINT) {
		if (cleanup())
			perror("cleanup");
		exit(0);
	}
}

static pid_t start_process(const char *file, char *const argv[], int fd[2])
{
	// TODO: error handling

	int pipein[2];
	int pipeout[2];

	if (pipe(pipein))
		return 1;
	if (pipe(pipeout))
		return 1;
	pid_t pid = fork();
	if (pid == -1) {
		return 1;
	} else if (pid == 0) {
		close(pipein[1]);
		close(pipeout[0]);

		dup2(pipein[0], STDIN_FILENO);
		dup2(pipeout[1], STDOUT_FILENO);
		close(pipein[0]);
		close(pipeout[1]);

		execvp(file, argv);
		fprintf(stderr, "execvp: could not execute `%s'", file);
		exit(-1);
	} else {
		close(pipein[0]);
		close(pipeout[1]);

		fd[0] = pipeout[0];
		fd[1] = pipein[1];
	}

	return pid;
}

static size_t read_maxima_out(char **buf, size_t *size)
{
	size_t len = 0;
	size_t remsize = *size;
	struct pollfd pfdin = { .fd = fmaxout, .events = POLLIN };
	do {
		int res = poll(&pfdin, 1, 100);
		if (!res)
			return -1;

		size_t readlen = read(fmaxout, *buf + len, remsize);
		if (readlen == -1 || readlen == 0) {
			return -1;
		} else if (readlen == remsize) {
			len += remsize;
			remsize = READSIZE_OUT;
			char *bufnew = realloc(*buf, len + remsize);
			if (!bufnew)
				return -1;
			*buf = bufnew;
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
static int get_answer(const char* question, char **answer, size_t *answersize)
{
	char *const qst = malloc(strlen(question) + 1);
	if (!qst)
		return -1;
	strcpy(qst, question);
	char *const argv[] = { "dmenu", "-p", qst, NULL };

	int p[2];
	pid_t pid = start_process("dmenu", argv, p);
	if (pid == -1) {
		free(qst);
		return -1;
	}
	close(p[1]);
	waitpid(pid, NULL, 0);
	free(qst);

	char *ans = malloc(*answersize);
	if (!ans)
		return -1;
	size_t anssize = *answersize;

	size_t len = 0;
	size_t remsize = anssize;
	size_t readlen;
	while ((readlen = read(p[0], ans + len, remsize)) == remsize) {

		len += remsize;
		remsize = anssize;
		char *ansnew = realloc(ans, len + remsize);
		if (!ansnew) {
			free(ans);
			return -1;
		}
		ans = ansnew;
	}
	if (readlen == -1) {
		free(ans);
		return -1;
	}

	len += readlen;

	// TODO: Does not strip '   positive   '
	// TODO: Problem with 'positive;' and probably other invalid answers

	if (len > 0 && ans[len - 1] == '\n')
		--len;
	if (len > 0 && ans[len - 1] == ';')
		--len;
	char *sans;
	size_t slen = strip(ans, len, &sans);
	if (len + 3 > *answersize) {
		*answersize = len + 2;
		char *answernew = realloc(*answer, *answersize);
		if (!answernew) {
			free(ans);
			return -1;
		}
		*answer = answernew;
	}
	strncpy(*answer, sans, slen);
	(*answer)[slen] = ';';
	(*answer)[slen + 1] = '\n';
	(*answer)[slen + 2] = '\0';
	free(ans);
	return 0;
}
static int send_maxima_cmd(const char *cmd)
{
	if (write(fmaxcmd, cmd, strlen(cmd)) == -1)
		return -1;
	return 0;
}
static int process_init_prompt()
{
	/* read initial output */
	outstrlen = read_maxima_out(&outstr, &outstrsize);
	if (outstrlen == -1)
		return -1;

	/* extract initial prompt */
	maxout_t *out = alloc_maxima_out();
	if (!out)
		return -1;

	if (parse_maxima_out(out, outstr, outstrlen)) {
		free_maxima_out(out);
		return -1;
	}

	prompttype_t pmttype;
	if (get_closing_prompt(out, &inprompt, &inpromptsize, &pmttype)) {
		free_maxima_out(out);
		return -1;
	}
	free_maxima_out(out);

	return 0;
}
static int process_maxima_out(const int cmdtype, const char *cmd)
{
	int retval = 0;

	maxout_t *out = alloc_maxima_out();
	if (!out)
		return -1;

	size_t promptsize = inpromptsize;
	char *prompt = malloc(promptsize);
	if (!prompt) {
		free_maxima_out(out);
		return -1;
	}
	prompttype_t prompttype;

	size_t anssize = BUFSIZE_ANS;
	char *ans = malloc(anssize);
	if (!ans) {
		free(prompt);
		free_maxima_out(out);
		return -1;
	}

	/* Parse first output */
	outstrlen = read_maxima_out(&outstr, &outstrsize);
	if (outstrlen == -1) {
		retval = -1;
		goto out_free;
	}

	if (parse_maxima_out(out, outstr, outstrlen)) {
		retval = -1;
		goto out_free;
	}

	if (get_closing_prompt(out, &prompt, &promptsize, &prompttype)) {
		retval = -1;
		goto out_free;
	}

	/* Write answers to maxima and parse additional output, as long as closing prompt is a question */
	while (prompttype == PROMPT_QUESTION) {
		if (get_answer(prompt, &ans, &anssize)) {
			retval = -1;
			goto out_free;
		}

		write(fmaxcmd, ans, strlen(ans));
		if (set_answer(out, ans)) {
			retval = -1;
			goto out_free;
		}

		outstrlen = read_maxima_out(&outstr, &outstrsize);
		if (outstrlen == -1) {
			retval = -1;
			goto out_free;
		}

		if (parse_maxima_out(out, outstr, outstrlen)) {
			retval = -1;
			goto out_free;
		}

		if (get_closing_prompt(out, &prompt, &promptsize, &prompttype)) {
			retval = -1;
			goto out_free;
		}
	}

	/* Write output to files */
	if (write_latex_res(latex_res_path, out, inprompt, cmd, cmdtype)) {
		retval = -1;
		goto out_free;
	}

	if (write_log(log_path, out, inprompt, cmd)) {
		retval = -1;
		goto out_free;
	}

	/* Update input prompt string */
	if (promptsize > inpromptsize) {
		inpromptsize = promptsize;
		char *p = realloc(inprompt, promptsize);
		if (!p) {
			retval = -1;
			goto out_free;
		}
		inprompt = p;
	}
	strcpy(inprompt, prompt);

out_free:
	free(ans);
	free(prompt);
	free_maxima_out(out);
	return retval;
}

static void split_cmd(char* cmd, char *action, char *arg)
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
static int handle_com(const char *arg, int *cmderr)
{
	if (ignore(arg)) {
		*cmderr = 0;
		return 0;
	} else if (!is_valid(arg)) {
		printf("`%s' is an invalid maxima command\n", arg);
		*cmderr = 1;
		return 0;
	}

	cmdtype_t cmdtype;
	if (preparse_cmd(arg, &parsedcmd, &parsedcmdsize, &cmdtype))
		return 1;

	if (send_maxima_cmd(parsedcmd))
		return 2;
	if (process_maxima_out(cmdtype, arg))
		return 3;

	*cmderr = 0;
	return 0;
}
static int handle_bat(const char *arg, int *cmderr)
{
	FILE *fbat = fopen(arg, "r");
	if (!fbat) {
		printf("could not open batch file `%s'\n", arg);
		*cmderr = 1;
		return 0;
	}

	ssize_t len;
	size_t size;
	char *line = malloc(BUFSIZE_IN);
	if (!line) {
		fclose(fbat);
		return -1;
	}
	while ((len = getline(&line, &size, fbat)) != EOF) {
		char *s;
		int n = strip(line, len, &s);
		s[n] = '\0';

		if (ignore(s)) {
			continue;
		} else if (!is_valid(s)) {
			printf("`%s' is an invalid maxima command\n", s);
			*cmderr = 1;
			break;
		}

		cmdtype_t cmdtype;
		if (preparse_cmd(s, &parsedcmd, &parsedcmdsize, &cmdtype)) {
			free(line);
			fclose(fbat);
			return -1;
		}

		if (send_maxima_cmd(parsedcmd)) {
			free(line);
			fclose(fbat);
			return -1;
		}
		if (process_maxima_out(cmdtype, s)) {
			free(line);
			fclose(fbat);
			return -1;
		}
	}
	free(line);

	if (fclose(fbat) == EOF)
		return -1;

	*cmderr = 0;
	return 0;
}
static int handle_cls()
{
	if (create_latex_res(latex_res_path))
		return -1;
	return 0;
}
static int handle_rst()
{
	/* Stop process */
	if (close(fmaxout) == -1)
		return -1;
	if (close(fmaxcmd) == -1)
		return -1;

	if (waitpid(maxpid, NULL, 0) == -1)
		return -1;

	/* Restart process */
	int p[2];
	maxpid = start_process("maxima", maxima_args, p);
	if (maxpid == -1)
		return -1;
	fmaxcmd = p[1];
	fmaxout = p[0];

	/* Handle init prompt */
	if (process_init_prompt())
		return -1;

	return 0;
}

static int npipe_read_in(char *action, char *arg)
{
	int fpipe = open(fifo_path, O_RDONLY);
	if (fpipe == -1)
		return 1;

	char *inbuf = malloc(BUFSIZE_IN);
	if (!inbuf) {
		close(fpipe);
		return 1;
	}
	size_t inlen = read(fpipe, inbuf, BUFSIZE_IN);
	if (inlen == -1)
		return 1;
	inbuf[inlen] = '\0';

	split_cmd(inbuf, action, arg);
	free(inbuf);

	if (close(fpipe) == -1)
		return 1;
	return 0;
}
static int npipe_write_err(int err)
{
	int fpipe = open(fifo_path, O_WRONLY);
	if (fpipe == -1)
		return 1;

	char serr[4];
	sprintf(serr, "%i", err);
	if (write(fpipe, serr, strlen(serr)) == -1)
		return 1;

	if (close(fpipe) == -1)
		return 1;
	return 0;
}
static void mainloop()
{
	char *action = malloc(BUFSIZE_IN);
	if (!action) {
		perror("malloc");
		cleanup();
		exit(-1);
	}
	char *arg = malloc(BUFSIZE_IN);
	if (!arg) {
		free(action);
		perror("malloc");
		cleanup();
		exit(-1);
	}

	int err;
	int cmderr;
	while (1) {
		if (npipe_read_in(action, arg)) {
			perror("npipe_read_in");
			goto err_free;
		}

		if (strcmp(action, "com") == 0) {
			err = handle_com(arg, &cmderr);
		} else if (strcmp(action, "bat") == 0) {
			err = handle_bat(arg, &cmderr);
		} else if (strcmp(action, "cls") == 0) {
			cmderr = 0;
			err = handle_cls();
		} else if (strcmp(action, "rst") == 0) {
			cmderr = 0;
			err = handle_rst();
		} else if (strcmp(action, "end") == 0) {
			break;
		} else {
			printf("texmax command `%s' not recognized\n", action);
		}
		if (err) {
			fprintf(stderr, "handle_%s:", action);
			perror(NULL);
			goto err_free;
		}

		if (npipe_write_err(cmderr)) {
			perror("npipe_write_err");
			goto err_free;
		}
	}
	free(arg);
	free(action);
	return;

err_free:
	free(arg);
	free(action);
	cleanup();
	exit(-1);
}
int main(int argc, char* argv[])
{
	if (signal(SIGINT, handle_signal) == SIG_ERR) {
		perror("signal");
		return -1;
	}

	int err = create_iofiles();
	if (err == -1) {
		perror("create_iofiles");
		return -1;
	} else if (err == 1) {
		return -1;
	}

	/* start maxima */
	int p[2];
	maxpid = start_process("maxima", maxima_args, p);
	if (maxpid == -1) {
		perror("start_process");
		cleanup();
		return -1;
	}
	fmaxcmd = p[1];
	fmaxout = p[0];

	/* allocate memory for maxima output */
	parsedcmdsize = BUFSIZE_IN;
	parsedcmd = malloc(parsedcmdsize);
	if (!parsedcmd) {
		perror("malloc");
		cleanup();
		return -1;
	}
	parsedcmd[0] = '\0';

	outstrsize = READSIZE_OUT;
	outstr = malloc(outstrsize);
	if (!outstr) {
		perror("malloc");
		cleanup();
		return -1;
	}

	inpromptsize = BUFSIZE_PROMPT;
	inprompt = malloc(inpromptsize);
	if (!inprompt) {
		perror("malloc");
		cleanup();
		return -1;
	}

	/* Start loop after handling initial prompt */
	if (process_init_prompt()) {
		perror("process_init_prompt");
		cleanup();
		return -1;
	}
	mainloop();

	if (cleanup())
		return -1;
	return 0;
}
