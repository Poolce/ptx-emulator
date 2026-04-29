// vadd: N=8192 FP32, 2×input ~64KB
// BENCH_FLOPS=16384  BENCH_BYTES=98304
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>
static const int N = 8192, REPS = 50, WARMUP = 10;

__global__ void vectorAdd(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}

int main() {
    size_t sz = N * sizeof(float);
    float *da, *db, *dc;
    cudaMalloc(&da, sz); cudaMalloc(&db, sz); cudaMalloc(&dc, sz);
    float *ha = new float[N], *hb = new float[N];
    for (int i = 0; i < N; i++) { ha[i] = float(i); hb[i] = float(i) * 0.5f; }
    cudaMemcpy(da, ha, sz, cudaMemcpyHostToDevice);
    cudaMemcpy(db, hb, sz, cudaMemcpyHostToDevice);
    int bs = 32, grid = (N + 31) / 32;
    for (int i = 0; i < WARMUP; i++) { vectorAdd<<<grid,bs>>>(da,db,dc,N); cudaDeviceSynchronize(); }
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < REPS; i++) { vectorAdd<<<grid,bs>>>(da,db,dc,N); cudaDeviceSynchronize(); }
    auto t1 = std::chrono::steady_clock::now();
    printf("%.4f\n", std::chrono::duration<double,std::milli>(t1-t0).count() / REPS);
    cudaFree(da); cudaFree(db); cudaFree(dc);
    delete[] ha; delete[] hb;
}
