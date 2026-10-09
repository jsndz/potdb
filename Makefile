CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -std=c99 -D_GNU_SOURCE

SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.c)
TARGET = potdb

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) db *.o *.db

.PHONY: all clean
