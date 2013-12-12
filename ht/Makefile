CC	= gcc -I. -I..

all:
	$(CC) -c hp_ht.c

test:
	$(CC) -D__UNIT_TEST__ hp_ht.c
	./a.out
