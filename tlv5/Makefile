
all:
	gcc -I.. -g -c hp_tlv.c


test:	
	gcc -g -I.. -D__UNIT_TEST__ hp_tlv.c test.c ../list/hp_list.o
	./a.out
