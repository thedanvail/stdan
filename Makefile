BUILD_DIR  ?= build
BUILD_TYPE ?= Debug
JOBS       ?= $(shell nproc 2>/dev/null || echo 4)
CMAKE_ARGS ?=
CLANG_TIDY ?= clang-tidy

.PHONY: all build release test format cc tidy cppcheck clean

all: build

build:
	cmake -S . -B "$(BUILD_DIR)" "-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)" \
		-DSTDAN_BUILD_TESTS=OFF $(CMAKE_ARGS)
	cmake --build "$(BUILD_DIR)" --parallel "$(JOBS)"

release:
	$(MAKE) build BUILD_TYPE=Release

test:
	cmake -S . -B "$(BUILD_DIR)" "-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)" \
		-DSTDAN_BUILD_TESTS=ON $(CMAKE_ARGS)
	cmake --build "$(BUILD_DIR)" --target stdan_tests --parallel "$(JOBS)"
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

format:
	find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
		-exec clang-format -i {} +

cc:
	cmake -S . -B "$(BUILD_DIR)" "-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)" \
		-DSTDAN_BUILD_TESTS=ON $(CMAKE_ARGS)
	ln -sf "$(BUILD_DIR)/compile_commands.json" compile_commands.json

tidy: cc
	jq -r '.[] | .file | select(contains("/vendor/") | not) | select(test("\\.(cpp|cc|cxx)$$"))' compile_commands.json \
		| sort -u \
		| xargs -r $(CLANG_TIDY) -p . --checks='-*,clang-analyzer-*' --warnings-as-errors=*

cppcheck:
	cppcheck --enable=all --suppress=missingInclude --suppress=missingIncludeSystem \
		--suppress=unusedFunction --error-exitcode=1 -i vendor -i "$(BUILD_DIR)" src include

clean:
	rm -rf "$(BUILD_DIR)" compile_commands.json
