# lockbox

**A lightweight sandbox for running Linux apps and games without giving them your network, your files, your keyboard
or your screen.**
Built and tested on **CachyOS with KDE Plasma (Wayland)**; works on any Arch-based distro and, with little effort, on
others.

```bash
lockbox --home ~/Boxes/my-app/home --ro ~/Apps/MyApp -- ~/Apps/MyApp/myapp
```

The app starts normally, with its window, GPU, sound and game controller. But inside the box:

| The app… | Without lockbox | With lockbox |
|---|---|---|
| reaches the internet (or your local network) | yes | **no** (`--net` allows it) |
| reads `~/Documents`, `~/.ssh`, browser passwords, other disks | yes | **no**: it only sees a fake home and the folders you allow |
| reads what you type in other programs (hidraw, X11) | can | **no** |
| types or clicks outside itself (uinput, KDE fake-input) | can | **no** |
| reads the clipboard at any time (a copied password!) | **can, on Wayland too** | **no** |
| captures your screen or controls your windows | can | **no** |
| uses rare kernel calls (io_uring, bpf, keyctl…) | yes | **no** (seccomp) |
| creates user namespaces (the entry point of many kernel exploits) | yes | **no** (`--allow-userns` for Proton) |
| knows your PC's unique ID (`/etc/machine-id`) | yes | **no**: every box has its own |

All of this is checked for real, from inside the box, with `--test` (see [Testing](#testing)).

## Why

Game ports, recompilations, emulators and AppImages downloaded from GitHub run with **all of your access**: the same
user that opens your bank in the browser. lockbox started as a way to isolate those games and grew into a general
tool for any program you do not trust 100% (or that simply needs nothing beyond its own folder).

On Windows, the idea is **Sandboxie-Plus**. On Linux there are already Flatpak, Firejail and Bubblejail; the
comparison, and why one more, is in [docs/COMPARISON.md](docs/COMPARISON.md). In short: lockbox is **a readable
script** (no setuid, no daemon, no Python runtime) that runs **any binary, AppImage or Proton game** as it already is.
It is also the only bwrap-based script I know of that uses the **Wayland security context**. Without it, KDE hands an
"isolated" app your clipboard and keyboard/mouse emulation.

## Install

Dependencies (CachyOS/Arch ship almost all of them): `bubblewrap`, `libseccomp`, `wayland`, `xorg-xwayland` (for
`--x11 private`), `gcc` or `clang`, `make`.

```bash
sudo pacman -S --needed bubblewrap libseccomp wayland xorg-xwayland base-devel
git clone https://github.com/mateusands/lockbox.git
cd lockbox
make
make install          # installs ~/.local/bin/lockbox and ~/.local/lib/lockbox (no root)
```

You can also use it without installing: `./bin/lockbox` finds its helpers in `build/`.

## Usage

```text
lockbox --home DIR [options] -- command [args...]
lockbox --home DIR [options] --test
```

| Option | What it does |
|---|---|
| `--home DIR` | **required**. Becomes the app's home, mounted over yours at the same path, so settings with absolute paths keep working. Everything the app writes "to the home" ends up here. |
| `--rw PATH` | a folder the app can read and **write** (saves, settings), at the same path |
| `--ro PATH` | a folder the app can only **read** (the program itself, ROMs, game files) |
| `--rw-at SOURCE DEST`, `--ro-at SOURCE DEST` | the same, mounted at another path |
| `--wayland direct` | Wayland **without** a security context (only if an app will not start with the default) |
| `--x11 private` | for X11-only apps (Qt 5 without `qt5-wayland`, older Wine/Proton): a **private Xwayland** in a window of its own that sees none of your other windows |
| `--x11 host` | your session's X11. **Avoid**: on X11 any client can read every other client's keystrokes |
| `--net` | allows network access. Without it, not even the local network exists inside the box |
| `--no-audio` | no PipeWire/Pulse (see [what it does not cover](#what-lockbox-does-not-cover)) |
| `--allow-userns` | for **Proton through umu/pressure-vessel** and the **Chromium/Electron** internal sandbox |
| `--memory 8G` | caps memory (through `systemd-run --user`) |
| `--udmabuf` | passes `/dev/udmabuf` (some emulators/recompilations import GPU memory faster on RADV) |
| `--name NAME` | app name given to the compositor (default: derived from the `--home` folder) |
| `--test` | runs nothing: shows what the app would see and probes every known leak |
| `LOCKBOX_ENV_X=value` | becomes `X=value` inside the box (your session's environment does **not** get in: no tokens) |

### Examples

Ready to copy, with comments, in [examples/](examples/):

- [simple-app.sh](examples/simple-app.sh): any binary, with only its own folder.
- [appimage.sh](examples/appimage.sh): an AppImage. FUSE does not work inside the box, so it is extracted once, also inside a box.
- [proton-umu.sh](examples/proton-umu.sh): a Windows game through umu-launcher + Proton (Wine Wayland).
- [app-x11.sh](examples/app-x11.sh): an X11-only app (e.g. mGBA on Qt 5) with the private Xwayland.
- [app-with-network.sh](examples/app-with-network.sh): an app that needs the network but not your files.
- [menu-entry.desktop](examples/menu-entry.desktop): a KDE menu entry with a "Test isolation" action.

## Testing

```bash
lockbox --home ~/Boxes/test --test
```

```text
  ok    personal folders hidden
  ok    no network
  ok    no hidraw
  ok    no uinput
  ok    no session X11
  ok    no privileged Wayland protocols
  ok    seccomp filter active
  ok    user namespaces blocked
  ok    machine-id of this box (348ed3bc…)
  note  PipeWire/Pulse: sound and MICROPHONE (use --no-audio if not needed)
result: OK
```

Run `--test` with **the same options** as your launcher: it tests the exact box the app will get. A `FAIL` line says
exactly what is exposed. With `--wayland direct`, for example, it shows that KDE hands out the clipboard, input
emulation, screen capture and window control.

`bash tests/leaks.sh` runs the actual exploits (reading the clipboard, kernel calls, user namespaces) with and without
each protection.

## How it works

In one sentence: [bubblewrap](https://github.com/containers/bubblewrap) builds a private view of the system (kernel
namespaces) in which only what lockbox mounted exists, and lockbox adds three layers that bwrap alone does not give
you. Each piece, and why it exists, is explained in **[docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md)**:

1. **Namespaces (bwrap):** own network, processes, IPC and hostname; `/usr` and `/etc` read-only; a fake home; empty `/tmp` and `/run`; a clean environment.
2. **Filtered devices:** the GPU yes; raw keyboard and mouse no; `hidraw` only for game controllers; `uinput` never.
3. **Wayland with a security context** (`lockbox-wayland`, ~80 lines of C): the same protocol Flatpak uses (`wp_security_context_v1`).
4. **seccomp** (`lockbox-seccomp`): denies 37 kernel calls (and 2 terminal ioctls) that normal apps do not use.
5. **User namespaces blocked** (bwrap's `--disable-userns`).

## What lockbox does NOT cover

Honesty first: this is **not** a virtual machine. The full list, with the reasons, is in
[docs/AUDIT.md](docs/AUDIT.md#still-open):

- **Microphone:** whoever has PipeWire can record the microphone. With no network there is nowhere to send it, but use `--no-audio` for apps that need no sound.
- **Kernel and GPU driver bugs:** lockbox shrinks the attack surface a lot, but the app still talks to the kernel and the GPU.
- **`--rw` folders that something OUTSIDE the box executes** are a way out: never give `--rw` to a program folder you also run outside, such as Steam's Proton.
- **Hardware fingerprint:** the motherboard model and disk serials still show in `/sys`. With no network, that stays on the machine.
- `--net`, `--x11 host`, `--wayland direct` and `--allow-userns` each open one specific door. Use them only when needed.

## Layout

```text
bin/lockbox              the main script (bash, commented)
lib/x11-private.sh       the private Xwayland behind --x11 private
src/lockbox-wayland.c    creates the Wayland socket with a security context
src/lockbox-seccomp.c    generates the seccomp filter
src/lockbox-probe.c      used by --test: lists the dangerous Wayland protocols the app can reach
protocols/               XML of the Wayland protocols used
examples/                launchers ready to adapt
docs/                    how it works, the audit and the comparison with other projects
tests/                   leak tests (run from inside the box)
```

## License

[MIT](LICENSE).
