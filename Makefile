BUILD_DIR  := build
BUILD_TYPE ?= Debug
JOBS       ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all configure build release test format clean compile_commands

all: build

# --- Configure (generates build system + compile_commands.json) ---

configure:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@ln -sf $(BUILD_DIR)/compile_commands.json compile_commands.json

# --- Build (auto-configures if needed) ---

build: | configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

# --- Release build ---

release:
	$(MAKE) build BUILD_TYPE=Release

# --- Tests ---

test: build
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
