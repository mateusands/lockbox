#!/usr/bin/env bash
# An app that needs the internet (a chat client, a launcher) but not your files.
# With --net, everything the app can read CAN leave the PC. So allow as few folders as possible and, if it needs no
# sound, use --no-audio (PipeWire also gives the microphone).
set -euo pipefail
base=$HOME/Boxes/online-app
program=$HOME/Apps/OnlineApp
downloads=$base/downloads                     # an exchange folder that belongs to this box, not your ~/Downloads

mkdir -p "$downloads"
box=(--home "$base/home" --net --no-audio --ro "$program" --rw "$downloads")
[[ ${1:-} == --test ]] && exec lockbox "${box[@]}" --test
exec lockbox "${box[@]}" -- "$program/app" "$@"
