CC	= gcc -g -I. -I..

OBJS	= hp_log.o

all: $(OBJS)
	$(CC) -c hp_log.c

include ../common/Makefile

test:
	make -C ../assert
	$(CC) -D__UNIT_TEST__ hp_log.c stubs.c test.c ../assert/hp_assert.o
	./a.out
