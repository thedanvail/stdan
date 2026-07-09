BUILD_DIR         := build
BUILD_TYPE        ?= Debug
STDAN_BUILD_TESTS ?= OFF
CC                ?= clang
CXX               ?= clang++
JOBS              ?= $(shell nproc 2>/dev/null || echo 4)
CLANG_TIDY        ?= clang-tidy
CPPCHECK          ?= cppcheck
JQ                ?= jq
CMAKE_ARGS        ?=
CONFIGURE_ARGS    := -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DSTDAN_BUILD_TESTS=$(STDAN_BUILD_TESTS) $(CMAKE_ARGS)

.PHONY: all configure build release test format clean compile_commands cc tidy cppcheck ci


all: build

configure:
	if printf '%s\n' "$(notdir $(CXX))" | grep -Eq '^clang\+\+'; then \
		CC="$(CC)" CXX="$(CXX)" cmake -B $(BUILD_DIR) $(CONFIGURE_ARGS) -DSTDAN_USE_LIBCXX=ON; \
	else \
		CC="$(CC)" CXX="$(CXX)" cmake -B $(BUILD_DIR) $(CONFIGURE_ARGS); \
	fi
	@ln -sf $(BUILD_DIR)/compile_commands.json compile_commands.json

build: | configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

release:
	$(MAKE) build BUILD_TYPE=Release

test:
	$(MAKE) configure STDAN_BUILD_TESTS=ON
	cmake --build $(BUILD_DIR) --target stdan_tests -j$(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

format:
	find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | xargs clang-format -i

fmt: format

cc:
	$(MAKE) configure STDAN_BUILD_TESTS=ON
	@echo "compile_commands.json symlinked to project root"

tidy: cc
	$(JQ) -r '.[] | .file | select(contains("/vendor/") | not) | select(test("\\.(cpp|cc|cxx)$$"))' compile_commands.json \
		| sort -u \
		| xargs -r $(CLANG_TIDY) -p . --checks='-*,clang-analyzer-*' --warnings-as-errors=*

cppcheck:
	$(CPPCHECK) --enable=all --suppress=missingInclude --suppress=missingIncludeSystem --suppress=unusedFunction --error-exitcode=1 -i vendor -i build src include

ci:
	$(MAKE) clean
	$(MAKE) cc
	$(MAKE) tidy
	$(MAKE) cppcheck
	$(MAKE) test

clean:
	rm -rf $(BUILD_DIR) compile_commands.json
