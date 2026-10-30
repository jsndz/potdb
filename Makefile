CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -std=c99 -D_GNU_SOURCE

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BENCH_DIR = bench
BUILD_DIR = build

TARGET = $(BUILD_DIR)/potdb
ENGINE_SRCS = $(SRC_DIR)/crc32.c $(SRC_DIR)/writer.c $(SRC_DIR)/reader.c

TEST_UNIT_BIN = $(BUILD_DIR)/test_unit
TEST_EDGE_BIN = $(BUILD_DIR)/test_edge
TEST_STRESS_BIN = $(BUILD_DIR)/test_stress
BENCH_BIN = $(BUILD_DIR)/bench_ptdb

all: potdb

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRC_DIR)/main.c $(ENGINE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

potdb: $(TARGET)
	cp $(TARGET) potdb

$(TEST_UNIT_BIN): $(TEST_DIR)/test_unit.c $(ENGINE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_EDGE_BIN): $(TEST_DIR)/test_edge_cases.c $(ENGINE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_STRESS_BIN): $(TEST_DIR)/test_stress.c $(ENGINE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BENCH_BIN): $(BENCH_DIR)/bench_ptdb.c $(ENGINE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

test: $(TEST_UNIT_BIN)
	@./$(TEST_UNIT_BIN)

edge: $(TEST_EDGE_BIN)
	@./$(TEST_EDGE_BIN)

stress: $(TEST_STRESS_BIN)
	@./$(TEST_STRESS_BIN)

bench: $(BENCH_BIN)
	@./$(BENCH_BIN)

bench-record: $(BENCH_BIN)
	@./bench/record.sh "$(MSG)"

all_tests: test edge stress

clean:
	rm -rf $(BUILD_DIR) potdb *.o *.db *.log

.PHONY: all test edge stress bench bench-record all_tests clean potdb
