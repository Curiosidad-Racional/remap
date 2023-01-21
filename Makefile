remap: remap.c
	gcc -O3 remap.c -o remap -lxcb -lX11

classname: classname.c
	gcc classname.c -o classname -lxcb

modal: modal.c
	gcc modal.c -o modal -lxcb

box: box.c
	gcc box.c -o box -lxcb -lxcb-icccm -lcairo

capslock_xlib: capslock_xlib.c
	gcc capslock_xlib.c -o capslock_xlib -lX11