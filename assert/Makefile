CC	= gcc -I. -I..

all:
	$(CC) -c hp_assert.c

test:
	$(CC) -D__UNIT_TEST__ hp_assert.c
	./a.out