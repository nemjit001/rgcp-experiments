.PHONY: all clean

CC=gcc
CFLAGS=-g -Wall -Werror -UDEBUG -I${HOME}/include/
LDFLAGS=-L${HOME}/libs/
LDLIBS=-lrgcp

all: tcp_throughput rgcp_throughput rgcp_stability_client

clean:
	rm -f *.o

tcp_throughput.o: tcp_throughput.c
tcp_throughput: tcp_throughput.o

rgcp_throughput.o: rgcp_throughput.c
rgcp_throughput: rgcp_throughput.o

rgcp_stability_client.o: rgcp_stability_client.c
rgcp_stability_client: rgcp_stability_client.o
