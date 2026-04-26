#include "cuemu_io.h"

#include <cuda_runtime.h>

#include <cassert>
#include <cmath>
#include <cstdio>

#define CHECK(x) assert((x) == cudaSuccess)

static constexpr int TILE = 32;

__global__ void row_reduce_conflicted(const float* __restrict__ in, float* __restrict__ out)
{
    __shared__ float smem[TILE * TILE];

    int tid = threadIdx.x;

    for (int i = 0; i < TILE; ++i)
        smem[tid * TILE + i] = in[tid * TILE + i];
    __syncthreads();

    float acc = 0.f;
    for (int i = 0; i < TILE; ++i)
        acc += smem[tid * TILE + i];

    out[tid] = acc;
}

__global__ void row_reduce_free(const float* __restrict__ in, float* __restrict__ out)
{
    __shared__ float smem_pad[TILE * (TILE + 1)];

    int tid = threadIdx.x;

    for (int i = 0; i < TILE; ++i)
        smem_pad[tid * (TILE + 1) + i] = in[tid * TILE + i];
    __syncthreads();

    float acc = 0.f;
    for (int i = 0; i < TILE; ++i)
        acc += smem_pad[tid * (TILE + 1) + i];

    out[tid] = acc;
}

static void cpu_row_reduce(const float* in, float* out)
{
    for (int t = 0; t < TILE; ++t)
    {
        float s = 0.f;
        for (int i = 0; i < TILE; ++i)
            s += in[t * TILE + i];
        out[t] = s;
    }
}

int main()
{
    static constexpr int N = TILE * TILE;

    float h_in[N];
    CuemuIo::generate<float>("in", h_in, N, [](size_t i) { return static_cast<float>(i % 7) - 3.f; });

    float h_ref[TILE];
    cpu_row_reduce(h_in, h_ref);

    float *d_in, *d_out;
    CHECK(cudaMalloc(&d_in, N * sizeof(float)));
    CHECK(cudaMalloc(&d_out, TILE * sizeof(float)));
    CHECK(cudaMemcpy(d_in, h_in, N * sizeof(float), cudaMemcpyHostToDevice));

    float h_out_c[TILE], h_out_f[TILE];

    row_reduce_conflicted<<<1, TILE>>>(d_in, d_out);
    CHECK(cudaDeviceSynchronize());
    CHECK(cudaMemcpy(h_out_c, d_out, TILE * sizeof(float), cudaMemcpyDeviceToHost));

    row_reduce_free<<<1, TILE>>>(d_in, d_out);
    CHECK(cudaDeviceSynchronize());
    CHECK(cudaMemcpy(h_out_f, d_out, TILE * sizeof(float), cudaMemcpyDeviceToHost));

    CHECK(cudaFree(d_in));
    CHECK(cudaFree(d_out));

    bool ok = true;
    for (int i = 0; i < TILE; ++i)
    {
        if (std::fabs(h_out_c[i] - h_ref[i]) > 1e-3f)
        {
            std::printf("FAIL conflicted[%d]: got %.4f expected %.4f\n", i, h_out_c[i], h_ref[i]);
            ok = false;
        }
        if (std::fabs(h_out_f[i] - h_ref[i]) > 1e-3f)
        {
            std::printf("FAIL conflict_free[%d]: got %.4f expected %.4f\n", i, h_out_f[i], h_ref[i]);
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
