#!/bin/sh
# Send dev input to a running build started with IMPACTO_DEV_INPUT set by run.sh.
#   noberu-dev/input.sh pad B        noberu-dev/input.sh click 640 360
printf '%s\n' "$*" >> "$(dirname "$0")/devinput.txt"
