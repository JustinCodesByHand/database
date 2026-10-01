CC      := gcc
CSTD    := -std=c17
WARN    := -Wall -Wextra -Werror -Wshadow -Wconversion -Wpointer-arith
DEBUG   := -g3 -O0
SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  := $(CSTD) $(WARN) $(DEBUG) $(SAN) -Isrc
LDFLAGS := $(SAN)

SRC       := $(shell find src -name '*.c')
SRC_NOMAIN:= $(filter-out src/main.c,$(SRC))
# scratch.c has its own main(); it must NOT be linked into build/tests.
TEST_SRC  := $(filter-out tests/scratch.c,$(shell find tests -name '*.c'))

BUILD := build

.PHONY: all test scratch run clean fmt

all: $(BUILD)/minidb

$(BUILD)/minidb: $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(BUILD)/tests: $(SRC_NOMAIN) $(TEST_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -Itests $(SRC_NOMAIN) $(TEST_SRC) -o $@ $(LDFLAGS)

# THE command you will run a thousand times
test: $(BUILD)/tests
	./$(BUILD)/tests

# Debugging aid: tokenize one string and print every token it produced.
#   make scratch
#   make scratch ARG='"SELECT"'
# Timeout is deliberate: the current lexer hangs on any non-whitespace input,
# and a hang with no output is much harder to read than exit=124.
scratch: $(BUILD)/scratch
	@timeout 5 ./$(BUILD)/scratch $(ARG); \
	 rc=$$?; \
	 if [ $$rc -eq 124 ]; then \
	   echo ""; echo ">>> HUNG (exit=124). The loop did not advance the cursor."; \
	 fi; \
	 exit $$rc

$(BUILD)/scratch: src/tokenizer.c src/tokenizer.h tests/scratch.c | $(BUILD)
	$(CC) $(CFLAGS) -Itests src/tokenizer.c tests/scratch.c -o $@ $(LDFLAGS)

run: $(BUILD)/minidb
	./$(BUILD)/minidb mini.db

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) *.db *.wal
	rm -f minidb.out