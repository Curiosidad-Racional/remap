#!/bin/sh
get_device() {
    grep -A 4 "$1" /proc/bus/input/devices \
        |sed -nr 's/^.*sysrq kbd (leds )?(event[0-9]+).*$/\2/p' \
        |head -1
}
DEV="/dev/input/$(get_device "SONiX Calibur V2 TE")"
sleep 1
intercept -g "$DEV" \
    | ./target/release/remap \
    | uinput -d "$DEV"
