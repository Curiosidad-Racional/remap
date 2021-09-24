remap: remap.c
	gcc -O3 remap.c -o remap -lxcb

classname: classname.c
	gcc classname.c -o classname -lxcb

box: box.c
	gcc box.c -o box -lxcb -lxcb-icccm -lcairo