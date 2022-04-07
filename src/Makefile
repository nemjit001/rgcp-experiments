.PHONY: all clean

CC=gcc
CFLAGS=-g -Wall -Werror -UDEBUG
LDLIBS=-lrgcp

all: tcp_throughput rgcp_throughput

clean:
	rm -f *.o

tcp_throughput.o: tcp_throughput.c
tcp_throughput: tcp_throughput.o

rgcp_throughput.o: rgcp_throughput.c
rgcp_throughput: rgcp_throughput.o

