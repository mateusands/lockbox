#!/usr/bin/env bash
# An app that only speaks X11. Examples: Qt 5 programs when the system has no qt5-wayland (mGBA, for instance, dies
# with "could not load the Qt platform plugin xcb"/SIGABRT without X11), Wine/Proton without a Wayland driver, old
# SDL games.
#
# --x11 private starts an Xwayland JUST for this box, in a window of its own (maximize it or make it fullscreen from
# KDE with Meta+F). The app sees none of your other windows and cannot read what you type in other programs.
# X11P_FULLSCREEN=1 starts it fullscreen; X11P_GEOMETRY=2560x1440 changes its size.
#
# Qt 6 apps (Eden, RPCS3...) sometimes turn Wayland off on their own and try X11. Before using --x11 private with
# them, try forcing Wayland: LOCKBOX_ENV_QT_QPA_PLATFORM=wayland (Eden also needs
# LOCKBOX_ENV_I_WANT_A_BROKEN_WAYLAND_UI=1).
set -euo pipefail
base=$HOME/Boxes/my-x11-app
program=$HOME/Apps/X11App

box=(--home "$base/home" --x11 private --ro "$program")
[[ ${1:-} == --test ]] && exec lockbox "${box[@]}" --test
exec lockbox "${box[@]}" -- "$program/app" "$@"
