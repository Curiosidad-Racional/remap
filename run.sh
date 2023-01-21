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
    pkill -x -9 intercept
    kill -9 "$PID"
fi
sleep 1
DEV="/dev/input/$(sed -nr '/leds/{s/^.*sysrq kbd (leds )?(event[0-9]+).*$/\2/p}' /proc/bus/input/devices|$FILT -1)"
exec > .remap.log 2>&1
SCRIPT_PATH="$(dirname $0)"
if [ -n "$MODE" ]
then
    (intercept -g "$DEV" | ./remap -C "feh --no-fehbg --bg-fill $SCRIPT_PATH/red.png" -c "/home/$SUDO_USER/.fehbg" -s "$MODE" | uinput -d "$DEV") &
else
    (intercept -g "$DEV" | ./remap -C "feh --no-fehbg --bg-fill $SCRIPT_PATH/red.png" -c "/home/$SUDO_USER/.fehbg" -s | uinput -d "$DEV") &
fi
