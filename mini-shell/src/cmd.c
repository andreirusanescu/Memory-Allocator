// SPDX-License-Identifier: BSD-3-Clause
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

int setenv(const char *name, const char *value, int overwrite);

/**
 * Internal change-directory command.
 * Execute cd.
 */
static bool shell_cd(word_t *dir)
{
	if (!dir)
		return true;

	/* change directory to HOME */
	if (!strcmp(dir->string, "~") || !strcmp(dir->string, "-")) {
		char *cwd = getcwd(NULL, 0);
		char *next_pwd = (dir->string[0] == '~' ? getenv("HOME") : getenv("OLDPWD"));

		DIE(!cwd, "getcwd() failed");
		DIE(!next_pwd, "getenv() failed");

		if (chdir(next_pwd) == -1) {
			free(cwd);
			return false;
		}

		DIE(setenv("OLDPWD", cwd, 1) == -1, "setenv() failed");
		DIE(setenv("PWD", next_pwd, 1) == -1, "setenv() failed");

		free(cwd);
		return true;
	}

	/* cd with a custom path */
	char *old_pwd = getcwd(NULL, 0);

	DIE(!old_pwd, "getenv() failed");

	/* Path does not exist */
	if (chdir(dir->string) == -1) {
		free(old_pwd);
		return false;
	}

	char *cwd = getcwd(NULL, 0);

	DIE(!cwd, "getcwd() failed");
	DIE(setenv("OLDPWD", old_pwd, 1) == -1, "setenv() failed");
	DIE(setenv("PWD", cwd, 1) == -1, "setenv() failed");
	free(old_pwd);
	free(cwd);

	return true;
}

/**
 * Internal exit/quit command.
 * Execute exit/quit.
 */
static int shell_exit(void)
{
	return SHELL_EXIT;
}

static int saved_stdin, saved_stdout, saved_stderr;

static void save_streams(void)
{
	saved_stdin = dup(STDIN_FILENO);
	DIE(saved_stdin < 0, "dup() failed");

	saved_stdout = dup(STDOUT_FILENO);
	DIE(saved_stdout < 0, "dup() failed");

	saved_stderr = dup(STDERR_FILENO);
	DIE(saved_stderr < 0, "dup() failed");
}

static void reset_streams(void)
{
	int rc;

	rc = dup2(saved_stdin, STDIN_FILENO);
	DIE(rc < 0, "dup2() failed");

	rc = dup2(saved_stdout, STDOUT_FILENO);
	DIE(rc < 0, "dup2() failed");

	rc = dup2(saved_stderr, STDERR_FILENO);
	DIE(rc < 0, "dup2() failed");

	rc = close(saved_stdin);
	DIE(rc < 0, "close() failed");

	rc = close(saved_stdout);
	DIE(rc < 0, "close() failed");

	rc = close(saved_stderr);
	DIE(rc < 0, "close() failed");
}

static void set_streams(simple_command_t *s)
{
	int fd, rc;

	if (s->in) {
		fd = open(get_word(s->in), O_RDONLY | O_CREAT, 0644);
		DIE(fd < 0, "open() failed");
		rc = dup2(fd, 0);
		DIE(rc < 0, "dup2() failed");
		rc = close(fd);
		DIE(rc < 0, "close() failed");
	}

	if (s->out && s->err && !strcmp(s->out->string, s->err->string)) {
		fd = open(get_word(s->out), O_RDWR | O_CREAT | O_TRUNC, 0644);
		DIE(fd < 0, "open() failed");

		rc = dup2(fd, 1);
		DIE(rc < 0, "dup2() failed");

		rc = dup2(fd, 2);
		DIE(rc < 0, "dup2() failed");

		rc = close(fd);
		DIE(rc < 0, "close() failed");
		return;
	}

	if (s->out) {
		if (s->io_flags & IO_OUT_APPEND)
			fd = open(get_word(s->out), O_RDWR | O_CREAT | O_APPEND, 0644);
		else
			fd = open(get_word(s->out), O_RDWR | O_CREAT | O_TRUNC, 0644);
		DIE(fd < 0, "open() failed");

		rc = dup2(fd, 1);
		DIE(rc < 0, "dup2() failed");
		rc = close(fd);
		DIE(rc < 0, "close() failed");
	}

	if (s->err) {
		if (s->io_flags & IO_ERR_APPEND)
			fd = open(get_word(s->err), O_RDWR | O_CREAT | O_APPEND, 0644);
		else
			fd = open(get_word(s->err), O_RDWR | O_CREAT | O_TRUNC, 0644);
		DIE(fd < 0, "open() failed");

		rc = dup2(fd, 2);
		DIE(rc < 0, "dup2() failed");
		rc = close(fd);
		DIE(rc < 0, "close() failed");
	}
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* Sanity checks. */
	if (!s)
		return SHELL_EXIT;

	/* If builtin command, execute the command. */
	if (!strcmp(s->verb->string, "exit") || !strcmp(s->verb->string, "quit"))
		return shell_exit();

	if (!strcmp(s->verb->string, "cd")) {
		save_streams();
		set_streams(s);
		int ret = shell_cd(s->params);

		reset_streams();

		if (!ret)
			printf("cd: no such file or directory: %s\n", s->params->string);

		return ret == false ? 1 : 0;
	}

	/* If variable assignment, execute the assignment and
	 * return the exit status.
	 */
	if (s->verb->next_part && !strcmp(s->verb->next_part->string, "=")) {
		/* Assigning a variable to a value */
		if (s->verb->next_part->next_part->expand == false) {
			DIE(setenv(s->verb->string, s->verb->next_part->next_part->string, 1) < 0, "setenv() failed");
			return 0;
		}

		/* Assigning a variable to another variable */

		word_t *start_parts = s->verb->next_part->next_part;
		char *string = get_word(start_parts);

		DIE(setenv(s->verb->string, string, 1) == -1, "setenv() failed");
		free(string);
		return 0;
	}

	/* If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */

	pid_t pid, wait;
	int status, rc, size;
	char **args;

	pid = fork();

	switch (pid) {
	case -1:
		DIE(1, "fork() failed");
		break;
	case 0:
		args = get_argv(s, &size);
		save_streams();
		set_streams(s);
		rc = execvp(args[0], (char *const *)args);
		reset_streams();
		if (rc == -1) {
			printf("Execution failed for '%s'\n", args[0]);
			exit(1);
		}
		exit(0);
	default:
		wait = waitpid(pid, &status, 0);
		DIE(wait < 0, "waitpid() failed");

		break;
	}

	return WEXITSTATUS(status);
}

/**
 * Process two commands in parallel, by creating two children.
 * Execute cmd1 and cmd2 simultaneously.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	pid_t pid1, pid2, wait;
	int status1, status2;

	pid1 = fork();

	switch (pid1) {
	case -1:
		DIE(1, "fork() failed");
		break;
	case 0:
		exit(parse_command(cmd1, level, father));
	}

	pid2 = fork();

	switch (pid2) {
	case -1:
		DIE(1, "fork() failed");
		break;
	case 0:
		exit(parse_command(cmd2, level, father));
	}

	wait = waitpid(pid1, &status1, 0);
	DIE(wait < 0, "waitpid() failed");

	wait = waitpid(pid2, &status2, 0);
	DIE(wait < 0, "waitpid() failed");

	return WEXITSTATUS(status1) && WEXITSTATUS(status2);
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 * Redirect the output of cmd1 to the input of cmd2.
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int pipe_fds[2], status1, status2, rc;
	pid_t pid1, pid2, wait;

	DIE(pipe(pipe_fds) < 0, "pipe() failed");

	pid1 = fork();
	switch (pid1) {
	case -1:
		DIE(1, "fork() failed");
	case 0:
		rc = close(pipe_fds[0]);
		DIE(rc < 0, "close() failed");

		rc = dup2(pipe_fds[1], STDOUT_FILENO);
		DIE(rc < 0, "dup2() failed");

		rc = close(pipe_fds[1]);
		DIE(rc < 0, "close() failed");
		exit(parse_command(cmd1, level, father));
	}

	pid2 = fork();
	switch (pid2) {
	case -1:
		DIE(1, "fork() failed");
	case 0:
		rc = close(pipe_fds[1]);
		DIE(rc < 0, "close() failed");

		rc = dup2(pipe_fds[0], STDIN_FILENO);
		DIE(rc < 0, "dup2() failed");

		rc = close(pipe_fds[0]);
		DIE(rc < 0, "close() failed");
		exit(parse_command(cmd2, level, father));
	}

	rc = close(pipe_fds[0]);
	DIE(rc < 0, "close() failed");

	rc = close(pipe_fds[1]);
	DIE(rc < 0, "close() failed");

	wait = waitpid(pid1, &status1, 0);
	DIE(wait < 0, "waitpid() failed");

	wait = waitpid(pid2, &status2, 0);
	DIE(wait < 0, "waitpid() failed");

	return WEXITSTATUS(status2);
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	if (!c)
		return 0;

	int ret = 0;

	/* Execute a simple command */
	if (c->op == OP_NONE)
		return parse_simple(c->scmd, level, c);

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* Execute the commands one after the other. */
		parse_command(c->cmd1, level + 1, c);
		return parse_command(c->cmd2, level + 1, c);

	case OP_PARALLEL:
		/* Execute the commands simultaneously. */
		return run_in_parallel(c->cmd1, c->cmd2, level + 1, c);
	case OP_CONDITIONAL_NZERO:
		/* Execute the second command only if the first one
		 * returns non zero.
		 */
		ret = parse_command(c->cmd1, level + 1, c);
		return ret != 0 ? parse_command(c->cmd2, level + 1, c) : ret;

	case OP_CONDITIONAL_ZERO:
		/* Execute the second command only if the first one
		 * returns zero.
		 */
		ret = parse_command(c->cmd1, level + 1, c);
		return ret == 0 ? parse_command(c->cmd2, level + 1, c) : ret;

	case OP_PIPE:
		/* Redirect the output of the first command to the
		 * input of the second.
		 */
		return run_on_pipe(c->cmd1, c->cmd2, level + 1, c);

	default:
		return SHELL_EXIT;
	}

	return ret;
}
