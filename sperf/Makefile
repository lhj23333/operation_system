CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

all: sperf

sperf: sperf.c
	$(CC) $(CFLAGS) -o sperf sperf.c

clean:
	rm -f sperf

.PHONY: all clean
