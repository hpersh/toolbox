CC	= gcc -I. -I..

hp_mem.o:	hp_mem.c
	$(CC) -c hp_mem.c

obj:	hp_mem.o

test:
	make -C ../log all
	make -C ../assert all
	make -C ../except all
	$(CC) -D__UNIT_TEST__ hp_mem.c ../except/hp_except.o ../assert/hp_assert.o ../log/hp_log.o
	./a.out
