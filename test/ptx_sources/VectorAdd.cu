// nvcc VectorAdd.cu --ptx

#include <cuda_runtime.h>

#include <cstdint>
#include <iostream>

__global__ void vectorAdd(const uint64_t* a, const uint64_t* b, uint64_t* c, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n)
    {
        c[idx] = a[idx] + b[idx];
    }
}

int main()
{
    const int n = 1000;
    size_t size = n * sizeof(uint64_t);

    uint64_t* h_a = new uint64_t[n];
    uint64_t* h_b = new uint64_t[n];
    uint64_t* h_c = new uint64_t[n];

    for (int i = 0; i < n; i++)
    {
        h_a[i] = i;
        h_b[i] = i * 2;
    }

    uint64_t *d_a, *d_b, *d_c;
    cudaMalloc(&d_a, size);
    cudaMalloc(&d_b, size);
    cudaMalloc(&d_c, size);

    cudaMemcpy(d_a, h_a, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, h_b, size, cudaMemcpyHostToDevice);

    int blockSize = 256;
    int numBlocks = (n + blockSize - 1) / blockSize;
    vectorAdd<<<numBlocks, blockSize>>>(d_a, d_b, d_c, n);

    cudaMemcpy(h_c, d_c, size, cudaMemcpyDeviceToHost);

    for (int i = 0; i < n; i++)
    {
        if (h_c[i] != h_a[i] + h_b[i])
        {
            std::cerr << "Err: " << i << std::endl;
            break;
        }
    }

    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);
    delete[] h_a;
    delete[] h_b;
    delete[] h_c;

    return 0;
}