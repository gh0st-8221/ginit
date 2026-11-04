CC ?= cc
CFLAGS ?= -Os -static -s -Wall -Wextra
PREFIX ?= /usr/local

all: ginit poweroff reboot

ginit: ginit.c config.h
	$(CC) $(CFLAGS) -o $@ ginit.c

poweroff: ctl.c config.h
	$(CC) $(CFLAGS) -DCMD="'p'" -DSIG=SIGUSR2 -DNAME='"poweroff"' -o $@ ctl.c

reboot: ctl.c config.h
	$(CC) $(CFLAGS) -DCMD="'r'" -DSIG=SIGTERM -DNAME='"reboot"' -o $@ ctl.c

install: all
	install -Dm755 ginit $(DESTDIR)$(PREFIX)/bin/ginit
	install -Dm755 poweroff $(DESTDIR)$(PREFIX)/bin/poweroff
	install -Dm755 reboot $(DESTDIR)$(PREFIX)/bin/reboot
	install -Dm755 rc $(DESTDIR)/etc/ginit/rc
	install -Dm755 stop $(DESTDIR)/etc/ginit/stop
	install -Dm755 ginit-diag $(DESTDIR)$(PREFIX)/bin/ginit-diag
	install -Dm644 profile.sh $(DESTDIR)/etc/profile.d/ginit.sh
	install -Dm755 mkuki-ginit.sh $(DESTDIR)$(PREFIX)/bin/mkuki-ginit
	install -Dm644 ginit-uki.hook $(DESTDIR)/etc/pacman.d/hooks/95-ginit-uki.hook
	install -Dm755 shim-systemctl $(DESTDIR)$(PREFIX)/bin/systemctl
	install -Dm755 shim-startx $(DESTDIR)$(PREFIX)/bin/startx

clean:
	rm -f ginit poweroff reboot

.PHONY: all install clean
