# Comparison with other projects

| | **lockbox** | Flatpak | Firejail | Bubblejail | Sandboxie-Plus (Windows) | landrun |
|---|---|---|---|---|---|---|
| Base | bubblewrap | bubblewrap | its own, **setuid root** | bubblewrap | Windows kernel driver | Landlock |
| Runs the program you already have (binary, AppImage, Proton) | **yes** | no: it must be packaged as a Flatpak | yes | yes | yes | yes (command line only) |
| No network by default | **yes** | no (per app) | no (per profile) | per profile | optional | optional |
| Wayland security context | **yes** | yes | no | not found in its code | — | no |
| seccomp | yes | yes | yes | yes | — | no |
| Blocks user namespaces | yes (option to allow) | yes | optional | yes | — | — |
| Filters hidraw/uinput (keyloggers, virtual keyboards) | **yes** | partly (`--device=all` lets everything in) | per profile | per profile | — | no |
| Isolated X11 for X11-only apps | **yes (private Xwayland)** | no (the session's X11) | optional (Xpra/Xephyr) | no | — | no |
| D-Bus filter | no (no D-Bus at all) | yes (xdg-dbus-proxy) | yes | yes | — | — |
| Microphone separate from sound | no | yes (portal) | no | no | — | — |
| Graphical interface | no (KDE menu + `--test`) | yes (app stores) | Firetools | yes | **yes** | no |
| Size | ~300 lines of bash + 3 small C helpers | large | large (C, setuid) | Python | large | Go |

## Why not just Flatpak

Flatpak is the Linux standard and the best choice for apps packaged for it. But game ports, recompilations, freshly
released emulators and AppImages from GitHub are not on Flathub, and when they are, they ask for
`--filesystem=home`, `--device=all` (which includes the keyboard's `hidraw` and `uinput`) or `--share=network`.
lockbox runs the program the way it came, and the default is closed.

## Why not Firejail

Firejail is a large **setuid root** binary. A bug in it becomes root: one unofficial comparison counts 18 CVEs, 12 of
them escalations to root. bubblewrap is small, audited and, on Arch/CachyOS, not even setuid: it uses unprivileged
user namespaces. Firejail's profiles are also permissive by default (made not to break apps), while lockbox starts
closed.

## Bubblejail

The closest one: bwrap, seccomp, a D-Bus filter and a graphical interface. In its code I did not find the Wayland
security context, a hidraw filter by vendor and joystick, or a private X11. It is written in Python and aimed at
desktop apps installed on the system; lockbox targets loose binaries, AppImages and games through Proton, and is a
script you can read in one sitting.

## Sandboxie-Plus

The inspiration for "one box per program". On Windows it works by **redirection**: the app thinks it wrote to
`C:\...`, but the file goes to the box folder (copy-on-write). In lockbox, the equivalent is the **fake home**:
everything the app writes "to the home" stays in `--home`. The rest of the system is read-only for the box. What
Sandboxie has and lockbox does not, yet: a graphical list of boxes, browsing and wiping a box's content, and "run any
program in a box" from the context menu. Those are good next steps (a Dolphin service-menu `.desktop` would cover the
last one).

## landrun (Landlock)

Landlock is a kernel feature (5.13+) that lets a process restrict itself to a list of folders, and since 6.7 to
network ports too. It is a **second lock**, independent of namespaces, and it is in lockbox's ideas for later. landrun
alone does not hide the rest of the system (it only denies access) and does not handle the display, keyboard or GPU.

## Sources

- bubblewrap: https://github.com/containers/bubblewrap
- Flatpak (its seccomp list and security context): https://github.com/flatpak/flatpak
- The security-context-v1 protocol: https://wayland.app/protocols/security-context-v1
- Bubblejail: https://github.com/igo95862/bubblejail · ArchWiki: https://wiki.archlinux.org/title/Bubblejail
- Firejail and the setuid discussion: https://github.com/netblue30/firejail/discussions/4601 ·
  https://gitlab.alpinelinux.org/alpine/aports/-/issues/12635
- Unofficial CVE comparison: https://katosh.github.io/agent_sandbox/reference/apptainer-comparison/
- Sandboxie-Plus: https://github.com/sandboxie-plus/Sandboxie
- landrun: https://github.com/Zouuup/landrun
