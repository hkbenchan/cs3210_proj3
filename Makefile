targets = ypfs hello

all: $(targets)

hello: hello.c
	gcc -Wall `pkg-config fuse --cflags --libs` hello.c -o hello

ypfs: ypfs.c
	gcc -Wall `pkg-config fuse libexif --cflags --libs` ypfs.c -o ypfs -lcurl

mount: ypfs
	./ypfs temp/

unmount:
	-fusermount -u temp/

update:
	git pull && make ypfs

clean:
	rm -f *.o
	rm -f $(targets)

clog:
	echo > /tmp/ypfs/log