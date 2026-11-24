#!/usr/bin/env bash
# An AppImage inside the box. FUSE does not work in a sandbox, so the AppImage is extracted into $base/app.
# The extraction also runs INSIDE a box: the AppImage can only read itself and write to the extraction folder.
# When the .AppImage file changes (an update), it is extracted again automatically.
set -euo pipefail
base=$HOME/Boxes/my-appimage
image=$HOME/Applications/MyApp.AppImage

stamp="$image $(stat -c '%s %Y' "$image")"
if [[ ! -x $base/app/AppRun || "$(cat "$base/app.stamp" 2>/dev/null)" != "$stamp" ]]; then
    rm -rf "$base/app" "$base/extract"
    mkdir -p "$base/extract"
    lockbox --home "$base/home" --ro "$image" --rw "$base/extract" -- \
        bash -c 'cd "$1" && "$2" --appimage-extract >/dev/null' _ "$base/extract" "$image"
    # newer AppImages extract to AppDir/ and leave squashfs-root as a link to it
    (cd "$base/extract" &&
        if [[ -L squashfs-root ]]; then d=$(readlink squashfs-root); unlink squashfs-root; mv "$d" ../app
        else mv squashfs-root ../app; fi)
    rm -rf "$base/extract"
    echo "$stamp" > "$base/app.stamp"
fi

box=(--home "$base/home" --ro "$base/app")
[[ ${1:-} == --test ]] && exec lockbox "${box[@]}" --test
# some AppImages decide where to store data from the APPIMAGE variable
export LOCKBOX_ENV_APPIMAGE=$image
exec lockbox "${box[@]}" -- "$base/app/AppRun" "$@"
