CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -I.

TARGET := app
SRC := main.c $(wildcard rfilesystem/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)