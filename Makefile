targets = apfs

all: $(targets)

apfs: apfs.c
	gcc -Wall `pkg-config fuse libexif --cflags --libs` apfs.c -o apfs -lcurl -lssl

mount: apfs
	./apfs temp/

unmount:
	-fusermount -u temp/

update:
	git pull && make apfs

clean:
	rm -f *.o
	rm -f $(targets)