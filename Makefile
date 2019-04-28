CC=gcc
CFLAGS=-Wall -g -fsanitize=address -std=gnu90
OUTPUTS=build bin
FOLDERS=bin build projects

all: build/server_cmds.o build/client_cmds.o build/manifest_utils.o build/lib.a $(FOLDERS)
	$(CC) $(CFLAGS) -o bin/WTFserver src/server_main.c build/server_cmds.o build/lib.a -lpthread -lz
	$(CC) $(CFLAGS) -o bin/WTF src/client_main.c build/client_cmds.o build/manifest_utils.o build/lib.a -lssl -lcrypto -lz

build/server_cmds.o: src/server_cmds.c src/server_cmds.h build
	$(CC) $(CFLAGS) -c src/server_cmds.c -o build/server_cmds.o

build/client_cmds.o: src/client_cmds.c src/client_cmds.h build
	$(CC) $(CFLAGS) -c src/client_cmds.c -o build/client_cmds.o

build/manifest_utils.o: src/manifest_utils.c src/manifest_utils.h build
	$(CC) $(CFLAGS) -c src/manifest_utils.c -o build/manifest_utils.o

build/lib.a: build/fileIO.o build/compression.o build
	ar -rs build/lib.a build/fileIO.o build/compression.o

build/fileIO.o: src/fileIO.c src/fileIO.h build
	$(CC) $(CFLAGS) -c src/fileIO.c -o build/fileIO.o

build/compression.o: src/compression.c src/compression.h
	$(CC) $(CFLAGS) -c src/compression.c -o build/compression.o

build:
	mkdir build

bin:
	mkdir bin

projects:
	mkdir projects

clean:
	rm -r $(OUTPUTS)
