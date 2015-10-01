CC=gcc -Wall

CFLAGS= -c -g

# this target will compile all the files
all: yash

yash: yash.o
	$(CC) yash.o -o yash

yash.o: yash.c
	$(CC) $(CFLAGS) yash.c

clean:
	rm -rf *.o yash
