CC	= gcc -I. -I..

all:
	$(CC) -c hp_mem.c

test:
	make -C ../log all
	make -C ../assert all
	make -C ../except all
	$(CC) -D__UNIT_TEST__ hp_mem.c ../except/hp_except.o ../assert/hp_assert.o ../log/hp_log.o
	./a.out
