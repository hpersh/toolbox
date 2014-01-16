
#DIRS	:= $(shell bash -c 'for f in *; do if [ -d $$f ]; then echo $$f; fi; done')
DIRS	= assert bmap

all:
	for d in $(DIRS); do make -C $$d; done
	ld -r -o hp.o `find $(DIRS) -name '*.o'`
