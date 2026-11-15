#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define ROOT_SOCK "/data/local/tmp/pc98260-root.sock"

static int read_exact(int fd, void *buffer, size_t length)
{
	char *p = buffer;
	while (length) {
		ssize_t got = read(fd, p, length);
		if (got < 0 && errno == EINTR) continue;
		if (got <= 0) return -1;
		p += got;
		length -= (size_t)got;
	}
	return 0;
}

static int selinux_is_permissive(void)
{
	char state = 0;
	int fd = open("/sys/fs/selinux/enforce", O_RDONLY | O_CLOEXEC);
	if (fd < 0) return -1;
	ssize_t n = read(fd, &state, 1);
	close(fd);
	return n == 1 && state == '0' ? 0 : -1;
}

static int maintain_permissive(void)
{
	if (selinux_is_permissive() == 0) return 0;
	int fd = open("/sys/fs/selinux/enforce", O_WRONLY | O_CLOEXEC);
	if (fd < 0) return -1;
	ssize_t n = write(fd, "0", 1);
	int saved_errno = errno;
	close(fd);
	if (n != 1) {
		errno = saved_errno;
		return -1;
	}
	return selinux_is_permissive();
}

static int set_su_context(void)
{
	static const char ctx[] = "u:r:init:s0";
	int fd = open("/proc/self/attr/current", O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		dprintf(STDERR_FILENO, "[rootd] open setcon failed errno=%d\n", errno);
		return -1;
	}
	ssize_t n = write(fd, ctx, sizeof(ctx) - 1);
	int saved_errno = errno;
	close(fd);
	if (n != (ssize_t)(sizeof(ctx) - 1)) {
		dprintf(STDERR_FILENO, "[rootd] setcon init failed errno=%d\n", saved_errno);
		return -1;
	}
	return 0;
}

static void policy_watch(void)
{
	pid_t owner = getppid();
	int failed = 0;
	for (;;) {
		if (getppid() != owner) _exit(0);
		if (maintain_permissive() < 0) {
			if (!failed)
				dprintf(STDERR_FILENO, "[rootd] permissive watch failed errno=%d\n", errno);
			failed = 1;
		} else {
			failed = 0;
		}
		usleep(200000);
	}
}

static void exec_direct(char *command)
{
	/* Experimental protocol: a command beginning with '!' is parsed as
	   whitespace-separated argv and exec'd directly, without /system/bin/sh.
	   The rootd process is full-cap, while sh deliberately drops to shell
	   uid 2000 on this ROM, so m6 cleanup must use this path. */
	char *argv[32];
	char *save = NULL;
	int argc = 0;
	char *token = strtok_r(command, " \t", &save);
	while (token && argc < 31) {
		argv[argc++] = token;
		token = strtok_r(NULL, " \t", &save);
	}
	argv[argc] = NULL;
	if (!argc) _exit(127);
	char *const env[] = { (char *)"PATH=/system/bin:/vendor/bin:/data/local/tmp",
		(char *)"HOME=/data/local/tmp", NULL };
	execve(argv[0], argv, env);
	_exit(127);
}

static void handle_client(int fd)
{
	uint32_t length = 0;
	char command[4097];
	if (read_exact(fd, &length, sizeof(length)) || length > 4096U ||
	    read_exact(fd, command, length)) return;
	command[length] = 0;
	pid_t child = fork();
	if (child < 0) return;
	if (child == 0) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (maintain_permissive() < 0 || set_su_context() < 0) {
			dprintf(STDERR_FILENO, "ROOT98260_CHILD_POLICY_FAILED\n");
			_exit(125);
		}
		if (fd > STDERR_FILENO) close(fd);
		char *const env[] = { (char *)"PATH=/system/bin:/vendor/bin:/data/local/tmp",
			(char *)"HOME=/data/local/tmp", NULL };
		if (length && command[0] == '!') {
			exec_direct(command + 1);
		}
		if (length) {
			char *const argv[] = { (char *)"sh", (char *)"-c",
				command, NULL };
			execve("/system/bin/sh", argv, env);
		} else {
			char *const argv[] = { (char *)"sh", (char *)"-i", NULL };
			execve("/system/bin/sh", argv, env);
		}
		_exit(127);
	}
	int status;
	while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
}

int pc98260_run_rootd(void)
{
	if (geteuid() != 0) return 1;
	const char *path = ROOT_SOCK;
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGPIPE, SIG_IGN);
	int listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (listener < 0) return 1;
	struct sockaddr_un address = { .sun_family = AF_UNIX };
	if (strlen(path) >= sizeof(address.sun_path)) {
		close(listener);
		return 1;
	}
	strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
	(void)unlink(path);
	mode_t old_umask = umask(007);
	int bound = bind(listener, (struct sockaddr *)&address, sizeof(address));
	(void)umask(old_umask);
	if (bound || chown(path, 0, 2000) || chmod(path, 0660) ||
	    listen(listener, 8)) {
		dprintf(STDERR_FILENO,
			"[rootd] startup failed errno=%d (%s)\n",
			errno, strerror(errno));
		close(listener);
		return 1;
	}
	dprintf(STDERR_FILENO, "[rootd] listening at %s pid=%d\n",
		path, getpid());
	pid_t watcher = fork();
	if (watcher < 0) {
		dprintf(STDERR_FILENO, "[rootd] policy watch fork failed errno=%d\n", errno);
		close(listener);
		(void)unlink(path);
		return 1;
	}
	if (watcher == 0) {
		policy_watch();
		_exit(0);
	}
	dprintf(STDERR_FILENO, "[rootd] permissive watch pid=%d context=u:r:init:s0\n", watcher);
	for (;;) {
		int client = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
		if (client < 0) {
			if (errno == EINTR) continue;
			break;
		}
		struct ucred peer;
		socklen_t peer_len = sizeof(peer);
		if (!getsockopt(client, SOL_SOCKET, SO_PEERCRED,
				&peer, &peer_len) &&
		    (peer.uid == 0 || peer.uid == 2000))
			handle_client(client);
		close(client);
	}
	close(listener);
	return 1;
}
