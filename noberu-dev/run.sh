#!/bin/sh
# Run the native macOS build against repo-local profiles and gamedata.
#   noberu-dev/run.sh -g sgps3 -ll Debug -lc VM
# Game files go in gamedata/<profile>/ (gitignored). Log: noberu-dev/impacto.log
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEV="$ROOT/noberu-dev"
mkdir -p "$DEV/saves"
sed "s|@ROOT@|$ROOT|g" "$DEV/basepaths.lua" > "$DEV/basepaths.gen.lua"
exec "$ROOT/install/Release/Impacto.app/Contents/MacOS/Impacto" \
  -bp "$DEV/basepaths.gen.lua" -gc "$DEV/gamedefinitions.lua" \
  -lf "$DEV/impacto.log" "$@"
