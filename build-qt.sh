#!/bin/sh
# Builds the Qt port of the 7-Zip file manager.
#
# Stage 1 compiles the platform independent part of 7-Zip (C codecs, archive
# handlers, UI/Common and UI/Agent) into CPP/7zip/Bundles/Qt7z/_o/lib7z.a with
# the same flags the 7zz console program uses.
# Stage 2 builds the Qt user interface in CPP/7zip/UI/Qt and links it.
#
# Outside a nix-shell this re-runs itself inside one.
set -e

root=$(cd "$(dirname "$0")" && pwd)

if [ -z "$IN_7ZQ_NIX_SHELL" ] && command -v nix-shell >/dev/null 2>&1; then
  if ! command -v g++ >/dev/null 2>&1 || ! command -v qmake6 >/dev/null 2>&1; then
    exec nix-shell "$root/shell.nix" --run "IN_7ZQ_NIX_SHELL=1 $root/build-qt.sh $*"
  fi
fi

jobs=$(nproc 2>/dev/null || echo 4)
qmake=$(command -v qmake6 || command -v qmake)

echo "==> 7-Zip engine (lib7z.a)"
make -C "$root/CPP/7zip/Bundles/Qt7z" -f makefile.gcc lib -j"$jobs"

echo "==> Qt file manager (7zQ)"
cd "$root/CPP/7zip/UI/Qt"
"$qmake" 7zQ.pro
make -j"$jobs"

echo
echo "built: $root/CPP/7zip/UI/Qt/7zQ"
