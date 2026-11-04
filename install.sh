#!/bin/sh
set -e
[ "$(id -u)" = 0 ] || exec sudo -E "$0" "$@"
cd "$(dirname "$0")"
make clean >/dev/null
make
make install
make clean >/dev/null
if [ -z "$DESTDIR" ] && [ -n "$SUDO_USER" ]; then
	h=$(getent passwd "$SUDO_USER" | cut -d: -f6)
	if [ -d "$h" ]; then
		touch "$h/.hushlogin"
		chown "$SUDO_USER:" "$h/.hushlogin"
	fi
fi
if [ -n "$NOUKI" ] || [ -n "$DESTDIR" ]; then
	echo "skipping UKI build"
elif [ ! -d /sys/firmware/efi ]; then
	echo "not booted in UEFI mode, skipping UKI build"
elif ! command -v mkinitcpio >/dev/null; then
	echo "mkinitcpio not found, skipping UKI build"
else
	./mkuki-ginit.sh
fi
echo "ginit installed"
