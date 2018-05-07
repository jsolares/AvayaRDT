CC=gcc
CFLAGS=-O2 -Wall
LFLAGS=-levent

server: server.o
	${CC} ${CFLAGS} -o server server.o bstrlib.c ${LFLAGS}

server.o: server.c
	${CC} ${CFLAGS} -c -o server.o server.c

clean:
	rm server server.o
