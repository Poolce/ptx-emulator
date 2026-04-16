#include "cuda_runtime.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#define CHECK_ERROR(x) assert(x == cudaError_t::cudaSuccess)

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

static void launch_cuda_mmul(const double* A,
                             std::size_t A_n,
                             std::size_t A_m,
                             const double* B,
                             std::size_t B_n,
                             std::size_t B_m,
                             double* C)
{
    constexpr std::size_t block_size = 32;

    double *gpuA, *gpuB, *gpuC;
    CHECK_ERROR(cudaMalloc((void**)&gpuA, A_n * A_m * sizeof(double)));
    CHECK_ERROR(cudaMalloc((void**)&gpuB, B_n * B_m * sizeof(double)));
    CHECK_ERROR(cudaMalloc((void**)&gpuC, A_n * B_m * sizeof(double)));

    CHECK_ERROR(cudaMemcpy(gpuA, A, A_n * A_m * sizeof(double), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(gpuB, B, B_n * B_m * sizeof(double), cudaMemcpyHostToDevice));

    dim3 threads(block_size, block_size);
    dim3 grid(B_m / block_size, A_n / block_size);

    mmul<block_size><<<grid, threads>>>(gpuA, A_m, gpuB, B_m, gpuC);
    cudaDeviceSynchronize();

    CHECK_ERROR(cudaMemcpy(C, gpuC, A_n * B_m * sizeof(double), cudaMemcpyDeviceToHost));
    CHECK_ERROR(cudaFree(gpuA));
    CHECK_ERROR(cudaFree(gpuB));
    CHECK_ERROR(cudaFree(gpuC));
}

// CPU reference: naive O(M*K*N) matmul
static void cpu_mmul(const double* A, std::size_t M, std::size_t K, const double* B, std::size_t N, double* C)
{
    for (std::size_t i = 0; i < M; ++i)
    {
        for (std::size_t j = 0; j < N; ++j)
        {
            double acc = 0.0;
            for (std::size_t k = 0; k < K; ++k)
            {
                acc += A[i * K + k] * B[k * N + j];
            }
            C[i * N + j] = acc;
        }
    }
}

// Simple LCG — no dependency on <random> or srand
static double next_val(unsigned& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<double>(static_cast<int>(state >> 8) & 0xFFFF) / 32768.0 - 1.0;
}

int main()
{
    constexpr std::size_t M = 128; // rows of A / rows of C
    constexpr std::size_t K = 128; // cols of A / rows of B
    constexpr std::size_t N = 128; // cols of B / cols of C

    std::vector<double> A(M * K);
    std::vector<double> B(K * N);
    std::vector<double> C(M * N, 0.0);
    std::vector<double> ref(M * N, 0.0);

    unsigned state = 0xDEADBEEFu;
    for (auto& v : A)
    {
        v = next_val(state);
    }
    for (auto& v : B)
    {
        v = next_val(state);
    }

    launch_cuda_mmul(A.data(), M, K, B.data(), K, N, C.data());
    cpu_mmul(A.data(), M, K, B.data(), N, ref.data());

    // For K=64 double accumulations ~1e-10 relative error is safe
    constexpr double tol = 1e-9;
    bool ok = true;

    for (std::size_t i = 0; i < M; ++i)
    {
        for (std::size_t j = 0; j < N; ++j)
        {
            const std::size_t idx = i * N + j;
            const double diff = std::abs(C[idx] - ref[idx]);
            const double scale = std::max(std::abs(ref[idx]), 1e-12);
            if (diff / scale > tol)
            {
                std::cerr << "FAIL: C[" << i << "][" << j << "] = " << C[idx] << ", expected " << ref[idx]
                          << "  (rel_diff=" << diff / scale << ")\n";
                ok = false;
            }
        }
    }

    if (ok)
    {
        std::cout << "OK: " << M << "x" << K << " * " << K << "x" << N << " matmul verified against CPU reference\n";
    }

    return ok ? 0 : 1;
}
