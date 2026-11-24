# Audit (November 2026)

Tests run **from inside** the previous box (an "ordinary" bwrap box: no network, fake home, Wayland only, no uinput,
controller-only hidraw) on a PC with CachyOS, kernel 7.3, KDE Plasma 6 (Wayland), Mesa/RADV 26.3, PipeWire and
bubblewrap 0.13.1. Each item says what was tested, the result and what the current version does. The tests are in
[`tests/`](../tests/).

## Leaks found and fixed

### 1. Clipboard, input emulation and screen capture through Wayland (severe)

**Test:** a ~40-line Wayland client (`tests/wayland-probe.c`) running inside the box:

```text
data-control: bound, fake-input: bound
fake-input: authenticated without error (no input sent)
READ THE CLIPBOARD WITHOUT FOCUS: 12 types, 77 bytes (content discarded)
```

KWin handed the sandboxed app `ext_data_control_manager_v1`, `org_kde_kwin_fake_input`,
`zkde_screencast_unstable_v1` and `org_kde_plasma_window_management`. In other words, a game downloaded from the
internet could read the password you just copied and register to fake keyboard and mouse input.

**Fix:** a Wayland socket with `wp_security_context_v1` (`src/lockbox-wayland.c`). The same test through the new
socket:

```text
data-control: not offered, fake-input: not offered
```

`xdg_wm_base`, `zwp_linux_dmabuf_v1`, `wp_linux_drm_syncobj_manager_v1`, `wl_seat`... are still offered. Tested with
Eden and RPCS3 (Qt 6), God of War III (SpartanPort), Pokémon Fulldex (SDL), Longshot and TotK Recomp (Vulkan), RE
Code: Veronica (Proton/umu, Wine Wayland) and Dante's Inferno (Proton on the private X11): all started normally.

### 2. No seccomp filter

**Test:** `/proc/self/status` → `Seccomp: 0`; `io_uring_setup` reached the kernel.
**Fix:** `src/lockbox-seccomp.c`. Now `Seccomp: 2`, and `io_uring_setup` returns `EPERM`.

### 3. User namespaces allowed

**Test:** `unshare -Ur true` worked inside the box.
**Fix:** `--unshare-user --disable-userns`; `--allow-userns` for Proton/umu and Chromium (tested: with the option,
RE Code: Veronica starts; without it, pressure-vessel fails with `ENOSPC`, as expected).

### 4. The real `/etc/machine-id`

The PC's unique and permanent ID was readable. **Fix:** every box gets a random one of its own.

### 5. A `--rw` folder executed outside the box (severe, configuration)

One launcher gave `--rw` to **Steam's Proton Experimental** folder, because Proton writes a `dist.lock` there. Steam
uses that folder **outside** the box, for other games. A malicious app could modify Proton and own the whole PC the
next time Steam started any game with it.

**Rule:** never give `--rw` to anything that runs outside the box: programs, scripts, the Steam folder,
`~/.local/bin`, `~/.config/autostart`, `~/.bashrc`... For Proton, use `umu-launcher` with a runtime inside the fake
home, or a copy of Proton that belongs to the box (see `examples/proton-umu.sh`).

### 6. Regression caught after the migration: `userfaultfd`

The first lockbox filter also denied `userfaultfd`. TotK Recomp then ran at **8 FPS in its menu** (124 ms per frame,
115 ms of it preparing the frame, the GPU busy for only 9 ms), with no error anywhere. Its log showed why: it decoded
**12.5 billion** ASTC texels in 25 seconds instead of about 0.24 billion, because without `userfaultfd` it cannot tell
which textures the game wrote and decodes all of them on every frame. Same game, 25 seconds each:

| Box | ASTC texels decoded |
|---|---|
| previous box (no seccomp) | 240 M |
| lockbox with the first filter | 12,523 M |
| lockbox `--no-seccomp` | 243 M |
| lockbox with `userfaultfd` allowed (current) | 246 M |

`userfaultfd` is now allowed (see [HOW-IT-WORKS.md](HOW-IT-WORKS.md#4-seccomp-fewer-doors-into-the-kernel)).
Lesson: a seccomp denial can show up as a silent slowdown, not as a crash.

### Fixed before this audit (earlier hardening)

- `/dev/uinput` went into the box: an app could create a virtual keyboard and type outside.
- **Every** `/dev/hidraw*` node went in, the keyboard's and the mouse's included (readable by the user through ACLs):
  an app could read keystrokes typed into any program.
- The session's X11 went in: an app could read the keystrokes and pixels of other X11/Xwayland windows.

## Still open

| Item | Risk | Why it is not closed |
|---|---|---|
| **Microphone** (PipeWire/Pulse) | the app records the microphone (tested: `pw-record` recorded) and changes other apps' volume | sound and microphone cannot be split without changing the system's PipeWire configuration. Pretending to be Flatpak (`/.flatpak-info`) did **not** restrict it. With no network, the recording does not leave the PC. `--no-audio` cuts everything |
| Kernel and GPU driver | a bug in them still allows an escape | seccomp and the user namespace block shrink the surface a lot; full isolation needs a virtual machine |
| Fingerprinting | motherboard model and disk serials in `/sys` | hiding all of `/sys` breaks the GPU and controllers; with no network, the information does not leave |
| Writing and running files in `--rw` folders | the app can create and run programs in its own folders | expected; the danger is only when something **outside** the box runs those folders (item 5) |
| The GPU's primary `card` node | the app opens `/dev/dri/card*` | only the compositor (DRM master) can change the display; apps need the node for Vulkan/WSI |
| No resource limits | an app can use all the memory | use `--memory 8G` |
| `--net` | with network, everything the app can read can leave | that is the option; use it only when needed |

## Repeating the tests

```bash
make
./bin/lockbox --home /tmp/lockbox-test --test                       # the self-test
./bin/lockbox --home /tmp/lockbox-test --wayland direct --test      # shows the Wayland leaks without the context
bash tests/leaks.sh                                                 # runs the probes with and without each protection
```
