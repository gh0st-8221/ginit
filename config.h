#define RC_SCRIPT "/etc/ginit/rc"
#define STOP_SCRIPT "/etc/ginit/stop"
#define SOCK_PATH "/run/ginit.sock"
#define RESPAWN_MS 2000

#ifdef GINIT_MAIN
static char *const svc[][8] = {
	{"/usr/bin/agetty", "--noissue", "--nohostname", "tty1", "linux", 0},
	{"/usr/bin/agetty", "--noissue", "--nohostname", "tty2", "linux", 0},
};
#endif
