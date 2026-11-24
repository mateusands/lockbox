# How lockbox works

This document explains every piece: what it does, **why** it exists and what happens without it. The code in
`bin/lockbox` follows the same order and is commented line by line.

## 1. The base: bubblewrap and namespaces

Linux can give a process its own "view" of parts of the system: **namespaces**.
[bubblewrap](https://github.com/containers/bubblewrap) (`bwrap`) is the program that builds that view; Flatpak and
Steam (pressure-vessel) use it underneath. It needs neither root nor setuid: it uses unprivileged *user namespaces*.

| Namespace | What lockbox does | Why |
|---|---|---|
| network (`--unshare-net`) | the box only has `lo` | no internet **and** no local network (router, printer, NAS, other services on your PC at `localhost`). It also isolates abstract Unix sockets (the session's X11, for example, listens on one) |
| mount | only what was mounted exists | this is what hides your home and your disks |
| processes (`--unshare-pid`) | the app only sees its own processes | it cannot read the memory or command line of other programs, or signal them |
| IPC (`--unshare-ipc`) | its own shared memory | X11 MIT-SHM and other old channels do not leak |
| UTS, cgroup | its own hostname and cgroup | less information about the machine |
| user (`--unshare-user --disable-userns`) | the app **cannot** create new namespaces | see section 5 |

### The file system inside the box

```text
/usr, /etc, /opt, /sys   read-only (the app needs the system libraries and the font, GPU... configuration)
/etc/machine-id          replaced by an ID of the box's own (home/.lockbox/machine-id)
/proc                    new: only the box's processes
/dev                     minimal: null, zero, random, tty... + GPU + /dev/input + controller hidraw
/tmp, /var/tmp, /run     empty (tmpfs), gone when the app exits
$HOME                    the --home folder (the fake home), at the SAME path as your real home
--rw / --ro              only the folders you allowed, at the same path (or wherever --rw-at/--ro-at says)
```

The fake home sits **at the same path** as the real one (`/home/you`) on purpose: many programs store absolute paths
in their settings, and those keep working inside and outside the box.

### The environment

`--clearenv` wipes every variable of your session: no `SSH_AUTH_SOCK`, no GitHub tokens, no
`DBUS_SESSION_BUS_ADDRESS` and so on. Only `HOME`, `USER`, `PATH`, `LANG`, `XDG_RUNTIME_DIR`, the display and audio
variables lockbox sets, `MANGOHUD` and whatever you pass with `LOCKBOX_ENV_NAME=value` get in.

**No D-Bus:** the session bus gives access to the keyring (KWallet/Secret Service), notifications, the file manager
and much more, so it stays out. Apps that want D-Bus print a warning and carry on; a D-Bus filter (like Flatpak's
`xdg-dbus-proxy`) is in the ideas below.

## 2. Devices: GPU yes, keyboard no

- **GPU (`/dev/dri`)**: passes, otherwise there is no 3D acceleration.
- **`/dev/input`**: passes whole, but **keyboard and mouse** nodes are `root:input` and your user cannot open them;
  game controller (joystick) nodes you can open through udev ACLs. A controller plugged in later shows up by itself.
- **`hidraw`**: gives the **raw reports** of any USB/Bluetooth device, the **keyboard** included, when udev gives your
  user access (many mechanical keyboards and "gaming" mice do, for their RGB tools). lockbox only passes a `hidraw`
  node that is both from a **controller maker** (Sony, Nintendo, Valve, Microsoft, 8BitDo) **and** a joystick (has a
  `jsN`). Microsoft also makes keyboards, hence the second condition.
- **`/dev/uinput` NEVER passes.** With it, an app creates a virtual keyboard and **types into any window outside the
  box**. Steam's udev rules give your user write access to it, so many sandbox scripts leak it without noticing.

## 3. Display: Wayland with a security context

Wayland is already much better than X11: an app cannot read the keystrokes of another window. **But** compositors
offer extra, *privileged* protocols for clipboard managers, screen sharing, KDE Connect and so on. On KDE Plasma 6,
**any** client connecting to the normal Wayland socket gets, among others:

| Protocol | What it allows |
|---|---|
| `ext_data_control_manager_v1` | reading (and replacing) the **clipboard at any time, without focus**: the password you copied from your password manager |
| `org_kde_kwin_fake_input` | **faking keyboard and mouse input** |
| `zkde_screencast_unstable_v1` | **capturing the screen** |
| `org_kde_plasma_window_management` | seeing and controlling **every program's windows** |

This was tested from inside an "ordinary" bwrap box: the app read the clipboard while not focused and authenticated
to fake-input (details in [AUDIT.md](AUDIT.md)).

**The fix** is the [`wp_security_context_v1`](https://wayland.app/protocols/security-context-v1) protocol, the same
one Flatpak uses:

1. Before starting the app, the `lockbox-wayland` helper asks the compositor for a **new socket, tagged** as
   "sandboxed" (engine `lockbox`, app id `lockbox.<name>`).
2. Only that socket goes into the box, as `wayland-0`.
3. The compositor treats everything arriving through it as a sandboxed app and **does not offer** the privileged
   protocols. The window, the GPU (dmabuf), the keyboard **when focused**, the mouse, the clipboard **when focused** and
   gamepads keep working.
4. The helper holds one end of a pipe that the app itself inherits: when the app and everything it started exit, the
   pipe closes, the compositor ends the context and the socket is removed.

If the compositor lacks the protocol (KDE 6, GNOME 46+ and Sway have it), lockbox warns and uses the normal socket.

### X11

X11 has no isolation: any client reads the keystrokes and pixels of all others. That is why the session's X11 does
**not** go in (only with `--x11 host`, which should be avoided). For X11-only apps, `--x11 private`
(`lib/x11-private.sh`) starts an **Xwayland just for the box**, in a window of its own, with its own password (MIT
cookie). The app sees an empty X11 where only it exists.

## 4. seccomp: fewer doors into the kernel

The app still makes kernel calls, and kernel bugs are the classic way to "escape" a namespace. `lockbox-seccomp`
generates a filter (BPF) that bwrap loads (`--seccomp`). It **denies** calls that desktop apps and games do not use
and that show up in many exploits:

- `io_uring_*`: the biggest source of kernel bugs in recent years. Google turned it off on Android and ChromeOS.
- `bpf`, `perf_event_open`: large surfaces used in exploits.
- `keyctl`, `add_key`, `request_key`: the kernel keyring.
- Mounting, modules, kexec, the clock, swap, `syslog`, `open_by_handle_at`...
- `ioctl(TIOCSTI/TIOCLINUX)`: "typing" into the terminal that started the app.

The list is Flatpak's, plus `io_uring`. Everything else stays allowed; a denied call returns `EPERM`, which programs
treat as "not supported". `lockbox-seccomp --list` prints the list.

**`userfaultfd` is deliberately allowed.** Emulators and recompilations use it to learn which guest memory the game
wrote (a texture changed, code was patched). An early version denied it and TotK Recomp dropped to 8 FPS in its menu,
re-decoding every texture on every frame, without any error message. The kernel already disables the exploitable part
(handling faults raised inside the kernel) for unprivileged users when `vm.unprivileged_userfaultfd = 0`, which is
the default.

**If an app is slow or broken only inside the box,** run it once with `--no-seccomp`. If that fixes it, a call on the
list is needed: please open an issue with the app's name.

## 5. User namespaces

Creating a user namespace gives a process a "fake root" inside it, which unlocks kernel code an ordinary user could
never reach. That is why they show up in so many exploits. lockbox uses `--unshare-user --disable-userns`: the app
**cannot** create new namespaces.

**Exception:** Proton through `umu-launcher` runs a bwrap of its own (pressure-vessel) and needs them; so does the
Chromium/Electron internal sandbox. For those, use `--allow-userns`: lockbox allows namespaces and the mount calls
they need, and keeps blocking the rest of the seccomp list.

## 6. What is left outside, and where

- **`/run/user/UID/lockbox.PID.XXXX/`**: this run's Wayland socket and filter. Removed by the next lockbox run (and
  `/run/user` is wiped at logout).
- **`--home/.lockbox/machine-id`**: this box's machine ID (generated once).

## Ideas for later

- A **D-Bus** filter (`xdg-dbus-proxy`) to allow only notifications, for example.
- **Microphone without losing sound:** give the box a "restricted" PipeWire socket (through `module-access` +
  WirePlumber), like Flatpak does.
- **Landlock** as a second file lock (kernel 5.13+), independent of namespaces.
- Hiding `/sys/class/dmi` and disk serial numbers.
- Ready-made profiles (`lockbox profile steam-proton`...) and a graphical interface in the spirit of Sandboxie-Plus.
