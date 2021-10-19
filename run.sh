#!/bin/sh
cd "$(dirname "$0")"
exec >> .remap.log 2>&1
date "+%Y-%m-%d %b %H:%M"
if [ "$1" != serio ]
then FILT=tail
else FILT=head
fi
pgrep -x remap >/dev/null && pkill -x intercept
wait
sleep 1
DEV="/dev/input/$(sed -nr 's/^.*sysrq kbd (event[0-9]+) leds.*$/\1/p' /proc/bus/input/devices|$FILT -1)"
(intercept -g "$DEV" | ./remap -s | uinput -d "$DEV") &
