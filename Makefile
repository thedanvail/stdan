BUILD_DIR         := build
BUILD_TYPE        ?= Debug
STDAN_BUILD_TESTS ?= OFF
JOBS              ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all configure build release test format clean compile_commands cc

all: build

# --- Configure (generates build system + compile_commands.json) ---

configure:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSTDAN_BUILD_TESTS=$(STDAN_BUILD_TESTS)
	@ln -sf $(BUILD_DIR)/compile_commands.json compile_commands.json

# --- Build (auto-configures if needed) ---

build: | configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

# --- Release build ---

release:
	$(MAKE) build BUILD_TYPE=Release

# --- Tests ---

test:
	$(MAKE) configure BUILD_TYPE=$(BUILD_TYPE) STDAN_BUILD_TESTS=ON
	cmake --build $(BUILD_DIR) --target stdan_tests -j$(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

# --- Format ---

format:
	find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | xargs clang-format -i

fmt: format

# --- compile_commands.json shortcut ---

cc: configure
	@echo "compile_commands.json symlinked to project root"

# --- Clean ---

clean:
	rm -rf $(BUILD_DIR)
	rm -f compile_commands.json
