#!/bin/sh
set -e
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 1; }
command -v mkinitcpio >/dev/null || { echo "mkinitcpio not found"; exit 1; }
etc=${ETC:-/etc}
esp=${ESP:-/boot}
kimg=${KERNEL:-}
if [ -z "$kimg" ]; then
	v=$(ls -1 /usr/lib/modules 2>/dev/null | sort -V | tail -n1)
	kimg=/usr/lib/modules/$v/vmlinuz
	[ -f "$kimg" ] || kimg=/boot/vmlinuz-linux
fi
ginit=${GINIT:-/usr/local/bin/ginit}
out=$esp/EFI/Linux/arch-ginit.efi
if [ -z "$ESP" ] && ! mountpoint -q "$esp"; then echo "$esp is not mounted"; exit 1; fi
[ -f "$kimg" ] || { echo "$kimg not found"; exit 1; }
[ -x "$ginit" ] || { echo "$ginit not found, run make install first"; exit 1; }

mkdir -p "$etc/kernel" "$esp/EFI/Linux"
cat > "$etc/mkinitcpio-ginit.conf" <<'EOF'
MODULES=(nvme)
BINARIES=()
FILES=()
HOOKS=(base udev autodetect modconf block filesystems)
COMPRESSION="zstd"
EOF

if [ -f "$etc/kernel/cmdline" ]; then
	base=$(tr '\n' ' ' < "$etc/kernel/cmdline")
else
	base=$(sed -E -e 's/(^| )BOOT_IMAGE=[^ ]*//g' -e 's/(^| )initrd=[^ ]*//g' /proc/cmdline)
fi
base=$(printf '%s' "$base" | sed -E -e 's/(^| )init=[^ ]*//g' -e 's/  +/ /g' -e 's/^ //' -e 's/ $//')
case " $base " in *" quiet "*) ;; *) base="$base quiet" ;; esac
case " $base " in *" loglevel="*) ;; *) base="$base loglevel=1" ;; esac
printf '%s init=%s\n' "$base" "$ginit" > "$etc/kernel/cmdline-ginit"

sed 's/^PRETTY_NAME=.*/PRETTY_NAME="Arch Linux (ginit)"/' "${OSREL:-$etc/os-release}" > "$etc/os-release-ginit"

mkinitcpio -c "$etc/mkinitcpio-ginit.conf" -k "$kimg" \
	--cmdline "$etc/kernel/cmdline-ginit" \
	--osrelease "$etc/os-release-ginit" \
	-U "$out"

echo "cmdline: $(cat "$etc/kernel/cmdline-ginit")"
echo "written: $out"
ls -l "$esp/EFI/Linux"
