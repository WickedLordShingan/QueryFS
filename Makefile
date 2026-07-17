CC := gcc
CFLAGS := -Wall -Wextra -g
BIN_DIR := ./binaries

# fuse3 flags via pkg-config
FUSE_CFLAGS := $(shell pkg-config --cflags fuse3)
FUSE_LIBS   := $(shell pkg-config --libs fuse3)

.PHONY: all test scanner_test parser_test interpreter_test baalafs clean dirs

all: dirs baalafs scanner_test parser_test interpreter_test

test: scanner_test parser_test interpreter_test

dirs:
	mkdir -p $(BIN_DIR)

scanner_test: dirs
	$(CC) $(CFLAGS) ./Scanner/scanner.c ./Scanner/scanner_unit_test.c \
		-o $(BIN_DIR)/scanner_test
	$(BIN_DIR)/scanner_test

parser_test: dirs
	$(CC) $(CFLAGS) ./Parser/parser.c ./Parser/parser_unit_test.c \
		./Scanner/scanner.c \
		-o $(BIN_DIR)/parser_test
	$(BIN_DIR)/parser_test

interpreter_test: dirs
	$(CC) $(CFLAGS) ./Interpreter/interpreter.c ./Interpreter/interpreter_unit_test.c \
		./Parser/parser.c ./Scanner/scanner.c \
		-o $(BIN_DIR)/interpreter_test
	$(BIN_DIR)/interpreter_test

baalafs: dirs
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) baalafs.c \
		./Interpreter/interpreter.c ./Parser/parser.c ./Scanner/scanner.c \
		./Universe_and_related/universe_and_related.c \
		-o $(BIN_DIR)/baalafs $(FUSE_LIBS)

clean:
	rm -f $(BIN_DIR)/*
