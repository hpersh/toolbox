include ../common/Makefile

OBJS	= hp_bmap.o

all:	$(OBJS)

hp_bmap.o: hp_bmap.c
	$(CC) -c hp_bmap.c

test:	hp_bmap.o test.c
	$(CC) test.c hp_bmap.o
	./a.out