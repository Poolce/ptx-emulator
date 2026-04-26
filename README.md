# PTX Emulator

A CPU-side emulator for CUDA kernels that intercepts the CUDA runtime via `LD_PRELOAD` and executes PTX assembly on CPU threads. The primary use cases are correctness testing without GPU hardware and detailed per-instruction profiling (branch efficiency, shared-memory bank conflicts, global-memory coalescing, register usage).

---

## Table of Contents

- [How It Works](#how-it-works)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Running a CUDA Binary](#running-a-cuda-binary)
  - [CLI Reference](#cli-reference)
  - [GPU Architecture Config](#gpu-architecture-config)
- [Profiling](#profiling)
  - [Collecting Profiling Data](#collecting-profiling-data)
  - [Generating an HTML Report](#generating-an-html-report)
  - [Report Metrics](#report-metrics)
- [Integration Tests](#integration-tests)
  - [CuemuIo — Controllable Input Generation](#cuemuio--controllable-input-generation)
- [Unit Tests](#unit-tests)
- [Project Layout](#project-layout)
- [Architecture Overview](#architecture-overview)

---

## How It Works

```
┌─────────────────────────────────────────────────────────────┐
│  CUDA binary (compiled with nvcc, contains embedded PTX)    │
└────────────────────────┬────────────────────────────────────┘
                         │  LD_PRELOAD=libemuruntime.so
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  libemuruntime.so  — CUDA runtime shim                      │
│  • cudaMalloc / cudaFree / cudaMemcpy                       │
│  • cudaLaunchKernel → extracts embedded PTX, parses it,     │
│    and runs each block on CPU threads                        │
└────────────────────────┬────────────────────────────────────┘
                         │
          ┌──────────────▼──────────────┐
          │  PTX Parser (app/ptx_asm/)  │
          │  Parses PTX text into an    │
          │  instruction list with PCs  │
          └──────────────┬──────────────┘
                         │
          ┌──────────────▼──────────────┐
          │  Execution Engine           │
          │  GlobalContext              │
          │    └─ BlockContext (×grid)  │
          │         ├─ SharedMemory     │
          │         ├─ BlockBarrier     │
          │         └─ WarpContext (×⌈threads/32⌉)  │
          │              ├─ PDOM divergence stack    │
          │              ├─ Register files           │
          │              └─ Profiling buffer         │
          └─────────────────────────────┘
```

The `cuemu` binary is a thin launcher that sets the required environment variables and re-executes the target binary under `LD_PRELOAD`. No source-code changes to the CUDA program are needed.

---

## Prerequisites

| Dependency | Purpose |
|---|---|
| CMake ≥ 3.20 | Build system |
| C++23 compiler (GCC 13+ / Clang 16+) | Core emulator |
| NVCC (CUDA toolkit) | Compile integration test `.cu` sources |
| Python ≥ 3.10 | `ptx_report` HTML generator |
| `cuobjdump` (CUDA toolkit) | PTX extraction for the report tool |
| `c++filt` | Kernel name demangling in reports |

The emulator itself does **not** require a GPU or GPU driver at runtime.

---

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Outputs:

| Path | Description |
|---|---|
| `build/bin/cuemu` | Emulator launcher CLI |
| `build/lib/libemuruntime.so` | CUDA runtime shim (injected via LD_PRELOAD) |
| `build/bin/unit_tests` | GoogleTest unit-test suite |
| `build/bin/<test_name>` | Integration test binaries |

---

## Running a CUDA Binary

Compile your CUDA program normally with `nvcc`, then run it under the emulator:

```bash
nvcc -o my_app my_app.cu
./build/bin/cuemu my_app
```

For source-level profiling annotation, compile with `-lineinfo`:

```bash
nvcc -lineinfo -o my_app my_app.cu
```

### CLI Reference

```
Usage: cuemu [OPTIONS] <binary>

Options:
  --config <path>              GPU architecture config file (TOML)
  --collect-profiling          Enable per-instruction metric collection
  --profiling-output <path>    Output file for profiling data (default: profiling.txt)
  -l, --log-level <level>      Log verbosity: DEBUG, INFO, WARNING, ERROR (default: INFO)
  -h, --help                   Show this help and exit

Examples:
  cuemu ./my_cuda_app
  cuemu --config configs/v100.toml ./my_cuda_app
  cuemu --collect-profiling --profiling-output prof.txt ./my_cuda_app
  cuemu -l DEBUG ./my_cuda_app
```

### GPU Architecture Config

Three presets are provided under `configs/`:

| File | Architecture | Warp size | Shared mem | Banks |
|---|---|---|---|---|
| `configs/a100.toml` | NVIDIA A100 (sm_80) | 32 | 48 KB | 32 × 4 B |
| `configs/h800.toml` | NVIDIA H800 (sm_90) | 32 | 48 KB | 32 × 4 B |
| `configs/v100.toml` | NVIDIA V100 (sm_70) | 32 | 48 KB | 32 × 4 B |

Example TOML structure:

```toml
[gpu]
name               = "NVIDIA A100 (Ampere sm_80)"
compute_capability = "8.0"

[warp]
size = 32

[shared_memory]
total_size = 49152
bank_count = 32
bank_width = 4
bank_groups = 4

[limits]
max_threads_per_block = 1024
max_warps_per_sm      = 64
max_blocks_per_sm     = 32
registers_per_thread  = 255
```

---

## Profiling

### Collecting Profiling Data

```bash
cuemu --collect-profiling --profiling-output profiling.txt ./my_cuda_app
```

This produces a text log with one record per warp instruction issue, including the execution mask (active thread bitmap), PC, block/warp IDs, and per-instruction metrics.

### Generating an HTML Report

The `tools/ptx_report` Python package turns the profiling log into a self-contained HTML report:

```bash
# Option A — pass a pre-collected profiling file and the binary (for PTX extraction):
python -m tools.ptx_report profiling.txt --binary ./my_cuda_app -o report.html

# Option B — let the tool run cuemu automatically:
python -m tools.ptx_report --binary ./my_cuda_app -o report.html

# Option C — supply a raw PTX file instead of a binary:
python -m tools.ptx_report profiling.txt --ptx my_app.ptx --source my_app.cu -o report.html
```

Full option reference:

```
positional:
  PROFILING_FILE        Pre-existing cuemu --collect-profiling dump.
                        If omitted, cuemu is invoked automatically.

options:
  --binary FILE         CUDA binary to profile / extract PTX from.
                        Repeat for multiple binaries.
  --ptx FILE            PTX file with optional .loc lineinfo.
                        Used only when --binary is not given.
  --source FILE         CUDA source file (.cu) for annotation.
                        If omitted, paths from PTX .file directives are tried.
  --cuemu PATH          Path to the cuemu executable (default: auto-detected).
  --config FILE         GPU architecture TOML (default: built-in Ampere sm_80).
  -l, --log-level       Emulator log verbosity: DEBUG, INFO, WARNING, ERROR.
  -o, --output FILE     Output HTML file (default: report.html).
  --title TEXT          Report title string.
```

### Report Metrics

Each instruction row in the report shows:

| Column | Description |
|---|---|
| PC | Program counter (sequential integer assigned by the PTX parser) |
| PTX | Raw PTX instruction text |
| Source | Source file:line (requires `-lineinfo` at compile time) |
| Exec Count | Total active-thread executions across all warps and blocks |
| Avg Branch Eff. | Average fraction of threads active per warp issue (1.0 = no divergence) |
| Bank Conflicts | Total shared-memory bank conflicts accumulated |
| Mem Txns | Total global-memory cache-line transactions |
| Avg Coalescing | Ratio of ideal to actual transactions (1.0 = perfectly coalesced) |

The per-kernel summary panel also shows:

- **Register Allocation** — declared register count per type from the PTX `.reg` directives, plus total write-operation counts derived from profiling records.
- **Hotspot tables** — top instructions by execution count, branch divergence, bank conflicts, and global memory inefficiency.
- **Source view** — annotated source lines when `.loc` debug info is present.

---

## Integration Tests

Ten CUDA programs under `test/ptx_sources/` serve as end-to-end correctness tests. Each computes a GPU result and validates it against a CPU reference.

| Test binary | Algorithm | What it exercises |
|---|---|---|
| `vadd_custom` | Element-wise vector addition | Basic global loads/stores |
| `gelu_custom` | GELU activation (tanh approximation) | Math intrinsics, elementwise |
| `softmax_custom` | Online softmax (max + sum + normalize) | Shared memory, reductions |
| `mmul_custom` | Tiled matrix multiply | Shared memory, 2-D indexing |
| `attention_custom` | Flash-attention style fused kernel | Shared memory, complex control flow |
| `fft_custom` | Radix-2 Cooley-Tukey DIT FFT (N=32) | Shared memory, butterfly permutation |
| `conjugate_gradient` | Conjugate gradient linear solver | Shared memory, iterative reductions |
| `bank_conflicts` | Shared memory with intentional stride | Bank conflict detection |
| `branch_divergence` | Warp-level conditional branches | PDOM stack, branch efficiency |
| `global_coalescing` | Strided global memory access | Coalescing analysis |

Run all integration tests after building:

```bash
ctest --test-dir build --output-on-failure
```

Run a single test directly:

```bash
./build/bin/fft_custom
```

### CuemuIo — Controllable Input Generation

Integration tests that use `cuemu_io.h` support environment-variable overrides for their input arrays, making it easy to test specific data patterns without recompiling:

```bash
# Override the "src_r" array with zeros:
CUEMU_GEN_SRC_R=zeros ./build/bin/fft_custom

# Use a uniform random distribution seeded at 42:
CUEMU_GEN_SRC_R="uniform:lo=-1.0:hi=1.0:seed=42" ./build/bin/fft_custom

# Use a sequential ramp starting at 0 with step 0.01:
CUEMU_GEN_SRC_R="sequential:start=0:step=0.01" ./build/bin/fft_custom
```

Available generation modes:

| Mode | Parameters | Description |
|---|---|---|
| `default` | — | Use the test's own default generator |
| `zeros` | — | All elements set to 0 |
| `ones` | — | All elements set to 1 |
| `constant` | `value=<v>` | All elements set to `v` |
| `sequential` | `start=<s>`, `step=<d>` | `s, s+d, s+2d, …` |
| `uniform` | `lo=<l>`, `hi=<h>`, `seed=<n>` | Uniform random in `[l, h)` |
| `normal` | `mean=<m>`, `std=<s>`, `seed=<n>` | Normal distribution |

The environment variable name is `CUEMU_GEN_<ARRAY_NAME_UPPER>`.

---

## Unit Tests

GoogleTest-based unit tests live under `test/unit/`:

| File | Covers |
|---|---|
| `test_ptx_asm.cpp` | PTX parsing and basic instruction execution |
| `test_executors.cpp` | Individual instruction executors |
| `test_barrier.cpp` | `BlockBarrier` warp synchronisation |
| `test_coalescing.cpp` | Global-memory coalescing metric computation |

Run:

```bash
./build/bin/unit_tests
# or via CTest:
ctest --test-dir build -R unit
```

---

## Project Layout

```
ptx-emulator/
├── app/
│   ├── context/
│   │   ├── global_context.{h,cpp}   # Top-level execution state for a kernel launch
│   │   ├── block_context.{h,cpp}    # Per-block state: shared memory, barrier
│   │   ├── warp_context.{h,cpp}     # Per-warp state: registers, PC, divergence stack
│   │   └── profiler.{h,cpp}         # Per-instruction metric recorder
│   ├── ptx_asm/
│   │   ├── module.{h,cpp}           # PTX module: parses text → instruction list
│   │   └── function.{h,cpp}         # PTX function and instruction executors
│   ├── runtime/
│   │   ├── runtime_def.{h,cpp}      # cudaMalloc / cudaFree / cudaMemcpy shims
│   │   ├── rt_stream.{h,cpp}        # Kernel launch scheduling and warp execution loop
│   │   └── rt_interface.{h,cpp}     # Top-level CUDA runtime interface
│   ├── utils/
│   │   ├── gpu_config.{h,cpp}       # TOML config loader (warp size, shared mem, …)
│   │   ├── logger.h                 # Lightweight logging macros
│   │   └── types.h                  # dim3 and other shared types
│   └── emulator/
│       └── main.cpp                 # cuemu launcher CLI
├── configs/
│   ├── a100.toml                    # NVIDIA A100 (sm_80) preset
│   ├── h800.toml                    # NVIDIA H800 (sm_90) preset
│   └── v100.toml                    # NVIDIA V100 (sm_70) preset
├── test/
│   ├── ptx_sources/                 # Integration test CUDA programs
│   │   ├── cuemu_io.h               # Controllable input generation helper
│   │   └── *.cu
│   └── unit/                        # GoogleTest unit tests
├── tools/
│   └── ptx_report/                  # Python HTML report generator
│       ├── __main__.py              # CLI entry point
│       ├── parser.py                # Parse cuemu profiling text → records
│       ├── ptx_parser.py            # Parse PTX → PC map, .loc, .reg declarations
│       ├── aggregator.py            # Aggregate records into per-function stats
│       └── renderer.py              # Render stats → self-contained HTML
├── 3rdparty/
│   └── googletest/
└── CMakeLists.txt
```

---

## Architecture Overview

### Execution Model

The emulator models the CUDA SIMT execution model at warp granularity:

- Each **warp** (32 threads) is a `WarpContext` with its own register file, program counter, and a **PDOM divergence stack** for structured branch reconvergence.
- Each **block** is a `BlockContext` owning a flat `std::vector<uint8_t>` shared-memory arena and a `BlockBarrier` for `__syncthreads()` emulation.
- Multiple warps within a block are executed cooperatively on a single CPU thread (round-robin). An optional `#ifdef` path runs warps in parallel CPU threads.

### Branch Divergence

When a conditional branch splits a warp, the emulator pushes an IPDOM reconvergence frame and a diverged-path frame onto the `execution_stack`. Execution alternates between diverged paths until the convergence PC is reached, at which point the full mask is restored.

### Shared Memory

Shared memory is allocated from a per-block arena. `RegisterSharedSymbol` stores a `size_t` offset into the arena; `GetSharedPtr` computes `data() + offset` on each access, so arena reallocations never leave dangling pointers.

Bank conflict detection counts how many distinct banks a warp-level shared-memory access touches, relative to the number of unique cache lines it would ideally require given the GPU config.

### Global Memory Coalescing

The profiler computes coalescing as `ideal_transactions / actual_transactions`, where:
- **actual** = number of distinct 128-byte cache lines touched by active threads
- **ideal** = `ceil(active_bytes / 128)` for the access size

### Profiling Pipeline

```
cuemu --collect-profiling      →  profiling.txt   (one record per warp issue)
cuobjdump -ptx <binary>        →  embedded PTX    (PC map + .loc source info)
python -m tools.ptx_report     →  report.html     (self-contained, no JS deps)
```
