#define _GNU_SOURCE
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define GINIT_MAIN
#include "config.h"

#define NSVC (sizeof svc / sizeof svc[0])
#define MAXM 1024

static pid_t spid[NSVC];
static long nxt[NSVC];

static long now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void unesc(char *s)
{
	char *d = s;
	while (*s) {
		if (s[0] == '\\' && s[1] >= '0' && s[1] <= '3' &&
		    s[2] >= '0' && s[2] <= '7' && s[3] >= '0' && s[3] <= '7') {
			*d++ = ((s[1] - '0') << 6) | ((s[2] - '0') << 3) | (s[3] - '0');
			s += 4;
		} else {
			*d++ = *s++;
		}
	}
	*d = 0;
}

static pid_t run(char *const *a)
{
	pid_t p = fork();
	if (p == 0) {
		sigset_t s;
		sigemptyset(&s);
		sigprocmask(SIG_SETMASK, &s, 0);
		setsid();
		execv(a[0], a);
		_exit(127);
	}
	return p;
}

static void waitpid_to(pid_t p, int ms)
{
	struct timespec ts = {0, 10000000};
	int st;
	if (p <= 0)
		return;
	while (ms > 0) {
		if (waitpid(p, &st, WNOHANG) != 0)
			return;
		nanosleep(&ts, 0);
		ms -= 10;
	}
	kill(p, SIGKILL);
	waitpid(p, &st, 0);
}

static int reap_until(int ms)
{
	struct timespec ts = {0, 10000000};
	pid_t p;
	while (ms > 0) {
		do
			p = waitpid(-1, 0, WNOHANG);
		while (p > 0);
		if (p < 0)
			return 1;
		nanosleep(&ts, 0);
		ms -= 10;
	}
	return 0;
}

static int keep(const char *p)
{
	static const char *const k[] = {"/proc", "/sys", "/dev"};
	size_t i, n;
	if (!strcmp(p, "/"))
		return 1;
	for (i = 0; i < 3; i++) {
		n = strlen(k[i]);
		if (!strncmp(p, k[i], n) && (p[n] == 0 || p[n] == '/'))
			return 1;
	}
	return 0;
}

static void umount_all(void)
{
	static char buf[131072];
	char *mp[MAXM];
	int nm = 0, i;
	size_t len = 0;
	ssize_t n;
	int fd = open("/proc/self/mounts", O_RDONLY);
	if (fd < 0)
		return;
	while (len < sizeof buf - 1 && (n = read(fd, buf + len, sizeof buf - 1 - len)) > 0)
		len += n;
	close(fd);
	buf[len] = 0;
	for (char *l = buf; *l && nm < MAXM;) {
		char *nl = strchr(l, '\n');
		char *s, *e;
		if (nl)
			*nl = 0;
		s = strchr(l, ' ');
		if (s) {
			s++;
			e = strchr(s, ' ');
			if (e) {
				*e = 0;
				unesc(s);
				mp[nm++] = s;
			}
		}
		if (!nl)
			break;
		l = nl + 1;
	}
	for (i = nm - 1; i >= 0; i--)
		if (!keep(mp[i]))
			umount2(mp[i], 0);
}

static void down(int cmd) __attribute__((noreturn));
static void down(int cmd)
{
	if (!access(STOP_SCRIPT, X_OK)) {
		char *a[] = {STOP_SCRIPT, 0};
		waitpid_to(run(a), 5000);
	}
	kill(-1, SIGTERM);
	if (!reap_until(3000)) {
		kill(-1, SIGKILL);
		reap_until(1000);
	}
	sync();
	umount_all();
	mount(0, "/", 0, MS_REMOUNT | MS_RDONLY, 0);
	sync();
	reboot(cmd);
	for (;;)
		pause();
}

static void mnt(const char *s, const char *t, const char *f, unsigned long fl, const char *d)
{
	mkdir(t, 0755);
	mount(s, t, f, fl, d);
}

static void setup(void)
{
	int fd;
	umask(022);
	chdir("/");
	mount(0, "/", 0, MS_REMOUNT, 0);
	mnt("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, 0);
	mnt("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, 0);
	mnt("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, "mode=0755");
	mnt("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "gid=5,mode=0620,ptmxmode=0666");
	mnt("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
	mnt("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755");
	mnt("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
	symlink("/proc/self/fd", "/dev/fd");
	symlink("/proc/self/fd/0", "/dev/stdin");
	symlink("/proc/self/fd/1", "/dev/stdout");
	symlink("/proc/self/fd/2", "/dev/stderr");
	fd = open("/proc/sys/kernel/printk", O_WRONLY);
	if (fd >= 0) {
		if (write(fd, "1 1 1 1", 7) < 0)
			;
		close(fd);
	}
	fd = open("/dev/console", O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2)
			close(fd);
	}
	fd = open("/etc/hostname", O_RDONLY);
	if (fd >= 0) {
		char b[64];
		ssize_t n = read(fd, b, sizeof b), i = 0;
		close(fd);
		while (i < n && b[i] != '\n')
			i++;
		if (i > 0)
			sethostname(b, i);
	}
	setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/bin", 1);
	setenv("TERM", "linux", 1);
}

static int mksock(void)
{
	struct sockaddr_un a = {.sun_family = AF_UNIX, .sun_path = SOCK_PATH};
	int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (s < 0)
		return -1;
	unlink(SOCK_PATH);
	if (bind(s, (struct sockaddr *)&a, sizeof a) < 0) {
		close(s);
		return -1;
	}
	chmod(SOCK_PATH, 0666);
	return s;
}

static void reap(void)
{
	int st;
	size_t i;
	pid_t p;
	while ((p = waitpid(-1, &st, WNOHANG)) > 0)
		for (i = 0; i < NSVC; i++)
			if (spid[i] == p)
				spid[i] = 0;
}

int main(void)
{
	sigset_t m;
	struct pollfd f[2];
	int sfd;
	if (getpid() != 1)
		return 1;
	setup();
	sigemptyset(&m);
	sigaddset(&m, SIGCHLD);
	sigaddset(&m, SIGUSR1);
	sigaddset(&m, SIGUSR2);
	sigaddset(&m, SIGTERM);
	sigaddset(&m, SIGINT);
	sigprocmask(SIG_BLOCK, &m, 0);
	reboot(RB_DISABLE_CAD);
	sfd = signalfd(-1, &m, SFD_CLOEXEC | SFD_NONBLOCK);
	f[0] = (struct pollfd){sfd, POLLIN, 0};
	f[1] = (struct pollfd){mksock(), POLLIN, 0};
	if (!access(RC_SCRIPT, X_OK)) {
		char *a[] = {RC_SCRIPT, 0};
		run(a);
	}
	for (;;) {
		long t = now();
		int to = -1;
		size_t i;
		for (i = 0; i < NSVC; i++) {
			if (spid[i])
				continue;
			if (nxt[i] <= t) {
				pid_t p = run(svc[i]);
				spid[i] = p > 0 ? p : 0;
				nxt[i] = t + RESPAWN_MS;
			}
			if (!spid[i]) {
				long d = nxt[i] - t;
				if (d < 1)
					d = 1;
				if (to < 0 || d < to)
					to = d;
			}
		}
		poll(f, 2, to);
		if (f[0].revents & POLLIN) {
			struct signalfd_siginfo si;
			while (read(sfd, &si, sizeof si) == sizeof si) {
				switch (si.ssi_signo) {
				case SIGCHLD:
					break;
				case SIGUSR1:
					down(RB_HALT_SYSTEM);
				case SIGUSR2:
					down(RB_POWER_OFF);
				default:
					down(RB_AUTOBOOT);
				}
			}
			reap();
		}
		if (f[1].revents & POLLIN) {
			char c;
			while (recv(f[1].fd, &c, 1, 0) == 1) {
				if (c == 'p')
					down(RB_POWER_OFF);
				if (c == 'r')
					down(RB_AUTOBOOT);
				if (c == 'h')
					down(RB_HALT_SYSTEM);
			}
		}
	}
}
