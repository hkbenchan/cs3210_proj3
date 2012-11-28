targets = ypfs hello

all: $(targets)

hello: hello.c
	gcc -Wall `pkg-config fuse --cflags --libs` hello.c -o hello

ypfs: ypfs.c
	gcc -Wall `pkg-config fuse --cflags --libs` ypfs.c -o ypfs

clean:
	rm -f *.o
	rm -f $(targets)
