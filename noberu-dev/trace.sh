#!/bin/sh
# Run under lldb for N seconds, printing the full log (console logging must be on
# in userconfig.toml; plain runs lose everything after "Initialising video system").
# A crash prints a backtrace.  noberu-dev/trace.sh 15 -g sgsteam > out.log
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SECS=$1; shift
export IMPACTO_MUTE="${IMPACTO_MUTE:-1}"
sed "s|@ROOT@|$ROOT|g" "$ROOT/noberu-dev/basepaths.lua" > "$ROOT/noberu-dev/basepaths.gen.lua"
exec perl -e "alarm $SECS; exec @ARGV" lldb --batch -o run -o "bt 25" -k "bt 25" -k quit -- \
  "$ROOT/install/Release/Impacto.app/Contents/MacOS/Impacto" \
  -bp "$ROOT/noberu-dev/basepaths.gen.lua" -gc "$ROOT/noberu-dev/gamedefinitions.lua" "$@"
