CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -std=c99 -D_GNU_SOURCE

TARGET = potdb
TEST_TARGET = test_ptdb

SRCS = src/main.c src/ptdb.c
TEST_SRCS = src/test_ptdb.c src/ptdb.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

$(TEST_TARGET): $(TEST_SRCS)
	$(CC) $(CFLAGS) $(TEST_SRCS) -o $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) *.o *.db *.log

.PHONY: all test clean
