# SolarNet

A modern C++20 Linux network framework inspired by muduo, rebuilt from the ground up for long-term maintenance and high performance.

## Status

Phase 0: Engineering infrastructure is ready. The project can be configured, built, tested, and benchmarked.

## Build Requirements

- CMake >= 3.20
- C++20 compiler (GCC >= 11 or Clang >= 14)
- Linux environment (WSL / Ubuntu VM / native)
- Optional: `clang-tidy`, `ninja-build`
- For tests/benchmarks (system packages, recommended):

```bash
sudo apt install libgtest-dev libbenchmark-dev
```

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Run Tests

```bash
ctest --test-dir build --output-on-failure
```

## Run Benchmarks

```bash
./build/benchmarks/bench_version
```

## Run Example

```bash
./build/examples/example_version
```

## Development Options

| CMake Option | Default | Description |
| --- | --- | --- |
| `SOLAR_NET_BUILD_TESTS` | ON | Build unit tests |
| `SOLAR_NET_BUILD_BENCHMARKS` | ON | Build microbenchmarks |
| `SOLAR_NET_BUILD_EXAMPLES` | ON | Build example programs |
| `SOLAR_NET_BUILD_SHARED` | OFF | Build shared library instead of static |
| `SOLAR_NET_USE_CLANG_TIDY` | ON | Run clang-tidy during build |

## Project Layout

- `solar_net/` — public headers and implementation files co-located (one module per file pair).
- `tests/` — GoogleTest unit tests.
- `benchmarks/` — Google Benchmark microbenchmarks.
- `examples/` — example programs.

- `.clang-format` for formatting
- `.clang-tidy` for static analysis
- All code must compile cleanly with the enabled warnings.

## License

TBD
