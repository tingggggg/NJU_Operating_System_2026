CC = gcc
CFLAGS = -fPIC -shared -O2 -Wall -Wextra -D_GNU_SOURCE
TARGET = libwheel.so
SRC = wheel.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

.PHONY: all clean
