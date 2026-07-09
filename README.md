# stdan

`stdan` is a small modern C++ project for experimenting with and implementing data structures, algorithms, and related supporting utilities.

The repository is organized to grow over time. New implementations, tests, and supporting components can be added without changing the overall project structure or build flow.

## Layout

- `include/` public headers
- `src/` library source files
- `tests/` test targets and test sources
- `vendor/` third-party dependencies

## Requirements

- CMake 3.16 or newer
- A C++23-capable compiler
- `make` for the provided Makefile workflow (windows people can deal - they're on windows after all)

## Build

### With CMake

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### With Make

```sh
make build
```

## Tests

Tests are optional and are enabled with `STDAN_BUILD_TESTS=ON`.

### With CMake

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSTDAN_BUILD_TESTS=ON
cmake --build build --target stdan_tests
ctest --test-dir build --output-on-failure
```

### With Make

```sh
make test
```

If the test dependency is not present yet, initialize submodules first:

```sh
git submodule update --init --recursive
```

## Notes

- The main library target is `stdan`.
- `compile_commands.json` can be generated as part of the configured build.
- The project currently uses a static library build.

