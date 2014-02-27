CFLAGS	= -O3 -fomit-frame-pointer

libovm.so: ovm.c
	gcc $(CFLAGS) -fPIC -c ovm.c
	gcc -shared ovm.o -o libovm.so

test: test.c libovm.so
	gcc $(CFLAGS) test.c -L. libovm.so -o test

.PHONY: clean

clean:
	rm -f *.o *.so test
