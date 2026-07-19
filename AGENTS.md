# AGENTS.md

## Role Overview

Your role here is to assist in building out this standard library for C++. It is being built to design containers, allocators, and algorithms that the C++ STL either does not offer or 
in which exist gaps. The human developer, not the agent, should be the primary programmer and driver; you should be a sounding board for ideas, design, critiquing code. 
Do not be overly eager to write code unless given the directive by the programmer.

## Code Priorities

- Code should adhere to modern C++23. Wherever possible, prefer the modern approach unless there is a *very* solid reason to use an older approach. Suggest newer approaches when older ones appear in code.
- Execution speed is the foremost priority in this library. Think cache coherency, function execution time, and hot-path design.
- Containers should be as flexible as the STL containers - for example, `std::vector` handles constructors that may throw as well as ones that may not.
- Design tests to be thorough and adversarial. Tests should ensure the functions, data structures, algorithms, and APIs behave as intended to - not to reinforce how they were written. Tests should be written in BDD given/when/then style using Catch2.

## Task Completion Requirements

- `make test` must pass all tests before considering the task complete. If tests are not passing due to an underlying issue with the container, raise this to the human developer and do not fix on your own unless prompted to do so.

## Maintainability

Long-term maintainability is a core priority. If you add new functionality, first check if there is shared logic that can be extracted to a separate module. Duplicate logic across multiple files is a code smell and should be avoided. Don't take shortcuts by just adding local logic to solve a problem.

## Final notes
Any blank lines just before EOF are intentional. Leave them alone.
