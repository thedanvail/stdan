# Contributing

## Environment setup

Use a C++23-capable compiler, CMake 3.20 or newer, and `make` on
Unix-like systems. Initialize the vendored dependencies before configuring the
project:

```sh
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSTDAN_BUILD_TESTS=ON
```

See the platform-specific commands in the repository README when building with
Visual Studio.

## Validation

Add or update Catch2 tests for behavior changes. Tests should use BDD-style
`SCENARIO`, `GIVEN`, `WHEN`, and `THEN` sections and cover failure paths where
relevant. Before submitting a change, run:

```sh
make test
```

All tests must pass. Keep changes compatible with C++23 and avoid unrelated
formatting or cleanup.

## Submissions

Submit a focused pull request that explains the problem, the chosen approach,
and any performance or compatibility implications. Include the validation
commands and results in the description. Keep commits reviewable, update
documentation for public API changes, and respond to review feedback with
additional tests when appropriate.
