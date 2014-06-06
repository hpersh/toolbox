INC	= -I..

CFLAGS	= -g

hp_json.o:
	gcc $(CFLAGS) $(INC) -c hp_json.c

test: hp_json.o
	gcc $(CFLAGS) $(INC) hp_json.o test.c ../stream/hp_stream.o
	./a.out

