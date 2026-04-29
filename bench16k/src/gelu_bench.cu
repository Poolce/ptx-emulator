// gelu: N=16384 FP32, input ~64KB
// BENCH_FLOPS=163840  BENCH_BYTES=131072
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>
static const int N = 16384, REPS = 30, WARMUP = 5;

__global__ void gelu_kernel(const float* in, float* out, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        float x = in[i];
        out[i] = 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));
    }
}

int main() {
    size_t sz = N * sizeof(float);
    float *di, *dout, *hi = new float[N];
    for (int i = 0; i < N; i++) hi[i] = (float(i) - N/2) / float(N/4);
    cudaMalloc(&di, sz); cudaMalloc(&dout, sz);
    cudaMemcpy(di, hi, sz, cudaMemcpyHostToDevice);
    int bs = 32, grid = (N + 31) / 32;
    for (int i = 0; i < WARMUP; i++) { gelu_kernel<<<grid,bs>>>(di,dout,N); cudaDeviceSynchronize(); }
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < REPS; i++) { gelu_kernel<<<grid,bs>>>(di,dout,N); cudaDeviceSynchronize(); }
    auto t1 = std::chrono::steady_clock::now();
    printf("%.4f\n", std::chrono::duration<double,std::milli>(t1-t0).count() / REPS);
    cudaFree(di); cudaFree(dout); delete[] hi;
}
