INC	= -I..

CFLAGS	= -g

all:
	gcc $(CFLAGS) $(INC) -c hp_tlv.c


test:	
	make -C ../stream
	gcc $(CFLAGS) $(INC) -D__UNIT_TEST__ hp_tlv.c test.c ../stream/hp_stream.o
	./a.out
