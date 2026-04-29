// attention: S=D=128 FP32 (128×128×4=64KB per Q/K/V), 3-kernel pipeline
// BENCH_FLOPS=8437760  BENCH_BYTES=327680
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>
#include <cmath>
static const int S = 128, D = 128, BS = 32, REPS = 10, WARMUP = 3;

// QK^T: (S×D) × (D×S) → S×S
template<int BLOCK>
__global__ void qk_matmul(const float* Q, const float* K, float* QKT,
                           int s, int d, float scale) {
    int row = blockIdx.y * BLOCK + threadIdx.y;
    int col = blockIdx.x * BLOCK + threadIdx.x;
    if (row >= s || col >= s) return;
    float acc = 0;
    for (int i = 0; i < d; i++) acc += Q[row*d+i] * K[col*d+i];
    QKT[row*s+col] = acc * scale;
}

// row-wise softmax on S×S; one block per row, BLOCK threads = S
template<int BLOCK>
__global__ void row_softmax(const float* in, float* out, int s) {
    int row = blockIdx.x, tid = threadIdx.x;
    __shared__ float sh[BLOCK];
    float v = expf(in[row*s+tid]);
    sh[tid] = v;
    __syncthreads();
    for (int off = BLOCK/2; off > 0; off >>= 1) {
        if (tid < off) sh[tid] += sh[tid+off];
        __syncthreads();
    }
    out[row*s+tid] = v / sh[0];
}

// Att × V: (S×S) × (S×D) → S×D
template<int BLOCK>
__global__ void av_matmul(const float* Att, const float* V, float* out,
                           int s, int d) {
    int row = blockIdx.y * BLOCK + threadIdx.y;
    int col = blockIdx.x * BLOCK + threadIdx.x;
    if (row >= s || col >= d) return;
    float acc = 0;
    for (int i = 0; i < s; i++) acc += Att[row*s+i] * V[i*d+col];
    out[row*d+col] = acc;
}

int main() {
    float scale = 1.0f / sqrtf(float(D));
    size_t sz_qkv = S * D * sizeof(float);   // 64 KB
    size_t sz_att = S * S * sizeof(float);   // 64 KB
    float *dQ, *dK, *dV, *dQKT, *dAtt, *dOut;
    cudaMalloc(&dQ,sz_qkv); cudaMalloc(&dK,sz_qkv); cudaMalloc(&dV,sz_qkv);
    cudaMalloc(&dQKT,sz_att); cudaMalloc(&dAtt,sz_att); cudaMalloc(&dOut,sz_qkv);
    float *h = new float[S*D];
    for (int i = 0; i < S*D; i++) h[i] = float(i%7-3) / 7.0f;
    cudaMemcpy(dQ,h,sz_qkv,cudaMemcpyHostToDevice);
    cudaMemcpy(dK,h,sz_qkv,cudaMemcpyHostToDevice);
    cudaMemcpy(dV,h,sz_qkv,cudaMemcpyHostToDevice);
    dim3 thr(BS,BS);
    dim3 gss((S+BS-1)/BS,(S+BS-1)/BS);  // for S×S output
    dim3 gsd((D+BS-1)/BS,(S+BS-1)/BS);  // for S×D output
    auto run = [&]() {
        qk_matmul<BS><<<gss,thr>>>(dQ,dK,dQKT,S,D,scale);
        row_softmax<S><<<S,S>>>(dQKT,dAtt,S);   // S threads per row
        av_matmul<BS><<<gsd,thr>>>(dAtt,dV,dOut,S,D);
        cudaDeviceSynchronize();
    };
    for (int i = 0; i < WARMUP; i++) run();
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < REPS; i++) run();
    auto t1 = std::chrono::steady_clock::now();
    printf("%.4f\n", std::chrono::duration<double,std::milli>(t1-t0).count() / REPS);
    cudaFree(dQ); cudaFree(dK); cudaFree(dV);
    cudaFree(dQKT); cudaFree(dAtt); cudaFree(dOut);
    delete[] h;
}
