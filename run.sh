#!/bin/sh
cd "$(dirname "$0")"
if [ "$1" == serio ]
then FILT=head
else FILT=tail
fi
pgrep -x remap >/dev/null && pkill -x intercept
wait
sleep 1
DEV="/dev/input/$(sed -nr 's/^.*sysrq kbd (event[0-9]+) leds.*$/\1/p' /proc/bus/input/devices|$FILT -1)"
(intercept -g "$DEV" | ./remap -s | uinput -d "$DEV") >log.txt 2>&1 &
