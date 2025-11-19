CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = myPstree

all: $(TARGET)

$(TARGET): myPstree.c
	$(CC) $(CFLAGS) -o $(TARGET) myPstree.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
