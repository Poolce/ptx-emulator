#include "cuemu_io.h"

#include <cuda_runtime.h>

#include <cassert>
#include <cmath>
#include <cstdio>

#define CHECK(x) assert((x) == cudaSuccess)

__global__ void kernel_divergent(const float* __restrict__ in, float* __restrict__ out, int n)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= n)
        return;

    float x = in[tid];
    float result;

    if (threadIdx.x & 1)
    {
        float term = x;
        result = x;
        for (int k = 1; k <= 5; ++k)
        {
            term *= -x * x / static_cast<float>((2 * k) * (2 * k + 1));
            result += term;
        }
    }
    else
    {
        float term = 1.f;
        result = 1.f;
        for (int k = 1; k <= 9; ++k)
        {
            term *= x / static_cast<float>(k);
            result += term;
        }
    }

    out[tid] = result;
}

__global__ void kernel_convergent(const float* __restrict__ in, float* __restrict__ out, int n)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= n)
        return;

    float x = in[tid];

    float term = 1.f;
    float result = 1.f;
    for (int k = 1; k <= 9; ++k)
    {
        term *= x / static_cast<float>(k);
        result += term;
    }

    out[tid] = result;
}

static float cpu_exp_series(float x)
{
    float term = 1.f, acc = 1.f;
    for (int k = 1; k <= 9; ++k)
    {
        term *= x / k;
        acc += term;
    }
    return acc;
}

static float cpu_sin_series(float x)
{
    float term = x, acc = x;
    for (int k = 1; k <= 5; ++k)
    {
        term *= -x * x / static_cast<float>((2 * k) * (2 * k + 1));
        acc += term;
    }
    return acc;
}

int main()
{
    static constexpr int N = 32;
    static constexpr int BLOCK = 32;

    float h_in[N];
    CuemuIo::generate<float>("in", h_in, N, [](size_t i) { return (static_cast<float>(i) - 15.5f) * 0.05f; });

    float h_ref_div[N], h_ref_conv[N];
    for (int i = 0; i < N; ++i)
    {
        h_ref_div[i] = (i & 1) ? cpu_sin_series(h_in[i]) : cpu_exp_series(h_in[i]);
        h_ref_conv[i] = cpu_exp_series(h_in[i]);
    }

    float *d_in, *d_out;
    CHECK(cudaMalloc(&d_in, N * sizeof(float)));
    CHECK(cudaMalloc(&d_out, N * sizeof(float)));
    CHECK(cudaMemcpy(d_in, h_in, N * sizeof(float), cudaMemcpyHostToDevice));

    float h_out_div[N], h_out_conv[N];

    kernel_divergent<<<1, BLOCK>>>(d_in, d_out, N);
    CHECK(cudaDeviceSynchronize());
    CHECK(cudaMemcpy(h_out_div, d_out, N * sizeof(float), cudaMemcpyDeviceToHost));

    kernel_convergent<<<1, BLOCK>>>(d_in, d_out, N);
    CHECK(cudaDeviceSynchronize());
    CHECK(cudaMemcpy(h_out_conv, d_out, N * sizeof(float), cudaMemcpyDeviceToHost));

    CHECK(cudaFree(d_in));
    CHECK(cudaFree(d_out));

    bool ok = true;
    for (int i = 0; i < N; ++i)
    {
        if (std::fabs(h_out_div[i] - h_ref_div[i]) > 1e-4f)
        {
            std::printf("FAIL divergent[%d]: got %.6f expected %.6f\n", i, h_out_div[i], h_ref_div[i]);
            ok = false;
        }
        if (std::fabs(h_out_conv[i] - h_ref_conv[i]) > 1e-4f)
        {
            std::printf("FAIL convergent[%d]: got %.6f expected %.6f\n", i, h_out_conv[i], h_ref_conv[i]);
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
