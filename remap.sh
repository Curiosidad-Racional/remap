#!/bin/sh
SCRIPT="$(realpath "$0")"
if [ -n "$SUDO_USER" ]
then
    cd "$(dirname "$SCRIPT")"
    pkill -x -9 intercept

    DEV="$1"
    shift
    (while :
    do
        sleep 1
        yad --button=Ok --text-width=13 --text='Started remap'
        intercept -g "$DEV" | ./remap -C "feh --no-fehbg --bg-fill red.png" -c "/home/$SUDO_USER/.fehbg" "$@" | uinput -d "$DEV"
        yad --button=Yes --button=No --text='Relaunch remap?' || break
    done
    yad --button=Ok --text-width=13 --text='Stopped remap') &
else
    USBKBD="$(grep -B 4 -E 'sysrq kbd (leds )?(event[0-9]+)' /proc/bus/input/devices|grep -B 1 -E '^P: Phys=u'|grep '^N: Name='|grep -v 'DaKai 2.4G RX'|cut -d'"' -f2|tr '\n' '!')"
    OTHKBD="$(grep -B 4 -E 'sysrq kbd (leds )?(event[0-9]+)' /proc/bus/input/devices|grep -B 1 -E '^P: Phys=[^u]'|grep '^N: Name='|cut -d'"' -f2|head -c -1|tr '\n' '!')"
    INPUTS="$(yad --separator='"' --focus-field=3 --form --field=device:CB --field=args:CB --field=sudo:H -- "$USBKBD$OTHKBD" '-s!-s -v')"
    if [ "$?" != 0 ]
    then
        return
    fi
    DEVICE="$(echo "$INPUTS"|cut -d'"' -f1)"
    PARAMS="$(echo "$INPUTS"|cut -d'"' -f2)"
    PASSWD="$(echo "$INPUTS"|cut -d'"' -f3)"

    DEVICE="/dev/input/$(grep -A 4 "$DEVICE" /proc/bus/input/devices|sed -nr 's/^.*sysrq kbd (leds )?(event[0-9]+).*$/\2/p'|head -1)"
    if [ ! -c "$DEVICE" ]
    then
        yad --button=Ok --text="<span foreground=\"red\">Invalid device:</span> $DEVICE"
        return
    fi
    if [ -z "$PASSWD" ]
    then
        yad --button=Ok --text='<span foreground="red">Empty password</span>'
        return
    fi
    if ! echo "$PASSWD" | sudo -S "$SCRIPT" "$DEVICE" $PARAMS
    then
        yad --button=Ok --text='<span foreground="red">Remap failed</span>'
    fi
fi