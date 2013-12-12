CC	= gcc -I. -I..

all:
	$(CC) -c hp_bmap.c

test:
	$(CC) -D__UNIT_TEST__ hp_bmap.c
	./a.out