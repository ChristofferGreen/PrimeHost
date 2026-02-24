# AGENTS.md for PrimeHost

## Scope
Defines naming and coding rules plus build/test entrypoints for contributors working in this repo.

## Naming rules
- **Namespaces:** PascalCase for public namespaces (e.g., `PrimeHost`).
- **Types/structs/classes/enums/aliases:** PascalCase.
- **Functions (free/member/static):** lowerCamelCase.
- **Struct/class fields:** lowerCamelCase.
- **Local/file-only helpers:** lower_snake_case is acceptable when marked `static` in a `.cpp`.
- **Constants (`constexpr`/`static`):** PascalCase; avoid `k`-prefixes.
- **Macros:** avoid new ones; if unavoidable, use `PH_` prefix and ALL_CAPS.

## Build/test workflow
- Debug: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build-debug`.
- Release: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release` then `cmake --build build-release`.
- Options: `-DPRIMEHOST_BUILD_TESTS=ON/OFF`, `-DPRIMEHOST_BUILD_EXAMPLES=ON/OFF`.
- Tests: from a build dir, run `ctest --output-on-failure` or execute `./PrimeHost_tests` directly.

## Tests
- Unit tests live in `tests/unit` and use `PH_TEST(group, name)` plus `PH_CHECK`.
- Keep test groups under 100 cases (enforced in `tests/unit/test_main.cpp`).

## Code guidelines
- Target C++23; prefer value semantics, RAII, `std::span`, and `std::optional`/`std::expected`.
- Use `std::chrono` types for durations/timeouts.
- Keep hot paths allocation-free; reuse buffers and caches where possible.
- Avoid raw `new`; use `std::unique_ptr` or `std::shared_ptr` with clear ownership intent.

## Collaboration
- When you make commits or change existing commits, always report the repo(s) and commit hashes in your response.

## Workflow
- For UI regressions: reproduce in the demo first, implement the fix, then verify the behavior (and add a test if the issue is testable).

## Dependencies
- Only change FetchContent repository pins or source directory overrides when required, and always report exactly what changed.
