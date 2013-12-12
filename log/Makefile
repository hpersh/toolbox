include ../common/Makefile

CC	= gcc -I. -I..

OBJS	= hp_log.o

BIN	= a.out

all: $(OBJS)
	$(CC) -c hp_log.c

test:
	make -C ../assert
	$(CC) -D__UNIT_TEST__ hp_log.c stubs.c test.c ../assert/hp_assert.o
	./a.out
