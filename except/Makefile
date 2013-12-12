CC	= gcc -g -I. -I..

all:	hp_expect.o

hp_except.o:	hp_except.c
	$(CC) -c hp_except.c

test:
	$(CC) -D__UNIT_TEST__ hp_except.c main.c
	./a.out
