# PrimeHost

PrimeHost is a C++23 cross-platform OS integration layer that provides surfaces, input, timing, and presentation for PrimeManifest and PrimeFrame.

## Building

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

Optional configuration:
- `-DPRIMEHOST_BUILD_TESTS=ON/OFF` toggles tests.
- `-DPRIMEHOST_BUILD_EXAMPLES=ON/OFF` toggles example binaries.

## Tests

From a build dir:

```sh
ctest --output-on-failure
```

Or run the test binary directly:

```sh
./PrimeHost_tests
```

## Docs
- `docs/primehost-plan.md`
- `docs/presentation-config.md`
