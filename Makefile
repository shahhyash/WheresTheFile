CC=gcc
CFLAGS=-Werror -Wall -g -fsanitize=address -std=gnu90
OUTPUTS=build bin
FOLDERS=bin build projects projects_client

all: build/server_cmds.o build/client_cmds.o build/lib.a $(FOLDERS)
	$(CC) $(CFLAGS) -o bin/WTFserver_main src/server_main.c build/server_cmds.o build/lib.a -lpthread
	$(CC) $(CFLAGS) -o bin/WTF src/client_main.c build/client_cmds.o build/lib.a -lssl -lcrypto

build/server_cmds.o: src/server_cmds.c src/server_cmds.h build
	$(CC) $(CFLAGS) -c src/server_cmds.c -o build/server_cmds.o

build/client_cmds.o: src/client_cmds.c src/client_cmds.h build
	$(CC) $(CFLAGS) -c src/client_cmds.c -o build/client_cmds.o

build/lib.a: build/fileIO.o build
	ar -rs build/lib.a build/fileIO.o

build/fileIO.o: src/fileIO.c src/fileIO.h build
	$(CC) $(CFLAGS) -c src/fileIO.c -o build/fileIO.o

build:
	mkdir build

bin:
	mkdir bin

projects:
	mkdir projects

projects_client:
	mkdir projects_client

clean:
	rm -r $(OUTPUTS)
