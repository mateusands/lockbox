# Library (use with "source"): a PRIVATE Xwayland for an app that needs X11.
# The app only gets the socket of this X server, which holds none of your other windows: it cannot read keystrokes
# or touch other X11 programs (Steam, Discord...). The server opens in a window of its own and is stopped when the
# app exits. It has its own password (MIT cookie), so not even other programs on the PC can connect to it.
#
#   source x11-private.sh
#   x11p_start              -> sets X11P_DISPLAY (:N), X11P_SOCKET, X11P_AUTH (client cookie), X11P_PID
#   x11p_bwrap_args         -> prints the bwrap arguments (one per line) that mount only this X server
#   x11p_stop               -> stops the server and deletes the cookie
# Screen size: X11P_GEOMETRY (default 1920x1080). It opens as a normal WINDOW (with decorations) that you can
# maximize or make fullscreen from the compositor (Meta+F on KDE). X11P_FULLSCREEN=1 starts it fullscreen
# (with two monitors it may open across both screens: that is why it is not the default).

x11p_start() {
    command -v Xwayland >/dev/null || { echo "x11-private: Xwayland is not installed" >&2; return 1; }
    [[ -n ${WAYLAND_DISPLAY:-} ]] || { echo "x11-private: needs a Wayland session" >&2; return 1; }
    X11P_DIR=$(mktemp -d "${XDG_RUNTIME_DIR:-/tmp}/lockbox-x11.XXXXXX")
    chmod 700 "$X11P_DIR"
    local cookie; cookie=$(mcookie)
    _x11p_write_auth "$X11P_DIR/server" 0 "$cookie"
    local fd="$X11P_DIR/display" extra=(-decorate)
    [[ ${X11P_FULLSCREEN:-0} == 1 ]] && extra=(-fullscreen)
    Xwayland -displayfd 3 -auth "$X11P_DIR/server" -nolisten tcp -noreset \
        -geometry "${X11P_GEOMETRY:-1920x1080}" "${extra[@]}" 3>"$fd" >"$X11P_DIR/xwayland.log" 2>&1 &
    X11P_PID=$!
    local i
    for i in $(seq 100); do
        [[ -s $fd ]] && break
        kill -0 "$X11P_PID" 2>/dev/null || { echo "x11-private: Xwayland did not start (see $X11P_DIR/xwayland.log)" >&2; return 1; }
        sleep 0.1
    done
    local n; n=$(tr -d '[:space:]' < "$fd")
    [[ $n =~ ^[0-9]+$ ]] || { echo "x11-private: no display number" >&2; x11p_stop; return 1; }
    X11P_DISPLAY=":$n"
    X11P_SOCKET="/tmp/.X11-unix/X$n"
    X11P_AUTH="$X11P_DIR/client"
    _x11p_write_auth "$X11P_AUTH" "$n" "$cookie"
    for i in $(seq 50); do [[ -S $X11P_SOCKET ]] && break; sleep 0.1; done
    [[ -S $X11P_SOCKET ]] || { echo "x11-private: socket $X11P_SOCKET did not appear" >&2; x11p_stop; return 1; }
}

x11p_bwrap_args() {
    printf '%s\n' --dir /tmp/.X11-unix --bind "$X11P_SOCKET" "$X11P_SOCKET" \
        --ro-bind "$X11P_AUTH" "$X11P_AUTH" \
        --setenv DISPLAY "$X11P_DISPLAY" --setenv XAUTHORITY "$X11P_AUTH"
}

x11p_stop() {
    [[ -n ${X11P_PID:-} ]] && kill "$X11P_PID" 2>/dev/null && wait "$X11P_PID" 2>/dev/null
    [[ -n ${X11P_DIR:-} && -d $X11P_DIR ]] && rm -rf "$X11P_DIR"
    X11P_PID= X11P_DIR=
}

# Xauthority file: family FamilyWild (0xffff), empty address, display number, MIT-MAGIC-COOKIE-1
_x11p_write_auth() {
    python3 -I - "$1" "$2" "$3" <<'PY'
import struct, sys
path, num, cookie = sys.argv[1], sys.argv[2].encode(), bytes.fromhex(sys.argv[3])
def s(b): return struct.pack(">H", len(b)) + b
data = struct.pack(">H", 0xFFFF) + s(b"") + s(num) + s(b"MIT-MAGIC-COOKIE-1") + s(cookie)
with open(path, "wb") as f: f.write(data)
PY
    chmod 600 "$1"
}
