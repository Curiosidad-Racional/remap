#!/bin/dash
cd "$(dirname "$0")"
MODE=
FILT=tail
while [ "$#" -gt 0 ]
do
    case "$1" in
    serio)
        FILT=head
        ;;
    -v)
        MODE=$1
        ;;
    esac
    shift
done
pgrep -x remap >/dev/null && pkill -x intercept
wait
sleep 1
DEV="/dev/input/$(sed -nr 's/^.*sysrq kbd (event[0-9]+) leds.*$/\1/p' /proc/bus/input/devices|$FILT -1)"
exec > .remap.log 2>&1
if [ -n "$MODE" ]
then
    (intercept -g "$DEV" | ./remap -s "$MODE" | uinput -d "$DEV") &
else
    (intercept -g "$DEV" | ./remap -s | uinput -d "$DEV") &
fi
