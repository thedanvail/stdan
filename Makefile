BUILD_ROOT        ?= build
BUILD_TYPE        ?= Debug
STDAN_BUILD_TESTS ?= OFF
BUILD_DIR         ?= $(BUILD_ROOT)/$(BUILD_TYPE)-tests-$(STDAN_BUILD_TESTS)
JOBS              ?= $(shell nproc 2>/dev/null || echo 4)
CMAKE_ARGS        ?=
CLANG_TIDY        ?= clang-tidy

.PHONY: all configure build release test format cc tidy cppcheck clean

all: build

configure:
	cmake -S . -B "$(BUILD_DIR)" $(CMAKE_ARGS) \
		"-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)" "-DSTDAN_BUILD_TESTS=$(STDAN_BUILD_TESTS)"

build: configure
	cmake --build "$(BUILD_DIR)" --parallel "$(JOBS)"

release:
	$(MAKE) build BUILD_TYPE=Release

test: STDAN_BUILD_TESTS := ON
test: configure
	cmake --build "$(BUILD_DIR)" --target stdan_tests --parallel "$(JOBS)"
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

format:
	find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
		-exec clang-format -i {} +

cc: STDAN_BUILD_TESTS := ON
cc: configure
	ln -sf "$(BUILD_DIR)/compile_commands.json" compile_commands.json

tidy: cc
	jq -r '.[] | .file | select(contains("/vendor/") | not) | select(test("\\.(cpp|cc|cxx)$$"))' compile_commands.json \
		| sort -u \
		| xargs $(CLANG_TIDY) -p . --checks='-*,clang-analyzer-*' --warnings-as-errors=*

cppcheck:
	cppcheck --enable=all --std=c++23 -I include \
		--suppress=missingInclude --suppress=missingIncludeSystem --suppress=unusedFunction \
		--suppress=unmatchedSuppression --suppress=normalCheckLevelMaxBranches \
		--error-exitcode=1 -i vendor -i "$(BUILD_DIR)" src include

clean:
	rm -rf "$(BUILD_ROOT)" "$(BUILD_DIR)" compile_commands.json
