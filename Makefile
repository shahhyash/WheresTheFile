CC=gcc
CFLAGS=-Wall -g -fsanitize=address -std=gnu90
OUTPUTS=build bin
FOLDERS=bin build

all: build/server_cmds.o build/client_cmds.o build/manifest_utils.o build/lib.a build/commit_utils.o  build/threads_and_locks.o $(FOLDERS)
	$(CC) $(CFLAGS) -o bin/WTFserver src/server_main.c build/server_cmds.o build/manifest_utils.o build/commit_utils.o build/threads_and_locks.o build/client_cmds.o build/lib.a -lcrypto -lpthread -lz
	$(CC) $(CFLAGS) -o bin/WTF src/client_main.c build/client_cmds.o build/manifest_utils.o build/commit_utils.o build/lib.a -lssl -lcrypto -lz

test: src/WTFtest.c build/server_cmds.o build/client_cmds.o build/manifest_utils.o build/lib.a build/threads_and_locks.o $(FOLDERS)
	$(CC) $(CFLAGS) -o bin/WTFserver src/server_main.c build/server_cmds.o build/manifest_utils.o build/commit_utils.o build/threads_and_locks.o build/client_cmds.o build/lib.a -lcrypto -lpthread -lz
	$(CC) $(CFLAGS) -o bin/WTF src/client_main.c build/client_cmds.o build/manifest_utils.o build/commit_utils.o build/lib.a -lssl -lcrypto -lz
	$(CC) $(CFLAGS) src/WTFtest.c -o bin/WTFtest build/lib.a -lz

build/server_cmds.o: src/server_cmds.c src/server_cmds.h build
	$(CC) $(CFLAGS) -c src/server_cmds.c -o build/server_cmds.o

build/client_cmds.o: src/client_cmds.c src/client_cmds.h build
	$(CC) $(CFLAGS) -c src/client_cmds.c -o build/client_cmds.o

build/manifest_utils.o: src/manifest_utils.c src/manifest_utils.h build
	$(CC) $(CFLAGS) -c src/manifest_utils.c -o build/manifest_utils.o

build/lib.a: build/fileIO.o build/compression.o build/commit_utils.o build
	ar -rs build/lib.a build/fileIO.o build/commit_utils.o build/compression.o

build/fileIO.o: src/fileIO.c src/fileIO.h build
	$(CC) $(CFLAGS) -c src/fileIO.c -o build/fileIO.o

build/compression.o: src/compression.c src/compression.h
	$(CC) $(CFLAGS) -c src/compression.c -o build/compression.o

build/threads_and_locks.o: src/threads_and_locks.c src/threads_and_locks.h
	$(CC) $(CFLAGS) -c src/threads_and_locks.c -o build/threads_and_locks.o

build/commit_utils.o: src/commit_utils.c src/commit_utils.h build
	$(CC) $(CFLAGS) -c src/commit_utils.c -o build/commit_utils.o

build:
	mkdir build

bin:
	mkdir bin

clean:
	rm -r $(OUTPUTS)
