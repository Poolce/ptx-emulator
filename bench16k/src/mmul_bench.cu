// mmul: 128×128 FP32 (64KB per matrix), block_size=32
// BENCH_FLOPS=4194304  BENCH_BYTES=196608
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>
static const int N = 128, BS = 32, REPS = 10, WARMUP = 3;

template<int BLOCK>
__global__ void mmul(const float* A, int Am, const float* B, int Bm, float* C) {
    int x = blockIdx.x * BLOCK + threadIdx.x;
    int y = blockIdx.y * BLOCK + threadIdx.y;
    float acc = 0;
    for (int i = 0; i < Am; i++) acc += A[Am * y + i] * B[Bm * i + x];
    C[y * Bm + x] = acc;
}

int main() {
    size_t sz = N * N * sizeof(float);
    float *dA, *dB, *dC, *hA = new float[N*N], *hB = new float[N*N];
    for (int i = 0; i < N*N; i++) { hA[i] = float(i%7)/7.0f; hB[i] = float(i%5)/5.0f; }
    cudaMalloc(&dA, sz); cudaMalloc(&dB, sz); cudaMalloc(&dC, sz);
    cudaMemcpy(dA, hA, sz, cudaMemcpyHostToDevice);
    cudaMemcpy(dB, hB, sz, cudaMemcpyHostToDevice);
    dim3 thr(BS, BS), grid(N/BS, N/BS);
    for (int i = 0; i < WARMUP; i++) { mmul<BS><<<grid,thr>>>(dA,N,dB,N,dC); cudaDeviceSynchronize(); }
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < REPS; i++) { mmul<BS><<<grid,thr>>>(dA,N,dB,N,dC); cudaDeviceSynchronize(); }
    auto t1 = std::chrono::steady_clock::now();
    printf("%.4f\n", std::chrono::duration<double,std::milli>(t1-t0).count() / REPS);
    cudaFree(dA); cudaFree(dB); cudaFree(dC);
    delete[] hA; delete[] hB;
}
