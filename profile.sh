if [ ! -d /run/systemd/system ] && [ "$(id -u)" != 0 ]; then
	if [ -z "$XDG_RUNTIME_DIR" ] && [ -d "/run/user/$(id -u)" ]; then
		XDG_RUNTIME_DIR=/run/user/$(id -u)
		export XDG_RUNTIME_DIR
	fi
	if [ -z "$XDG_VTNR" ]; then
		_t=$(tty 2>/dev/null)
		case "$_t" in
		/dev/tty[0-9]*)
			XDG_VTNR=${_t#/dev/tty}
			export XDG_VTNR
			;;
		esac
		unset _t
	fi
	if [ -n "$XDG_RUNTIME_DIR" ] && [ -d "$XDG_RUNTIME_DIR" ]; then
		if ! pgrep -u "$(id -u)" -x dbus-daemon >/dev/null 2>&1; then
			rm -f "$XDG_RUNTIME_DIR/bus"
			setsid -f dbus-daemon --session --address="unix:path=$XDG_RUNTIME_DIR/bus" --nofork --nopidfile >/dev/null 2>&1
		fi
		DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
		export DBUS_SESSION_BUS_ADDRESS
	fi
	if [ -n "$XDG_RUNTIME_DIR" ] && ! pgrep -u "$(id -u)" -x pipewire >/dev/null 2>&1; then
		setsid -f pipewire >/dev/null 2>&1
		setsid -f wireplumber >/dev/null 2>&1
		setsid -f pipewire-pulse >/dev/null 2>&1
	fi
fi
