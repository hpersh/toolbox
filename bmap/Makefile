include ../common/Makefile

OBJS	= hp_bmap.o

all:	$(OBJS)

hp_bmap.o: hp_bmap.c
	$(CC) -c hp_bmap.c

obj:	hp_bmap.o

test:	hp_bmap.o test.c
	make -C ../mem obj
	make -C ../assert obj
	$(CC) test.c hp_bmap.o ../mem/hp_mem.o ../assert/hp_assert.o
	./a.out