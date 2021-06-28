CC = gcc 
CCARGS = -std=c11 -Iinclude -Oz
LIBSRC = $(wildcard lib/*.c)


.PHONY: ls mkfs cat cph cpd rm struct

all: ls mkfs cat cph cpd rm struct


mkfs: $(LIBSRC) tools/mkfs.c
	mkdir -p bin
	$(CC) $(CCARGS) -o bin/mbfs-mkfs $^

ls: $(LIBSRC) tools/ls.c
	mkdir -p bin
	$(CC) $(CCARGS) -o bin/mbfs-ls $^

cat: $(LIBSRC) tools/cat.c
	mkdir -p bin
	$(CC) $(CCARGS) -o bin/mbfs-cat $^

cph: $(LIBSRC) tools/cph.c
	mkdir -p bin
	$(CC) $(CCARGS) -o bin/mbfs-cph $^

cpd: $(LIBSRC) tools/cpd.c
	mkdir -p bin
	$(CC) $(CCARGS) -o bin/mbfs-cpd $^

rm: $(LIBSRC) tools/rm.c
	mkdir -p bin
	$(CC) $(CCARGS) -o bin/mbfs-rm $^

struct: $(LIBSRC) tools/struct.c
	mkdir -p bin
	$(CC) $(CCARGS) -o bin/mbfs-struct $^

