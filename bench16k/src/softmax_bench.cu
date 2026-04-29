// softmax: 512 rows × 32 cols FP32, input ~64KB
// BENCH_FLOPS=49152  BENCH_BYTES=131072
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>
static const int ROWS = 512, COLS = 32, REPS = 20, WARMUP = 5;

template<int BS>
__global__ void softmax_kernel(const float* in, float* out) {
    int row = blockIdx.x, tid = threadIdx.x;
    __shared__ float sh[BS];
    float val = expf(in[row * BS + tid]);
    sh[tid] = val;
    __syncthreads();
    for (int off = BS/2; off > 0; off >>= 1) {
        if (tid < off) sh[tid] += sh[tid + off];
        __syncthreads();
    }
    out[row * BS + tid] = val / sh[0];
}

int main() {
    size_t sz = ROWS * COLS * sizeof(float);
    float *di, *dout, *hi = new float[ROWS * COLS];
    for (int i = 0; i < ROWS * COLS; i++) hi[i] = float(i % 7) - 3.0f;
    cudaMalloc(&di, sz); cudaMalloc(&dout, sz);
    cudaMemcpy(di, hi, sz, cudaMemcpyHostToDevice);
    for (int i = 0; i < WARMUP; i++) { softmax_kernel<32><<<ROWS,COLS>>>(di,dout); cudaDeviceSynchronize(); }
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < REPS; i++) { softmax_kernel<32><<<ROWS,COLS>>>(di,dout); cudaDeviceSynchronize(); }
    auto t1 = std::chrono::steady_clock::now();
    printf("%.4f\n", std::chrono::duration<double,std::milli>(t1-t0).count() / REPS);
    cudaFree(di); cudaFree(dout); delete[] hi;
}
