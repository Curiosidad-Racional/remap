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
PID=$(pgrep -x remap)
if [ -n "$PID" ]
then
    pkill -x intercept
    kill "$PID"
fi
DEV="/dev/input/$(sed -nr '/leds/{s/^.*sysrq kbd (leds )?(event[0-9]+).*$/\2/p}' /proc/bus/input/devices|$FILT -1)"
exec > .remap.log 2>&1
sleep 1
if [ -n "$MODE" ]
then
    (intercept -g "$DEV" | ./remap -s "$MODE" | uinput -d "$DEV") &
else
    (intercept -g "$DEV" | ./remap -s | uinput -d "$DEV") &
fi
