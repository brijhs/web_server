CC=gcc
CFLAGS=-g -O2 -Wall
LDLIBS=-lpthread

all: server

server: server.c

clean:
	rm -rf *.o *~ *.dSYM echoserver echoserver-nothreads

