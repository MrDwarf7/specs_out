# Generic Cross-Platform C/C++ Makefile
# Usage: make [BUILD_TYPE=debug|release] [VERBOSE=1]
#
# Adapted from tel_fw — kept generic so it can be ported to other projects.
# Customize PROJECT_NAME, LDLIBS, and the TEST_* section below per-project.

# Project configuration
PROJECT_NAME = specs_out

# Default to parallel build. Respect explicit -j from command line.
NPROCS := $(shell nproc)
ifneq (,$(findstring j,$(MAKEFLAGS)))
  # user provided -j on command line, respect it
else
  MAKEFLAGS += -j$(NPROCS)
endif

CC = gcc
CPPCHECK_VERSION = c23

# Include directories
# -I
LIBS =

# External libraries
# -l
LDLIBS = -lpthread -lrt

# Platform detection
ifeq ($(OS),Windows_NT)
	PLATFORM = windows
	EXE_EXT = .exe
	MKDIR = mkdir
	RMDIR = rmdir /S /Q
	PATH_SEP = \\$(strip)
	FIX_PATH = $(subst /,\\,$1)
	FIND = $(shell where /s /b 2>nul || echo *)
else
	PLATFORM = unix
	EXE_EXT =
	MKDIR = mkdir -p
	RMDIR = rm -rf
	PATH_SEP = /
	FIX_PATH = $1
	FIND = find
endif

# Note on the usage of Wswitch flags:
# They can be disabled locally in the file via:
#
##### `pragma GCC diagnostic ignored "-Wswitch-default"`

ERROR_CATCH_FLAGS = -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion -Wformat-security -Wswitch -Wswitch-enum

SECURITY_FLAGS = -fstack-protector-strong -fstack-clash-protection -pie -fPIE -ftrapv

# File extensions and directories
SRC_EXT = c
SRC_DIR = ./src
LIBS_DIR = ./libs
VENDOR_DIR = ./vendor
TEST_DIR = ./test

# Build type — determines output directory
#   debug  -> build/
#   release -> build_rel/
BUILD_TYPE ?= debug
ifeq ($(BUILD_TYPE),release)
  BUILD_DIR = build_rel
else
  BUILD_DIR = build
endif

OBJ_DIR = $(BUILD_DIR)/obj
TARGET  = $(BUILD_DIR)/$(PROJECT_NAME)$(EXE_EXT)

# Source files — split src/ (strict flags) from libs/ (vendored, relaxed flags)
SRC_SOURCES = $(shell $(FIND) $(SRC_DIR) -name '*.$(SRC_EXT)' 2>/dev/null | sed 's|^\./||' | sort)
LIB_SOURCES = $(shell $(FIND) $(LIBS_DIR) -name '*.$(SRC_EXT)' 2>/dev/null | sed 's|^\./||' | sort)
SRC_OBJECTS = $(patsubst %, $(OBJ_DIR)/%, $(SRC_SOURCES:%=%.o))
LIB_OBJECTS = $(patsubst %, $(OBJ_DIR)/%, $(LIB_SOURCES:%=%.o))
OBJECTS = $(SRC_OBJECTS) $(LIB_OBJECTS)
DEPS = $(OBJECTS:.o=.d)

# Relaxed flags for vendored libs (no -Werror, no conversion warnings)
LIB_CFLAGS = $(STD_FLAG) -O2 -w -I$(LIBS_DIR) $(addprefix -I,$(shell $(FIND) $(LIBS_DIR) -type d 2>/dev/null))

# Compiler flags
GCC_VERSION := $(shell $(CC) -dumpfullversion 2>/dev/null || $(CC) -dumpversion 2>/dev/null || echo "0")
GCC_MAJOR := $(word 1, $(subst ., ,$(GCC_VERSION)))
STD_FLAG := $(shell if [ "$(GCC_MAJOR)" -ge 14 ] 2>/dev/null; then echo "-std=gnu23"; else echo "-std=gnu2x"; fi)
BASE_CFLAGS = $(ERROR_CATCH_FLAGS) $(STD_FLAG) -march=native -pthread \
								$(SECURITY_FLAGS) \
								-I$(SRC_DIR) -I$(LIBS_DIR) $(addprefix -I,$(shell $(FIND) $(LIBS_DIR) -type d 2>/dev/null))

DEBUG_CFLAGS = -g -O0 -DDEBUG
RELEASE_CFLAGS = -O3 -DNDEBUG -D_FORTIFY_SOURCE=3 -flto

# Platform-specific flags
ifeq ($(PLATFORM),windows)
	BASE_CFLAGS += -Wunicode
	LDFLAGS = -Wl,--subsystem,console -Wunicode
else
	LDFLAGS = -Wl,-z,relro,-z,now,-z,noexecstack
endif

# Build type configuration
ifeq ($(BUILD_TYPE),release)
	CFLAGS = $(BASE_CFLAGS) $(RELEASE_CFLAGS)
else
	CFLAGS = $(BASE_CFLAGS) $(DEBUG_CFLAGS)
endif

# Verbose output control
VERBOSE ?= 0
ifeq ($(VERBOSE),1)
  Q =
else
  Q = @
endif

####################################################################################################

# Default target
all: $(TARGET)

# Link target
$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(Q)printf "Linking $@\n"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

# Compile src/ files with strict flags
$(OBJ_DIR)/src/%.c.o: src/%.c | $(OBJ_DIR)
	$(Q)printf "Compiling $<\n"
	$(Q)$(MKDIR) $(dir $@)
	$(Q)$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Compile libs/ (vendored) with relaxed flags
$(OBJ_DIR)/libs/%.c.o: libs/%.c | $(OBJ_DIR)
	$(Q)printf "Compiling [lib] $<\n"
	$(Q)$(MKDIR) $(dir $@)
	$(Q)$(CC) $(LIB_CFLAGS) -I$(LIBS_DIR) -MMD -MP -c $< -o $@

# Create directories
$(BUILD_DIR) $(OBJ_DIR):
	$(Q)$(MKDIR) $(call FIX_PATH,$@)

# Format — run clang-format over project code (src/ and test/), skip libs/ and vendor/
FORMAT_DIRS = $(SRC_DIR) $(TEST_DIR)
FORMAT_FILES = $(shell $(FIND) $(FORMAT_DIRS) -name '*.c' -o -name '*.h' 2>/dev/null | sort)

format:
	$(Q)printf "Formatting project code...\n"
	$(Q)clang-format -i $(FORMAT_FILES)

# Build targets — format first, then recurse with the right BUILD_TYPE
build_debug: format
	@printf "Building debug version...\n"
	$(MAKE) BUILD_TYPE=debug

build_release: format
	@printf "Building release version...\n"
	$(MAKE) BUILD_TYPE=release

# Run targets — build then run
run_debug: build_debug
	@printf "Running $(PROJECT_NAME) (debug)...\n"
	@$(BUILD_DIR)/$(PROJECT_NAME)$(EXE_EXT)

run_release: build_release
	@printf "Running $(PROJECT_NAME) (release)...\n"
	@$(BUILD_DIR)/$(PROJECT_NAME)$(EXE_EXT)

# Clean up — remove both build directories
clean:
	$(Q)printf "Cleaning build directories...\n"
	$(Q)if [ -d "build" ] || [ -d "$(call FIX_PATH,build)" ]; then \
		$(RMDIR) $(call FIX_PATH,build); \
		fi
	$(Q)if [ -d "build_rel" ] || [ -d "$(call FIX_PATH,build_rel)" ]; then \
		$(RMDIR) $(call FIX_PATH,build_rel); \
		fi

# Static analysis
check:
	$(Q)printf "Running cppcheck on $(SRC_SOURCES)\n"
	$(Q)cppcheck --enable=all --suppress=missingIncludeSystem --force \
										--inconclusive --std=$(CPPCHECK_VERSION) \
										--suppress=staticFunction:src/parse.c \
										--suppress=unusedFunction:src/prelude.h \
										$(SRC_SOURCES)

reset:
	$(Q)printf "Resetting build environment...\n"
	$(MAKE) clean
	$(Q)$(MAKE) build_debug
	$(Q)printf "Build environment reset complete.\n"

# Show configuration
info:
	@printf "Build configuration:\n"
	@printf "  Platform: $(PLATFORM)\n"
	@printf "  Compiler: $(CC)\n"
	@printf "  Build Type: $(BUILD_TYPE)\n"
	@printf "  Build Dir: $(BUILD_DIR)\n"
	@printf "  Verbose: $(VERBOSE)\n"
	@printf "  LDFLAGS: $(LDFLAGS)\n"
	@printf "  LDLIBS: $(LDLIBS)\n"
	@printf "  CFLAGS: $(CFLAGS)\n"
	@printf "\n"
	@printf "  Target: $(TARGET)\n"
	@printf "  Object Dir: $(OBJ_DIR)\n"
	@printf "  Source Dir: $(SRC_DIR)\n"
	@printf "\n"
	@printf "  Sources: $(SRC_SOURCES)\n"
	@printf "\n"
	@printf "  Objects: $(OBJECTS)\n"

# Help target
help:
	@printf "Available targets:\n"
	@printf "  all              - Build the project (default: debug)\n"
	@printf "  build_debug      - Build debug version with symbols   -> build/\n"
	@printf "  build_release    - Build optimized release version    -> build_rel/\n"
	@printf "  clean            - Remove both build directories\n"
	@printf "  check            - Run static analysis with cppcheck\n"
	@printf "  format           - Run clang-format over src/ and test/\n"
	@printf "  info             - Show build configuration\n"
	@printf "  test             - Build and run all test suites\n"
	@printf "  release          - Package release binary into tarball\n"
	@printf "  a                - Build debug + release + run all tests\n"
	@printf "  help             - Show this help\n"
	@printf "\n"
	@printf "Options:\n"
	@printf "  BUILD_TYPE=debug|release  - Set build type\n"
	@printf "  VERBOSE=1                 - Enable verbose output\n"

####################################################################################################
# Testing — single entry point via test_main, routed by suite name arg
#
# Customize TEST_MAIN_SRCS and suite names per-project.
# New suites: add to TestSuite enum in test/test_suites.h, add run_* prototype,
#   add test_*.c file, add it to TEST_MAIN_SRCS below, add shortcut target.
####################################################################################################

TEST_MAIN_BIN = $(BUILD_DIR)/test/test_main
TEST_OBJ_DIR  = $(BUILD_DIR)/test_obj

# Project-specific test sources — list test_*.c files + any src/ files they need
TEST_MAIN_SRCS = \
	test/test_main.c \
	test/test_parse.c \
	test/test_format.c \
	test/test_cli.c \
	test/test_capture.c \
	src/arena.c \
	src/parse.c \
	src/format.c \
	src/cli.c \
	src/capture.c

TEST_OBJECTS = $(patsubst %, $(TEST_OBJ_DIR)/%, $(TEST_MAIN_SRCS:%=%.o))

TEST_CFLAGS = $(STD_FLAG) -g -O0 -pthread -DDEBUG -DTESTING_MAIN_C \
	-Wall -Wextra -Wconversion -Wsign-conversion \
	-fstack-protector-strong \
	-I$(SRC_DIR) \
	-I$(LIBS_DIR) \
	-I$(TEST_DIR) \
	$(addprefix -I,$(shell $(FIND) $(LIBS_DIR) -type d 2>/dev/null))

# Compile test sources into object files (separate obj dir to avoid collision with main build)
$(TEST_OBJ_DIR)/%.c.o: %.c | $(TEST_OBJ_DIR)
	$(Q)printf "Compiling [test] $<\n"
	$(Q)$(MKDIR) $(dir $@)
	$(Q)$(CC) $(TEST_CFLAGS) -MMD -MP -c $< -o $@

$(TEST_OBJ_DIR) $(dir $(TEST_MAIN_BIN)):
	$(Q)$(MKDIR) $(call FIX_PATH,$@)

# Link test binary from objects
$(TEST_MAIN_BIN): $(TEST_OBJECTS)
	$(Q)printf "Linking test runner $@\n"
	$(Q)$(MKDIR) $(dir $@)
	$(Q)$(CC) $(filter-out -MMD -MP, $(TEST_CFLAGS)) -o $@ $^ $(LDLIBS)

# Run all suites — force rebuild of test objects so we always
# test against the latest src/ changes (avoids stale .o issue).
test: $(TEST_MAIN_BIN)
	$(Q)printf "=== Running all tests... ===\n"
	$(Q)$(TEST_MAIN_BIN)

# Run test_main directly
test_main: $(TEST_MAIN_BIN)
	$(Q)printf "=== Running main tests... ===\n"
	$(Q)$(TEST_MAIN_BIN)

info_test:
	@printf "Test configuration:\n"
	@printf "	Test directory: $(TEST_DIR)\n"
	@printf "	Test binary:    $(TEST_MAIN_BIN)\n"

####################################################################################################
# Release packaging
####################################################################################################

DIST_DIR = dist

# Package release binary into a tarball for GH release
release: build_release
	$(Q)printf "Packaging release...\n"
	$(Q)$(MKDIR) $(DIST_DIR)
	$(Q)cp build_rel/$(PROJECT_NAME) $(DIST_DIR)/$(PROJECT_NAME)
	$(Q)OS_LOWER="$${RUNNER_OS:-$$(uname -s)}"; \
	 ARCH_LOWER="$${RUNNER_ARCH:-$$(uname -m)}"; \
	 OS_LOWER="$$(echo "$$OS_LOWER" | tr '[:upper:]' '[:lower:]')"; \
	 ARCH_LOWER="$$(echo "$$ARCH_LOWER" | tr '[:upper:]' '[:lower:]')"; \
	 cd $(DIST_DIR) && tar czf $(PROJECT_NAME)-$${OS_LOWER}-$${ARCH_LOWER}.tar.gz $(PROJECT_NAME)

####################################################################################################
# Convenience shortcuts
####################################################################################################

# b and build are synonymous
b: build_debug

br: build_release

# r and run are synonymous
r: run_debug

rr: run_release

# testing
t: test

test-parse: $(TEST_MAIN_BIN)
	$(Q)$(TEST_MAIN_BIN) parse

test-format: $(TEST_MAIN_BIN)
	$(Q)$(TEST_MAIN_BIN) format

test-cli: $(TEST_MAIN_BIN)
	$(Q)$(TEST_MAIN_BIN) cli

test-capture: $(TEST_MAIN_BIN)
	$(Q)$(TEST_MAIN_BIN) capture

# Launch the real binary with argv. OUT= must be set for dry/write/clobber.
# No-destination launches print to stdout without writing a file.
run-help: build_debug
	$(Q)$(BUILD_DIR)/$(PROJECT_NAME)$(EXE_EXT) --help

run-dry: build_debug
	$(Q)if [ -z "$(OUT)" ]; then printf "run-dry requires OUT=path\n" >&2; exit 2; fi
	$(Q)$(BUILD_DIR)/$(PROJECT_NAME)$(EXE_EXT) --dry-run -o "$(OUT)"

run-write: build_debug
	$(Q)if [ -z "$(OUT)" ]; then printf "run-write requires OUT=path\n" >&2; exit 2; fi
	$(Q)$(BUILD_DIR)/$(PROJECT_NAME)$(EXE_EXT) -t -o "$(OUT)"

run-clobber: build_debug
	$(Q)if [ -z "$(OUT)" ]; then printf "run-clobber requires OUT=path\n" >&2; exit 2; fi
	$(Q)$(BUILD_DIR)/$(PROJECT_NAME)$(EXE_EXT) -o "$(OUT)" < /dev/null

# Already-built launchers for hyperfine. Build first with `make b`, `make br`, `make t`.
# These skip format/rebuild so the measurement is the program, not clang-format.
BENCH_DEBUG = ./build/$(PROJECT_NAME)$(EXE_EXT)
BENCH_REL   = ./build_rel/$(PROJECT_NAME)$(EXE_EXT)
BENCH_TEST  = ./build/test/test_main

bench-debug-help:
	@$(BENCH_DEBUG) --help >/dev/null 2>&1

bench-debug-dry:
	$(Q)if [ -z "$(OUT)" ]; then printf "bench-debug-dry requires OUT=path\n" >&2; exit 2; fi
	@$(BENCH_DEBUG) --dry-run -o "$(OUT)" >/dev/null 2>&1

bench-debug-write:
	$(Q)if [ -z "$(OUT)" ]; then printf "bench-debug-write requires OUT=path\n" >&2; exit 2; fi
	@$(BENCH_DEBUG) -t -o "$(OUT)" >/dev/null 2>&1

bench-rel-help:
	@$(BENCH_REL) --help >/dev/null 2>&1

bench-rel-dry:
	$(Q)if [ -z "$(OUT)" ]; then printf "bench-rel-dry requires OUT=path\n" >&2; exit 2; fi
	@$(BENCH_REL) --dry-run -o "$(OUT)" >/dev/null 2>&1

bench-rel-write:
	$(Q)if [ -z "$(OUT)" ]; then printf "bench-rel-write requires OUT=path\n" >&2; exit 2; fi
	@$(BENCH_REL) -t -o "$(OUT)" >/dev/null 2>&1

bench-test:
	@$(BENCH_TEST) >/dev/null

bench-test-parse:
	@$(BENCH_TEST) parse >/dev/null

bench-test-format:
	@$(BENCH_TEST) format >/dev/null

bench-test-cli:
	@$(BENCH_TEST) cli >/dev/null

bench-test-capture:
	@$(BENCH_TEST) capture >/dev/null

perf:
	$(Q)bash scripts/perf-hyperfine.sh

dump-asm: build_debug
	$(Q)objdump -d $(BUILD_DIR)/$(PROJECT_NAME)$(EXE_EXT)

dump-s: build_debug
	$(Q)$(MKDIR) $(BUILD_DIR)/asm
	$(Q)$(CC) $(CFLAGS) -S -o $(BUILD_DIR)/asm/parse.s src/parse.c
	$(Q)$(CC) $(CFLAGS) -S -o $(BUILD_DIR)/asm/capture.s src/capture.c
	$(Q)$(CC) $(CFLAGS) -S -o $(BUILD_DIR)/asm/format.s src/format.c

cl: clean
c: check
res: reset

h: help

# all: debug + release + tests
a: build_debug build_release test


# i: install
# build run
.PHONY: all \
	build_debug \
	build_release \
	check \
	clean \
	format \
	help \
	info \
	info_test \
	release \
	run_debug \
	run_release \
	test \
	test_main \
	test-parse \
	test-format \
	test-cli \
	test-capture \
	run-help \
	run-dry \
	run-write \
	run-clobber \
	dump-asm \
	dump-s \
	bench-debug-help \
	bench-debug-dry \
	bench-debug-write \
	bench-rel-help \
	bench-rel-dry \
	bench-rel-write \
	bench-test \
	bench-test-parse \
	bench-test-format \
	bench-test-cli \
	bench-test-capture \
	perf \
	b br r rr f cl c res h a

# Include dependency files
-include $(DEPS)
