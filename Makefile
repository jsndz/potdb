CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -std=c99 -D_GNU_SOURCE

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BENCH_DIR = bench
BUILD_DIR = build

TARGET = $(BUILD_DIR)/potdb
LIB_TARGET = $(BUILD_DIR)/libptdb.a
ENGINE_SRCS = $(SRC_DIR)/crc32.c $(SRC_DIR)/writer.c $(SRC_DIR)/reader.c
ENGINE_OBJS = $(BUILD_DIR)/crc32.o $(BUILD_DIR)/writer.o $(BUILD_DIR)/reader.o

TEST_UNIT_BIN = $(BUILD_DIR)/test_unit
TEST_EDGE_BIN = $(BUILD_DIR)/test_edge
TEST_STRESS_BIN = $(BUILD_DIR)/test_stress
BENCH_BIN = $(BUILD_DIR)/bench_ptdb

all: potdb lib

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_TARGET): $(ENGINE_OBJS)
	ar rcs $@ $^

lib: $(LIB_TARGET)

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
	@./bench/record.sh "$(MSG)" "$(SYNC)" "$(INTERVAL)"

all_tests: test edge stress

WASM_DIR = web
WASM_TARGET = $(WASM_DIR)/potdb.js
EMCC ?= $(shell which emcc 2>/dev/null || echo /usr/lib/emscripten/emcc)

wasm: $(WASM_TARGET)

$(WASM_TARGET): $(ENGINE_SRCS)
	mkdir -p $(WASM_DIR)
	$(EMCC) -O3 -I$(INC_DIR) $^ \
		-s WASM=1 \
		-s FORCE_FILESYSTEM=1 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s EXPORTED_FUNCTIONS=_ptdb_init,_ptdb_open,_ptdb_close,_ptdb_insert_2d,_ptdb_insert_3d,_ptdb_query_range_2d,_ptdb_reader_open,_ptdb_reader_close,_ptdb_reader_next_chunk,_ptdb_get_point_2d,_malloc,_free \
		-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,FS \
		-o $@

clean:
	rm -rf $(BUILD_DIR) $(WASM_DIR)/potdb.js $(WASM_DIR)/potdb.wasm potdb *.o *.db *.log

.PHONY: all test edge stress bench bench-record all_tests clean potdb lib wasm
