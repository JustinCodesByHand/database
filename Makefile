CC      := gcc
CSTD    := -std=c17
WARN    := -Wall -Wextra -Werror -Wshadow -Wconversion -Wpointer-arith
DEBUG   := -g3 -O0
SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  := $(CSTD) $(WARN) $(DEBUG) $(SAN) -Isrc
LDFLAGS := $(SAN)

SRC       := $(shell find src -name '*.c')
SRC_NOMAIN:= $(filter-out src/main.c,$(SRC))
TEST_SRC  := $(shell find tests -name '*.c')

BUILD := build

.PHONY: all test run clean fmt

all: $(BUILD)/minidb

$(BUILD)/minidb: $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(BUILD)/tests: $(SRC_NOMAIN) $(TEST_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -Itests $(SRC_NOMAIN) $(TEST_SRC) -o $@ $(LDFLAGS)

# THE command you will run a thousand times
test: $(BUILD)/tests
	./$(BUILD)/tests

run: $(BUILD)/minidb
	./$(BUILD)/minidb mini.db

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) *.db *.wal