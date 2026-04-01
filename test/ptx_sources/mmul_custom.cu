#include "cuda_runtime.h"
#include "omp.h"

#include <cassert>
#include <iostream>
#include <vector>

#define CHECK_ERROR(x) assert(x == cudaError_t::cudaSuccess);

template <int block_size>
__global__ void mmul(const double* A, std::size_t A_m, const double* B, std::size_t B_m, double* C)
{
    std::size_t id_x = blockIdx.x * block_size + threadIdx.x;
    std::size_t id_y = blockIdx.y * block_size + threadIdx.y;

    double C_XY_Element = 0;
#pragma unroll
    for (std::size_t i = 0; i < A_m; i++)
        C_XY_Element += A[A_m * id_y + i] * B[B_m * i + id_x];

    C[id_y * B_m + id_x] = C_XY_Element;
}

void launch_cuda_mmul(const double* A,
                      std::size_t A_n,
                      std::size_t A_m,
                      const double* B,
                      std::size_t B_n,
                      std::size_t B_m,
                      double* C)
{
    const std::size_t block_size = 32;

    double *gpuA, *gpuB, *gpuC;

    // MEMORY ALLOC
    CHECK_ERROR(cudaMalloc((void**)&gpuA, A_n * A_m * sizeof(double)));
    CHECK_ERROR(cudaMalloc((void**)&gpuB, B_n * B_m * sizeof(double)));
    CHECK_ERROR(cudaMalloc((void**)&gpuC, A_n * B_m * sizeof(double)));

    // MEMORY COPY H to D
    CHECK_ERROR(cudaMemcpy(gpuA, A, A_n * A_m * sizeof(double), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(gpuB, B, B_n * B_m * sizeof(double), cudaMemcpyHostToDevice));

    dim3 blocks(block_size, block_size);
    dim3 grid(B_m / block_size, A_n / block_size);

    mmul<block_size><<<grid, blocks>>>(gpuA, A_m, gpuB, B_m, gpuC);
    cudaDeviceSynchronize();
    // MEMORY COPY D to H
    CHECK_ERROR(cudaMemcpy(C, gpuC, A_n * B_m * sizeof(double), cudaMemcpyDeviceToHost));

    CHECK_ERROR(cudaFree(gpuA));
    CHECK_ERROR(cudaFree(gpuB));
    CHECK_ERROR(cudaFree(gpuC));
}

int main()
{
    std::vector<double> A(4000000, 1);
    std::vector<double> B(4000000, 2);
    std::vector<double> C(4000000);

    launch_cuda_mmul(A.data(), 2000, 2000, B.data(), 2000, 2000, C.data());

    return 0;
}