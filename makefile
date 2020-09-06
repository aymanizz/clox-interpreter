# Makefile for building a single configuration of the C interpreter. It can take
# variables passed in for:
#
# MODE         "debug" or "release".
# TARGET       Name of the output executable.
# BIN_DIR      Output directory for executable.
# BUILD_DIR    Output directory for object files.
# SOURCE_DIR   Directory where source files are found.
# INCLUDE_DIR  Directory where header files are found.

CFLAGS := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
INCLUDE_DIR := include
SOURCE_DIR := src
BUILD_DIR := build
BIN_DIR := bin
TARGET := clox

# Mode configuration.
ifeq ($(MODE),debug)
	CFLAGS += -O0 -DDEBUG -g
	OBJ_DIR := $(BUILD_DIR)/debug
else
	CFLAGS += -O3 -flto
	OBJ_DIR := $(BUILD_DIR)/release
endif

CFLAGS += -I$(INCLUDE_DIR)
# stream checks generates annoying false-positives warnings
CLANG_TIDY_CHECKS=-*,clang-analyzer-*,-clang-analyzer-cplusplus*,-clang-analyzer-alpha.unix.Stream

# Files.
HEADERS := $(wildcard $(INCLUDE_DIR)/*.h)
SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS := $(addprefix $(OBJ_DIR)/, $(notdir $(SOURCES:.c=.o)))

# Targets ---------------------------------------------------------------------

all: format lint build

ci: check-format build  # lint has lots of false-positive so disabled in ci

format: $(SOURCES) $(HEADERS)
	@ clang-format -style=file -i $^

check-format: $(SOURCES) $(HEADERS)
	@ for f in $^; do echo "checking $$f"; clang-format -style=file $$f | diff $$f - || exit 1; done

build: $(BIN_DIR)/$(TARGET)

lint: $(SOURCES) $(HEADERS)
	@ OUTPUT=$$(clang-tidy -checks=$(CLANG_TIDY_CHECKS) $^ -- -I$(INCLUDE_DIR)); \
	if test -n "$$OUTPUT"; then echo "$$OUTPUT" && exit 1; fi

clean:
	@ $(RM) -rf $(BUILD_DIR) $(BIN_DIR)

# Link the interpreter.
$(BIN_DIR)/$(TARGET): $(OBJECTS)
	@ printf "%8s %-40s %s\n" $(CC) $@ "$(CFLAGS)"
	@ mkdir -p $(BIN_DIR)
	@ $(CC) $(CFLAGS) $^ -o $@

# Compile object files.
$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.c $(HEADERS)
	@ printf "%8s %-40s %s\n" $(CC) $< "$(CFLAGS)"
	@ mkdir -p $(OBJ_DIR)
	@ $(CC) -c $(CFLAGS) -o $@ $<

.PHONY: all clean
