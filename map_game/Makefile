# Makefile for compiling labyrinth.c

CC = gcc
CFLAGS = -g -Wall -Wextra -Werror 
LDFLAGS =

TARGET = labyrinth
SRCS = labyrinth.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean debug

all: CFLAGS += -O0 -g
all: $(TARGET)

release: CFLAGS += -O2
release: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS) *.o

debug: all
	gdb ./$(TARGET)