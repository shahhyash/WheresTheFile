CC=gcc
CFLAGS=-Werror -Wall -g -fsanitize=address -std=gnu90
OUTPUTS=build bin

all: src/client.c src/server.c build/lib.a bin build
	$(CC) $(CFLAGS) -o bin/WTFserver src/server.c build/lib.a -lpthread
	$(CC) $(CFLAGS) -o bin/WTF src/client.c build/lib.a

build/lib.a: build/fileIO.o build
	ar -rs build/lib.a build/fileIO.o

build/fileIO.o: src/fileIO.c src/fileIO.h build
	$(CC) $(CFLAGS) -c src/fileIO.c -o build/fileIO.o

build:
	mkdir build

bin:
	mkdir bin

clean:
	rm -r $(OUTPUTS)
