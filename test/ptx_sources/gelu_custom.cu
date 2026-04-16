#include "cuda.h"

#include <math_constants.h>

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#define CHECK_ERROR(x) assert(x == cudaError_t::cudaSuccess);

__global__ void Gelu(const float* input, float* output, int n)
{
    int id = threadIdx.x + blockIdx.x * blockDim.x;
    if (id < n)
    {
        float x = input[id];
        float y = 0.5f * x * (1.0f + tanhf(sqrtf(2.0f / CUDART_PI_F) * (x + 0.044715f * x * x * x)));
        output[id] = y;
    }
}

void launch_cuda_gelu(const float* input, float* output, std::size_t n)
{
    const std::size_t block_size = 32;

    float *gpuIn, *gpuOut;

    CHECK_ERROR(cudaMalloc((void**)&gpuIn, n * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuOut, n * sizeof(float)));

    CHECK_ERROR(cudaMemcpy(gpuIn, input, n * sizeof(float), cudaMemcpyHostToDevice));

    dim3 block(block_size);
    dim3 grid((n + block_size - 1) / block_size);

    Gelu<<<grid, block>>>(gpuIn, gpuOut, n);
    cudaDeviceSynchronize();

    CHECK_ERROR(cudaMemcpy(output, gpuOut, n * sizeof(float), cudaMemcpyDeviceToHost));

    CHECK_ERROR(cudaFree(gpuIn));
    CHECK_ERROR(cudaFree(gpuOut));
}

static float cpu_gelu(float x)
{
    constexpr float sqrt_2_over_pi = 0.7978845608f;
    constexpr float coeff = 0.044715f;
    float x3 = x * x * x;
    return 0.5f * x * (1.0f + std::tanh(sqrt_2_over_pi * (x + coeff * x3)));
}

int main()
{
    constexpr std::size_t N = 2096;
    constexpr float lo = -3.0f;
    constexpr float hi = 3.0f;

    std::vector<float> in(N), out(N, 0.0f);
    for (std::size_t i = 0; i < N; ++i)
        in[i] = lo + (hi - lo) * float(i) / float(N - 1);

    launch_cuda_gelu(in.data(), out.data(), N);

    constexpr float tol = 1e-4f;
    bool ok = true;
    for (std::size_t i = 0; i < N; ++i)
    {
        float expected = cpu_gelu(in[i]);
        float diff = std::abs(out[i] - expected);
        float scale = std::max(std::abs(expected), 1e-6f);
        if (diff / scale > tol)
        {
            std::cerr << "FAIL: gelu(" << in[i] << ") = " << out[i] << ", expected " << expected
                      << "  (rel_diff=" << diff / scale << ")\n";
            ok = false;
        }
    }

    if (ok)
        std::cout << "OK: all " << N << " GELU values match CPU reference\n";

    return ok ? 0 : 1;
}
