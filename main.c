#include <ctype.h>
#include <errno.h>
#include <fcntl.h> 
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "parse.h"
#include "util.h"
#include "str.h"

#define BUFSIZE_OUT 16384
#define BUFSIZE_IN 512

#define BUFSIZE_ANS 16

static char *fifo_path;
static char *latex_doc_path;
static char *latex_res_path;
static char *log_path;

static pid_t maxpid;
static int fmaxcmd;
static int fmaxout;

static char *outbuf;
static size_t outlen;
static size_t outsize;

static char *cmdprompt;
static size_t cmdpromptsize;
static char *pcmd;
static size_t pcmdsize;

static int maxerr;

static size_t read_single_maxima_out(char *outbuf, size_t *outsize)
{
	// TODO: Read all output until maybe either prompt or timeout

	size_t outlen = 0;

	size_t readlen;
	size_t readsize = *outsize;
	size_t readoff = 0;
	while ((readlen = read(fmaxout, outbuf + readoff, readsize)) == readsize) {
		outlen += readlen;
		*outsize += readsize;
		outbuf = erealloc(outbuf, *outsize);

		readoff += readlen;
	}
	outlen += readlen;
	outbuf[outlen] = '\0';
	return outlen;
}
static size_t read_maxima_out(char *outbuf, size_t *outsize)
{
	char *buf = outbuf;
	size_t size = *outsize;
	size_t singlelen = read_single_maxima_out(buf, &size);
	size_t len = singlelen;
	while (!raw_out_has_prompt(buf)) {
		buf += len;
		size -= len;
		singlelen = read_maxima_out(buf, &size);
		len += singlelen;
	}
	*outsize = size;
	return len;
}
static void get_answer(char* question, char **ans)
{
	const char* _ans = "yes";
	strcpy(*ans, _ans);
}

static void send_maxima_cmd(const char *cmd)
{
	write(fmaxcmd, pcmd, strlen(pcmd));
}
static int process_continuous_maxima_out(const int cmdtype, const char *cmd, int *question)
{
	outlen = read_maxima_out(outbuf, &outsize);

	maxout *pout = parse_maxima_out(outbuf, outlen, cmdtype);
	if (write_latex_res(latex_res_path, pout, pcmd, cmdprompt, cmdtype)) {
		die("Could not write to `%s'", latex_res_path);
	}
	if (write_log(log_path, pout, cmd, cmdprompt)) {
		die("Could not write to `%s'", log_path);
	}

	get_closing_prompt(pout, cmdprompt, &cmdpromptsize);
	*question = has_closing_question(pout);
	int err = get_error(pout);
	free_maxima_out(pout);

	return err;
}
static int process_maxima_out(const int cmdtype, const char *cmd)
{
	char *ans = malloc(BUFSIZE_ANS);
	int err, question;
	while (!(err = process_continuous_maxima_out(cmdtype, cmd, &question)) && question) {
		get_answer(cmdprompt, &ans);
		write(fmaxcmd, ans, strlen(ans));
	}
	free(ans);
	return err;
}

static void start_maxima()
{
	int linkcmd[2];
	int linkout[2];
	
	pipe(linkcmd);
	pipe(linkout);
	pid_t pid = fork();
	if (pid == -1) {
		die("Could not fork\n");
	} else if (pid == 0) {
		/* maxima session */
		close(linkcmd[1]);
		close(linkout[0]);

		dup2(linkcmd[0], STDIN_FILENO);
		dup2(linkout[1], STDOUT_FILENO);
		close(linkcmd[0]);
		close(linkout[1]);

		execl("/usr/bin/maxima", "maxima",
				"--quiet", "--init-lisp=init.lisp", NULL);
		exit(0);
	} else {
		/* front session */
		close(linkcmd[0]);
		close(linkout[1]);

		maxpid = pid;
		fmaxcmd = linkcmd[1];
		fmaxout = linkout[0];
	}
}
static void stop_maxima()
{
	// TODO: Does not work, why?
	char c = EOF;
	write(fmaxcmd, &c, 1);

	int res;
	while ((res = kill(maxpid, 0)) != -1)
		printf("killres: %i\n", res);

	close(fmaxcmd);
	close(fmaxout);
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
static void handle_com(const char *arg)
{
	cmdtype_t cmdtype = preparse_cmd(arg, pcmd, &pcmdsize);
	if (cmdtype == CMDTYPE_INVALID) {
		printf("Invalid maxima command");
		maxerr = 1;
		return;
	}

	send_maxima_cmd(pcmd);
	maxerr = process_maxima_out(cmdtype, arg);
}
static void handle_bat(const char *arg)
{
	FILE *fbat = fopen(arg, "r");
	if (!fbat) {
		printf("Could not open batch file `%s'\n", arg);
		return;
	}

	int len;
	size_t size;
	char *line = emalloc(BUFSIZE_IN);
	int n;
	char *s;
	while (!maxerr && (len = getline(&line, &size, fbat)) != EOF) {
		n = strip(line, len, &s);
		s[n] = '\0';

		cmdtype_t cmdtype = preparse_cmd(s, pcmd, &pcmdsize);
		send_maxima_cmd(pcmd);
		maxerr = process_maxima_out(cmdtype, s);
	}
	free(line);

	fclose(fbat);
}
static void handle_rst(const char *arg)
{
	stop_maxima();
	start_maxima();
}

static int npipe_read_in(char *action, char *arg)
{
	int fpipe = open(fifo_path, O_RDONLY);
	if (fpipe < 0)
		return 1;

	/* Just for testing */
	//strcpy(inbuf, "com\nx:1;");

	char *inbuf = emalloc(BUFSIZE_IN);
	size_t inlen = read(fpipe, inbuf, BUFSIZE_IN);
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
	write(fpipe, serr, strlen(serr));

	if (close(fpipe) < 0)
		return 1;
	return 0;
}
static int mainloop()
{
	outsize = BUFSIZE_OUT;
	outbuf = emalloc(outsize);

	pcmdsize = BUFSIZE_IN;
	pcmd = emalloc(pcmdsize);
	pcmd[0] = '\0';

	outlen = read_maxima_out(outbuf, &outsize);

	cmdpromptsize = outlen + 1;
	cmdprompt = emalloc(cmdpromptsize);

	maxout *pout = parse_maxima_out(outbuf, outlen, CMDTYPE_MATH);
	get_closing_prompt(pout, cmdprompt, &cmdpromptsize);
	free_maxima_out(pout);

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
			die("Could not create `%s'\n", cache_dir);
	}
	if (create_latex_doc(latex_doc_path, latex_res_name)) {
		die("Could not create `%s'", latex_doc_path);
	}

	if (stat(fifo_path, &sb) || !S_ISFIFO(sb.st_mode)) {
		if (mkfifo(fifo_path, 0644) < 0)
			die("Could not create `%s'\n", fifo_name);
	}

	char *action = emalloc(BUFSIZE_IN);
	char *arg = emalloc(BUFSIZE_IN);
	while (1) {
		if (npipe_read_in(action, arg)) {
			fprintf(stderr, "Could not read input\n");
			exit(1);
		}

		//printf("action: %s\n", action);
		//printf("arg:\n%s\n", arg);
		
		if (strcmp(action, "com") == 0) {
			handle_com(arg);
		} else if (strcmp(action, "bat") == 0) {
			handle_bat(arg);
		} else if (strcmp(action, "rst") == 0) {
			handle_rst(arg);
		} else {
			printf("Action `%s' not recognized\n", action);
		}

		/* Just for testing */
		// exit(0);

		if (npipe_write_err(maxerr)) {
			fprintf(stderr, "Could not write result\n");
			exit(1);
		}
	}

	if (!stat(fifo_path, &sb) && S_ISFIFO(sb.st_mode)) {
		if (unlink(fifo_path))
			die("Could not remove `%s'\n", fifo_path);
	}

	free(arg);
	free(action);

	free(log_path);
	free(latex_res_path);
	free(latex_doc_path);
	free(fifo_path);

	free(cmdprompt);
	free(pcmd);
	free(outbuf);
}
int main(int argc, char* argv[])
{
	start_maxima();
	mainloop();
	return 0;
}
