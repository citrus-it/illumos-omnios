/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright 2026 Oxide Computer Company
 */

/*
 * Shell Escape I/O Backend
 *
 * The MDB parser implements two forms of shell escapes: (1) traditional adb(1)
 * style shell escapes of the form "!command", which simply allows the user to
 * invoke a command (or shell pipeline) as if they had executed sh -c command
 * and then return to the debugger, and (2) shell pipes of the form "dcmds !
 * command", in which the output of one or more MDB dcmds is sent as standard
 * input to a shell command (or shell pipeline).  Form (1) can be handled
 * entirely from the parser by calling mdb_shell_exec (below); it simply
 * spawns the shell, executes the desired command, and waits for completion.
 * Form (2) is slightly more complicated: we construct a UNIX pipe, spawn
 * the shell, and then build an fdio object out of the write end of the pipe.
 * We then layer a shellio object (implemented below) and iob on top, and
 * set mdb.m_out to point to this new iob.  The shellio is simply a pass-thru
 * to the fdio, except that its io_close routine performs a waitpid for the
 * spawned child process.
 *
 * The parser also implements two corresponding forms of shell input escape,
 * in which the standard output of the shell command is captured and then
 * evaluated by the debugger as if it had been read from a macro file:
 * (3) "!<command", which executes the command and then sources its output,
 * and (4) "dcmds !< command", in which the output of one or more dcmds is
 * first sent as standard input to the shell command, as in form (2).
 * In both cases the child's standard output is captured in an unlinked
 * temporary file rather than a pipe: the child can never block writing its
 * results, so no deadlock can arise between the debugger feeding the child
 * and the child feeding the debugger. Form (3) is handled entirely from the
 * parser by mdb_shell_source. For form (4), the captured output cannot be
 * evaluated until the dcmd pipeline has run and the child has exited, so
 * mdb_shell_pipe_source just records the capture; mdb_call() invokes
 * mdb_shell_source_run() once the statement has completed (the frame reset
 * destroys the shellio, which waits for the child), and the error paths in
 * mdb_run() invoke mdb_shell_source_discard() to drop a pending capture if
 * the statement instead fails.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <spawn.h>

#include <mdb/mdb_shell.h>
#include <mdb/mdb_lex.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb.h>

extern char **environ;

/*
 * Spawn the shell to run the given command.  close_fd, when not -1, is
 * closed in the child first; fd 0 is then replaced by stdin_fd and fd 1 by
 * stdout_fd when those are not -1. Descriptors above stderr are closed in
 * the child by a file action rather than by manipulating our own fd flags;
 * posix_spawn() reports a failure to exec back to us directly, so there is
 * no need to preserve descriptors for a child-side error message.
 */
static int
shell_spawn(char *cmd, int stdin_fd, int stdout_fd, int close_fd, pid_t *pidp)
{
	posix_spawn_file_actions_t fact;
	int err;
	char *argv[] = {
	    (char *)strbasename(mdb.m_shell), "-c", cmd, NULL };

	if ((err = posix_spawn_file_actions_init(&fact)) != 0)
		return (err);

	if (close_fd != -1)
		err = posix_spawn_file_actions_addclose(&fact, close_fd);
	if (err == 0 && stdin_fd != -1) {
		err = posix_spawn_file_actions_adddup2(&fact, stdin_fd,
		    STDIN_FILENO);
	}
	if (err == 0 && stdout_fd != -1) {
		err = posix_spawn_file_actions_adddup2(&fact, stdout_fd,
		    STDOUT_FILENO);
	}
	if (err == 0) {
		err = posix_spawn_file_actions_addclosefrom_np(&fact,
		    STDERR_FILENO + 1);
	}
	if (err == 0) {
		err = posix_spawnp(pidp, mdb.m_shell, &fact, NULL,
		    argv, environ);
	}

	(void) posix_spawn_file_actions_destroy(&fact);
	return (err);
}

void
mdb_shell_exec(char *cmd)
{
	int err, status;
	pid_t pid;

	if (access(mdb.m_shell, X_OK) == -1)
		yyperror("cannot access %s", mdb.m_shell);

	if ((err = shell_spawn(cmd, -1, -1, -1, &pid)) != 0) {
		errno = err;
		yyperror("failed to exec %s", mdb.m_shell);
	}

	do {
		mdb_dprintf(MDB_DBG_SHELL, "waiting for PID %d\n", (int)pid);
	} while (waitpid(pid, &status, 0) == -1 && errno == EINTR);

	mdb_dprintf(MDB_DBG_SHELL, "waitpid %d -> 0x%x\n", (int)pid, status);
	strfree(cmd);
}

/*
 * This use of the io_unlink entry point is a little strange: we have stacked
 * the shellio on top of the fdio, but before the shellio's close routine can
 * wait for the child process, we need to close the UNIX pipe file descriptor
 * in order to generate an EOF to terminate the child.  Since each io is
 * unlinked from its iob before being popped by mdb_iob_destroy, we use the
 * io_unlink entry point to release the underlying fdio (forcing its io_close
 * routine to be called) and remove it from the iob's i/o stack out of order.
 */

/*ARGSUSED*/
static void
shellio_unlink(mdb_io_t *io, mdb_iob_t *iob)
{
	mdb_io_t *fdio = io->io_next;

	ASSERT(iob->iob_iop == io);
	ASSERT(fdio != NULL);

	io->io_next = fdio->io_next;
	fdio->io_next = NULL;
	mdb_io_rele(fdio);
}

static void
shellio_close(mdb_io_t *io)
{
	pid_t pid = (pid_t)(intptr_t)io->io_data;
	int status;

	do {
		mdb_dprintf(MDB_DBG_SHELL, "waiting for PID %d\n", (int)pid);
	} while (waitpid(pid, &status, 0) == -1 && errno == EINTR);

	mdb_dprintf(MDB_DBG_SHELL, "waitpid %d -> 0x%x\n", (int)pid, status);
}

static const mdb_io_ops_t shellio_ops = {
	.io_read = no_io_read,
	.io_write = no_io_write,
	.io_seek = no_io_seek,
	.io_ctl = no_io_ctl,
	.io_close = shellio_close,
	.io_name = no_io_name,
	.io_link = no_io_link,
	.io_unlink = shellio_unlink,
	.io_setattr = no_io_setattr,
	.io_suspend = no_io_suspend,
	.io_resume = no_io_resume
};

/*
 * Construct a pipe to a spawned shell command and redirect subsequent
 * debugger output into it. When stdout_fd is not -1 the child's standard
 * output is additionally redirected there; on failure the descriptor is
 * closed before the error is reported, so the caller need not clean up.
 */
static void
shell_pipe(char *cmd, int stdout_fd)
{
	uint_t iflag = mdb_iob_getflags(mdb.m_out) & MDB_IOB_INDENT;
	mdb_iob_t *iob;
	mdb_io_t *io;
	int err, pfds[2];
	pid_t pid;

	if (access(mdb.m_shell, X_OK) == -1) {
		if (stdout_fd != -1)
			(void) close(stdout_fd);
		yyperror("cannot access %s", mdb.m_shell);
	}

	if (pipe(pfds) == -1) {
		if (stdout_fd != -1)
			(void) close(stdout_fd);
		yyperror("failed to open pipe");
	}

	iob = mdb_iob_create(mdb_fdio_create(pfds[1]), MDB_IOB_WRONLY | iflag);
	mdb_iob_clrflags(iob, MDB_IOB_AUTOWRAP | MDB_IOB_INDENT);
	mdb_iob_resize(iob, BUFSIZ, BUFSIZ);

	if ((err = shell_spawn(cmd, pfds[0], stdout_fd, pfds[1], &pid)) != 0) {
		(void) close(pfds[0]);
		(void) close(pfds[1]);
		if (stdout_fd != -1)
			(void) close(stdout_fd);
		mdb_iob_destroy(iob);
		errno = err;
		yyperror("failed to exec %s", mdb.m_shell);
	}

	(void) close(pfds[0]);
	strfree(cmd);

	io = mdb_alloc(sizeof (mdb_io_t), UM_SLEEP);

	io->io_ops = &shellio_ops;
	io->io_data = (void *)(intptr_t)pid;
	io->io_next = NULL;
	io->io_refcnt = 0;

	mdb_iob_stack_push(&mdb.m_frame->f_ostk, mdb.m_out, yylineno);
	mdb_iob_push_io(iob, io);
	mdb.m_out = iob;
}

void
mdb_shell_pipe(char *cmd)
{
	shell_pipe(cmd, -1);
}

/*
 * The file descriptor of the unlinked temporary file holding the captured
 * standard output of a pending "dcmds !< command" escape, or -1 when no
 * capture is outstanding. There can be at most one: the parser permits one
 * shell escape per statement, and mdb_call() consumes the capture before the
 * sourced commands can themselves create another.
 */
static int shell_src_fd = -1;

/*
 * Create an unlinked temporary file to receive a child's standard output.
 */
static int
shell_tmpfd(void)
{
	char tmpl[] = "/tmp/mdb.XXXXXX";
	int fd;

	if ((fd = mkstemp(tmpl)) == -1)
		return (-1);

	(void) unlink(tmpl);
	return (fd);
}

/*
 * Evaluate the captured output in the given file descriptor as debugger
 * input, in the manner of $<<. Ownership of the descriptor passes to the
 * iob; it is closed when the input is exhausted. The error handling here
 * mirrors cmd_src_file(): errors that must abort or unwind past this
 * statement are propagated to the current frame.
 */
static void
shell_source_eval(int fd)
{
	mdb_frame_t *fp = mdb.m_frame;
	int err;

	if (lseek(fd, 0, SEEK_SET) == -1) {
		(void) close(fd);
		yyperror("failed to rewind temporary file");
	}

	mdb_iob_stack_push(&fp->f_istk, mdb.m_in, yylineno);
	mdb.m_in = mdb_iob_create(mdb_fdio_create(fd), MDB_IOB_RDONLY);
	err = mdb_run();

	ASSERT(fp == mdb.m_frame);
	mdb.m_in = mdb_iob_stack_pop(&fp->f_istk);
	yylineno = mdb_iob_lineno(mdb.m_in);

	if (err == MDB_ERR_PAGER && mdb.m_fmark != fp)
		longjmp(fp->f_pcb, err);

	if (err == MDB_ERR_QUIT || err == MDB_ERR_ABORT ||
	    err == MDB_ERR_SIGINT || err == MDB_ERR_OUTPUT)
		longjmp(fp->f_pcb, err);
}

void
mdb_shell_source(char *cmd)
{
	int err, fd, status;
	pid_t pid;

	if (access(mdb.m_shell, X_OK) == -1)
		yyperror("cannot access %s", mdb.m_shell);

	if ((fd = shell_tmpfd()) == -1)
		yyperror("failed to create temporary file");

	if ((err = shell_spawn(cmd, -1, fd, -1, &pid)) != 0) {
		(void) close(fd);
		errno = err;
		yyperror("failed to exec %s", mdb.m_shell);
	}

	strfree(cmd);

	do {
		mdb_dprintf(MDB_DBG_SHELL, "waiting for PID %d\n", (int)pid);
	} while (waitpid(pid, &status, 0) == -1 && errno == EINTR);

	mdb_dprintf(MDB_DBG_SHELL, "waitpid %d -> 0x%x\n", (int)pid, status);

	shell_source_eval(fd);
}

void
mdb_shell_pipe_source(char *cmd)
{
	int fd;

	if ((fd = shell_tmpfd()) == -1)
		yyperror("failed to create temporary file");

	shell_pipe(cmd, fd);

	ASSERT(shell_src_fd == -1);
	shell_src_fd = fd;
}

void
mdb_shell_source_run(void)
{
	int fd = shell_src_fd;

	if (fd == -1)
		return;

	shell_src_fd = -1;
	shell_source_eval(fd);
}

void
mdb_shell_source_discard(void)
{
	if (shell_src_fd != -1) {
		(void) close(shell_src_fd);
		shell_src_fd = -1;
	}
}

/*
 * Set up the shell stage of a dcmd pipeline ("dcmd ! 'command' | dcmd"):
 * debugger output is redirected into the spawned command just as for
 * mdb_shell_pipe, and the command's standard output is captured in an
 * unlinked temporary file whose descriptor is returned. The caller owns
 * the descriptor: once the producing dcmd has completed and the shellio
 * has been destroyed (waiting for the child), the capture is replayed
 * into the consuming side of the pipeline with mdb_shell_filter_pump.
 */
int
mdb_shell_filter(const char *cmd)
{
	int fd;

	if ((fd = shell_tmpfd()) == -1)
		yyperror("failed to create temporary file");

	shell_pipe(strdup(cmd), fd);
	return (fd);
}

/*
 * Replay a captured shell stage's output into the write side of a dcmd
 * pipeline, where it is parsed into the consuming dcmd's address list just
 * as direct dcmd output would be. Parse errors raised by the read side
 * propagate by longjmp to the caller's frame; the caller retains ownership
 * of fd throughout so that it can be closed on all paths.
 */
void
mdb_shell_filter_pump(int fd, mdb_iob_t *iob)
{
	char buf[BUFSIZ];
	ssize_t n;

	if (lseek(fd, 0, SEEK_SET) == -1)
		yyperror("failed to rewind temporary file");

	while ((n = read(fd, buf, sizeof (buf))) > 0) {
		if (mdb_iob_write(iob, buf, n) < 0)
			break;
	}
}

/*
 * Release the capture descriptor for a shell pipeline stage. This exists as
 * a function (rather than the caller using close() directly) because the
 * caller is code shared with kmdb, where descriptors do not exist.
 */
void
mdb_shell_filter_close(int fd)
{
	if (fd != -1)
		(void) close(fd);
}
