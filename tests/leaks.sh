#!/usr/bin/env bash
# leaks.sh: runs the probes inside the box, with and without each protection, and shows the difference.
# Sends no key or click; from the clipboard it only counts bytes.
#   bash tests/leaks.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
lockbox=$root/bin/lockbox
b=$root/build/tests
mkdir -p "$b"
make -s -C "$root"
# the Wayland probe (protocols in protocols/)
for p in ext-data-control-v1:dc kde-fake-input:fi; do
    xml=$root/protocols/${p%%:*}.xml; n=${p##*:}
    wayland-scanner client-header "$xml" "$b/$n.h"; wayland-scanner private-code "$xml" "$b/$n.c"
done
cc -O1 -I"$b" -o "$b/wayland-probe" "$root/tests/wayland-probe.c" "$b/dc.c" "$b/fi.c" $(pkg-config --cflags --libs wayland-client)

home=$(mktemp -d)
trap 'rm -rf "$home"' EXIT
kernel='grep "^Seccomp:" /proc/self/status
        python3 -c "import ctypes,os;l=ctypes.CDLL(None,use_errno=True);r=l.syscall(425,8,ctypes.c_void_p(0));e=ctypes.get_errno();print(\"io_uring_setup:\",\"BLOCKED (EPERM)\" if e==1 else \"reaches the kernel (allowed)\")"
        unshare -U true 2>/dev/null && echo "user namespace: CAN create" || echo "user namespace: blocked"
        echo "machine-id: $(head -c 8 /etc/machine-id)… (real: '"$(head -c 8 /etc/machine-id)"'…)"'

title() { echo; echo "######## $*"; }
title "lockbox WITHOUT the extra protections (--wayland direct --allow-userns --no-seccomp)"
"$lockbox" --home "$home" --wayland direct --allow-userns --no-seccomp --ro-at "$b/wayland-probe" /run/probe -- \
    bash -c "/run/probe; $kernel" 2>&1 || true
title "lockbox default"
"$lockbox" --home "$home" --ro-at "$b/wayland-probe" /run/probe -- bash -c "/run/probe; $kernel" 2>&1 || true
title "self-test (--test)"
"$lockbox" --home "$home" --test 2>&1 | tail -12
