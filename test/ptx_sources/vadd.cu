// nvcc VectorAdd.cu --ptx

#include "cuemu_io.h"

#include <cuda_runtime.h>

#include <cstdint>
#include <iostream>

__global__ void vectorAdd(const float* a, const float* b, float* c, int n)
{

    uint64_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n)
    {
        c[idx] = a[idx] + b[idx];
    }
}

int main()
{
    const int n = 900;
    size_t size = n * sizeof(float);

    float* h_a = new float[n];
    float* h_b = new float[n];
    float* h_c = new float[n];

    cuemu_io::generate<float>("a", h_a, n, [](size_t i) { return float(i) / 10.0f; });
    cuemu_io::generate<float>("b", h_b, n, [](size_t i) { return float(i) / 10.0f; });

    float *d_a, *d_b, *d_c;
    cudaMalloc(&d_a, size);
    cudaMalloc(&d_b, size);
    cudaMalloc(&d_c, size);

    cudaMemcpy(d_a, h_a, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, h_b, size, cudaMemcpyHostToDevice);

    int blockSize = 400;
    int numBlocks = 3;
    vectorAdd<<<numBlocks, blockSize>>>(d_a, d_b, d_c, n);
    cudaDeviceSynchronize();

    cudaMemcpy(h_c, d_c, size, cudaMemcpyDeviceToHost);

    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);

    delete[] h_a;
    delete[] h_b;
    delete[] h_c;

    return 0;
}
