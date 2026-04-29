// conjugate gradient: N=4096, tri-diagonal A, 10 iterations
// BENCH_FLOPS=696320  BENCH_BYTES=1474560
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>
#include <cmath>
static const int N = 4096, MAX_ITER = 10, REPS = 3, WARMUP = 1;
static const int BS = 256;

template<int BLOCK>
__global__ void spmv_tridiag(const float* diag, const float* off,
                              const float* x, float* y, int n) {
    int i = blockIdx.x * BLOCK + threadIdx.x;
    if (i >= n) return;
    float v = diag[i] * x[i];
    if (i > 0)   v += off[i-1] * x[i-1];
    if (i < n-1) v += off[i]   * x[i+1];
    y[i] = v;
}

template<int BLOCK>
__global__ void dot_partial(const float* a, const float* b, float* out, int n) {
    __shared__ float s[BLOCK];
    int i = blockIdx.x * BLOCK + threadIdx.x, tid = threadIdx.x;
    s[tid] = (i < n) ? a[i] * b[i] : 0.0f;
    __syncthreads();
    for (int off = BLOCK/2; off > 0; off >>= 1) {
        if (tid < off) s[tid] += s[tid+off];
        __syncthreads();
    }
    if (tid == 0) out[blockIdx.x] = s[0];
}

__global__ void axpy(float* y, float alpha, const float* x, int n) {
    int i = blockIdx.x * BS + threadIdx.x;
    if (i < n) y[i] += alpha * x[i];
}

__global__ void xpay(float* y, float alpha, const float* x, int n) {
    int i = blockIdx.x * BS + threadIdx.x;
    if (i < n) y[i] = x[i] + alpha * y[i];
}

int main() {
    const int NB = (N + BS - 1) / BS;
    float *diag = new float[N], *off = new float[N-1];
    float *b_h = new float[N], *hpart = new float[NB];
    for (int i = 0; i < N; i++) { diag[i] = 4.0f; b_h[i] = 1.0f; }
    for (int i = 0; i < N-1; i++) off[i] = -1.0f;

    float *ddiag,*doff,*dx,*dr,*dp,*dap,*dpartial,*db;
    cudaMalloc(&ddiag,N*4); cudaMalloc(&doff,(N-1)*4);
    cudaMalloc(&dx,N*4); cudaMalloc(&dr,N*4); cudaMalloc(&dp,N*4);
    cudaMalloc(&dap,N*4); cudaMalloc(&db,N*4); cudaMalloc(&dpartial,NB*4);
    cudaMemcpy(ddiag,diag,N*4,cudaMemcpyHostToDevice);
    cudaMemcpy(doff,off,(N-1)*4,cudaMemcpyHostToDevice);
    cudaMemcpy(db,b_h,N*4,cudaMemcpyHostToDevice);

    auto cg_solve = [&]() {
        cudaMemset(dx, 0, N*4);
        cudaMemcpy(dr,db,N*4,cudaMemcpyDeviceToDevice);
        cudaMemcpy(dp,db,N*4,cudaMemcpyDeviceToDevice);
        for (int it = 0; it < MAX_ITER; it++) {
            spmv_tridiag<BS><<<NB,BS>>>(ddiag,doff,dp,dap,N);
            dot_partial<BS><<<NB,BS>>>(dp,dap,dpartial,N);
            cudaMemcpy(hpart,dpartial,NB*4,cudaMemcpyDeviceToHost);
            float pap = 0; for (int i = 0; i < NB; i++) pap += hpart[i];
            dot_partial<BS><<<NB,BS>>>(dr,dr,dpartial,N);
            cudaMemcpy(hpart,dpartial,NB*4,cudaMemcpyDeviceToHost);
            float rr = 0; for (int i = 0; i < NB; i++) rr += hpart[i];
            if (fabsf(pap) < 1e-30f) break;
            float alpha = rr / pap;
            axpy<<<NB,BS>>>(dx, alpha,dp,N);
            axpy<<<NB,BS>>>(dr,-alpha,dap,N);
            dot_partial<BS><<<NB,BS>>>(dr,dr,dpartial,N);
            cudaMemcpy(hpart,dpartial,NB*4,cudaMemcpyDeviceToHost);
            float rr_new = 0; for (int i = 0; i < NB; i++) rr_new += hpart[i];
            float beta = (rr > 0) ? rr_new/rr : 0.0f;
            xpay<<<NB,BS>>>(dp,beta,dr,N);
        }
        cudaDeviceSynchronize();
    };

    for (int i = 0; i < WARMUP; i++) cg_solve();
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < REPS; i++) cg_solve();
    auto t1 = std::chrono::steady_clock::now();
    printf("%.4f\n", std::chrono::duration<double,std::milli>(t1-t0).count() / REPS);

    cudaFree(ddiag); cudaFree(doff); cudaFree(dx); cudaFree(dr);
    cudaFree(dp); cudaFree(dap); cudaFree(db); cudaFree(dpartial);
    delete[] diag; delete[] off; delete[] b_h; delete[] hpart;
}
