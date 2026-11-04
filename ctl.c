#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "config.h"

#if !defined(CMD) || !defined(SIG) || !defined(NAME)
#error
#endif

int main(void)
{
	struct sockaddr_un a = {.sun_family = AF_UNIX, .sun_path = SOCK_PATH};
	char c = CMD;
	int s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s >= 0 && sendto(s, &c, 1, 0, (struct sockaddr *)&a, sizeof a) == 1)
		return 0;
	if (!access("/run/systemd/system", F_OK))
		execl("/usr/bin/systemctl", "systemctl", NAME, (char *)0);
	if (geteuid() == 0 && !kill(1, SIG))
		return 0;
	return write(2, "ginit: unreachable\n", 19) < 0 ? 2 : 1;
}
