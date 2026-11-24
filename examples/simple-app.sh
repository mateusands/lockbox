#!/usr/bin/env bash
# Any program (a plain binary), with no network and only its own folder.
#   simple-app.sh            starts the app
#   simple-app.sh --test     shows what the app sees and probes the leaks
# Copy it to the box folder (e.g. ~/Boxes/my-app/run.sh) and adjust the 3 variables.
set -euo pipefail
base=$HOME/Boxes/my-app              # this box's folder: the fake home lives in $base/home
program=$HOME/Apps/MyApp             # where the program is (read-only)
data=$HOME/.local/share/myapp        # where it keeps saves/settings (writable)

mkdir -p "$data"
box=(--home "$base/home" --ro "$program" --rw "$data")
[[ ${1:-} == --test ]] && exec lockbox "${box[@]}" --test
exec lockbox "${box[@]}" -- "$program/myapp" "$@"
