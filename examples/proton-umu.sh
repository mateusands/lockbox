#!/usr/bin/env bash
# A Windows game/program through umu-launcher + Proton, inside the box.
#
# - --allow-userns: umu runs the Steam Linux Runtime (pressure-vessel), which sets up its own bwrap and needs to
#   create user namespaces. Every other protection stays (network, files, secure Wayland, seccomp).
# - Wine Wayland (PROTON_ENABLE_WAYLAND=1): no X11 at all. Proton-CachyOS has it; Valve's Proton Experimental does
#   not, so use --x11 private with it.
# - umu downloads the Steam Runtime (steamrt) the first time, which needs the network: run it once with --net (or
#   copy ~/.local/share/umu from another box) and then always without network.
# - NEVER give --rw to a Proton folder that Steam uses outside the box: a malicious app could modify Proton and run
#   unconfined the next time Steam starts a game. Use a read-only Proton (as here) or a copy that belongs to the box.
set -euo pipefail
base=$HOME/Boxes/my-windows-game
game=$HOME/Games/MyGame                       # the game folder (with the .exe)
proton=/usr/share/steam/compatibilitytools.d/proton-cachyos-slr

mkdir -p "$base/prefix"
box=(--home "$base/home" --allow-userns
     --rw "$game" --rw "$base/prefix" --ro "$proton")
[[ ${1:-} == --test ]] && exec lockbox "${box[@]}" --test
export LOCKBOX_ENV_WINEPREFIX=$base/prefix
export LOCKBOX_ENV_GAMEID=umu-mygame
export LOCKBOX_ENV_STORE=none
export LOCKBOX_ENV_PROTONPATH=$proton
export LOCKBOX_ENV_UMU_RUNTIME_UPDATE=0       # no network: do not try to update the runtime
export LOCKBOX_ENV_PROTON_ENABLE_WAYLAND=1
exec lockbox "${box[@]}" -- umu-run "$game/MyGame.exe" "$@"
