#!/bin/sh
SCRIPT="$(realpath "$0")"
get_device() {
    grep -A 4 "$1" /proc/bus/input/devices \
        |sed -nr 's/^.*sysrq kbd (leds )?(event[0-9]+).*$/\2/p' \
        |head -1
}
if [ -n "$SUDO_USER" ]
then
    # exec >/tmp/remap.log 2>&1
    cd "$(dirname "$SCRIPT")"
    pkill -x -9 intercept

    DEVICE="$1"
    shift
    (while :
    do
        COUNT=0
        DEV="/dev/input/$(get_device "$DEVICE")"
        while [ ! -c "$DEV" ]
        do
            sleep 2
            if [ $((COUNT=COUNT+1)) -gt 10 ]
            then
                COUNT=0
                if pgrep -f '^\./target/release/remap -C '
                then
                    exit
                fi
            fi
            DEV="/dev/input/$(get_device "$DEVICE")"
        done
        if pgrep -f '^\./target/release/remap -C '
        then
            dunstify -a remap -u critical -t 5000 "remap already running"
            exit
        fi
        case $(dunstify -a remap -u critical -t 1000 -A 'default,Reply' 'Launching remap') in
        (1) ;;
        (*) break;;
        esac
        # sleep 1
        # yad --name='yad:*' --button=Ok --text-width=13 --text='Started remap' &
        dunstify -a remap -u normal -t 2000 "Started remap"
        intercept -g "$DEV" \
            | ./target/release/remap -C "feh --no-fehbg --bg-fill red.png" -c "/home/$SUDO_USER/.fehbg" "$@" \
            | uinput -d "$DEV"
        # yad --name='yad:*' --button=Yes --button=No --text='Relaunch remap?' || break
    done
    # yad --name='yad:*' --button=Ok --text-width=13 --text='Stopped remap'
    dunstify -a remap -u critical -t 2000 "Stopped remap"
    ) &
else
    USBKBD="$(grep -B 4 -E 'sysrq kbd (leds )?(event[0-9]+)' /proc/bus/input/devices \
        |grep -B 1 -E '^P: Phys=u'|grep '^N: Name='|grep -v 'DaKai 2.4G RX' \
        |cut -d'"' -f2|tr '\n' '!')"
    OTHKBD="$(grep -B 4 -E 'sysrq kbd (leds )?(event[0-9]+)' /proc/bus/input/devices \
        |grep -B 1 -E '^P: Phys=[^u]'|grep '^N: Name=' \
        |cut -d'"' -f2|head -c -1|tr '\n' '!')"
    while :
    do
        INPUTS="$(yad --separator='|' --focus-field=3 --form --field=device:CB --field=args:CB --field=sudo:H -- "$USBKBD$OTHKBD" '-s!-s -v')"
        if [ "$?" != 0 ]
        then
            exit
        fi
        IFS='|' read -r DEVICE PARAMS PASSWD <<-EOF
		$INPUTS
		EOF

        DEV="/dev/input/$(get_device "$DEVICE")"
        if [ ! -c "$DEV" ]
        then
            yad --button=Ok --text="<span foreground=\"red\">Invalid device:</span> $DEV"
            exit 2
        fi
        if [ -z "$PASSWD" ]
        then
            yad --button=Ok --text='<span foreground="red">Empty password</span>'
            exit 2
        fi
        if ! printf '%s' "$PASSWD" | sudo -S "$SCRIPT" "$DEVICE" $PARAMS
        then
            yad --button=Ok --text='<span foreground="red">Remap failed</span>'
        else
            break
        fi
    done
fi
