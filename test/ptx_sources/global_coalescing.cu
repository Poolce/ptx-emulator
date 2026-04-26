// global_coalescing.cu
//
// Demonstrates and verifies global memory coalescing behaviour.
//
// Two kernels reduce a ROWS×COLS float matrix; both are numerically correct
// but differ only in how they access global memory:
//
//   kernel_row_reduce  (coalesced)
//     Block b sums row b.  Thread tid reads A[b*COLS + tid] — 32 consecutive
//     floats land in a single 128-byte L1 cache line.
//     Expected profiling: global_mem_transactions=1, global_coalescing=1.0
//
//   kernel_col_reduce  (strided / uncoalesced)
//     Block b sums column b.  Thread tid reads A[tid*COLS + b] — stride of
//     COLS floats (128 bytes) places each thread's datum in a different cache
//     line, so the hardware issues one transaction per active thread.
//     Expected profiling: global_mem_transactions=32, global_coalescing~0.03
//
// Both results are verified against independent CPU references.

#include "cuemu_io.h"

#include <cuda_runtime.h>

#include <cassert>
#include <cmath>
#include <cstdio>

#define CHECK(x) assert((x) == cudaSuccess)

static constexpr int ROWS = 32;
static constexpr int COLS = 32; // must equal warp size for the coalescing story to be clean

// ---------------------------------------------------------------------------
// Coalesced: each block reduces one full row.
// The single ld.global per thread reads a 32-float stride-1 run → 1 cache line.
// ---------------------------------------------------------------------------
__global__ void kernel_row_reduce(const float* __restrict__ A, float* __restrict__ row_sums)
{
    __shared__ float smem[COLS];
    const int tid = threadIdx.x;
    const int row = blockIdx.x;

    smem[tid] = A[row * COLS + tid]; // coalesced global read
    __syncthreads();

    for (int s = COLS / 2; s > 0; s >>= 1)
    {
        if (tid < s)
            smem[tid] += smem[tid + s];
        __syncthreads();
    }

    if (tid == 0)
        row_sums[row] = smem[0];
}

// ---------------------------------------------------------------------------
// Strided: each block reduces one full column.
// The single ld.global per thread reads elements spaced COLS floats apart
// (128 bytes = one cache line per element) → 32 distinct cache lines.
// ---------------------------------------------------------------------------
__global__ void kernel_col_reduce(const float* __restrict__ A, float* __restrict__ col_sums)
{
    __shared__ float smem[ROWS];
    const int tid = threadIdx.x;
    const int col = blockIdx.x;

    smem[tid] = A[tid * COLS + col]; // strided global read
    __syncthreads();

    for (int s = ROWS / 2; s > 0; s >>= 1)
    {
        if (tid < s)
            smem[tid] += smem[tid + s];
        __syncthreads();
    }

    if (tid == 0)
        col_sums[col] = smem[0];
}

// ---------------------------------------------------------------------------
// CPU references
// ---------------------------------------------------------------------------
static void cpu_row_reduce(const float* A, float* out)
{
    for (int r = 0; r < ROWS; ++r)
    {
        float s = 0.f;
        for (int c = 0; c < COLS; ++c)
            s += A[r * COLS + c];
        out[r] = s;
    }
}

static void cpu_col_reduce(const float* A, float* out)
{
    for (int c = 0; c < COLS; ++c)
    {
        float s = 0.f;
        for (int r = 0; r < ROWS; ++r)
            s += A[r * COLS + c];
        out[c] = s;
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    static constexpr int N = ROWS * COLS;

    float h_A[N];
    CuemuIo::generate<float>("A", h_A, N, [](size_t k) { return static_cast<float>(k % 7) - 3.f; });

    float h_ref_row[ROWS], h_ref_col[COLS];
    cpu_row_reduce(h_A, h_ref_row);
    cpu_col_reduce(h_A, h_ref_col);

    float *d_A, *d_out;
    CHECK(cudaMalloc(&d_A, N * sizeof(float)));
    CHECK(cudaMalloc(&d_out, ROWS * sizeof(float))); // ROWS == COLS

    CHECK(cudaMemcpy(d_A, h_A, N * sizeof(float), cudaMemcpyHostToDevice));

    // --- coalesced run ---
    float h_row[ROWS];
    kernel_row_reduce<<<ROWS, COLS>>>(d_A, d_out);
    CHECK(cudaDeviceSynchronize());
    CHECK(cudaMemcpy(h_row, d_out, ROWS * sizeof(float), cudaMemcpyDeviceToHost));

    // --- strided run ---
    float h_col[COLS];
    kernel_col_reduce<<<COLS, ROWS>>>(d_A, d_out);
    CHECK(cudaDeviceSynchronize());
    CHECK(cudaMemcpy(h_col, d_out, COLS * sizeof(float), cudaMemcpyDeviceToHost));

    CHECK(cudaFree(d_A));
    CHECK(cudaFree(d_out));

    bool ok = true;
    for (int i = 0; i < ROWS; ++i)
    {
        if (std::fabs(h_row[i] - h_ref_row[i]) > 1e-3f)
        {
            std::printf("FAIL row_reduce[%d]: got %.4f expected %.4f\n", i, h_row[i], h_ref_row[i]);
            ok = false;
        }
    }
    for (int i = 0; i < COLS; ++i)
    {
        if (std::fabs(h_col[i] - h_ref_col[i]) > 1e-3f)
        {
            std::printf("FAIL col_reduce[%d]: got %.4f expected %.4f\n", i, h_col[i], h_ref_col[i]);
            ok = false;
        }
    }

    if (ok)
        std::puts("SUCCESS");
    return ok ? 0 : 1;
}
