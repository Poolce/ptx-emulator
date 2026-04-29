// fft: FFT_N=32, BATCHES=256 FP32 (2×32×256×4=64KB)
// BENCH_FLOPS=204800  BENCH_BYTES=131072
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>
#include <cmath>
static const int FFT_N = 32, LOG2_N = 5, BATCHES = 256, REPS = 20, WARMUP = 5;
static const float PI_F = 3.14159265358979323846f;

__device__ __forceinline__ int bit_rev5(int x) {
    return ((x&1)<<4)|((x&2)<<2)|(x&4)|((x&8)>>2)|((x&16)>>4);
}

__global__ void fft_forward(const float* src_r, const float* src_i,
                             float* dst_r, float* dst_i,
                             const float* tw_r, const float* tw_i) {
    int tid = threadIdx.x, batch = blockIdx.x;
    __shared__ float sr[FFT_N], si[FFT_N];
    sr[tid] = src_r[batch*FFT_N + bit_rev5(tid)];
    si[tid] = src_i[batch*FFT_N + bit_rev5(tid)];
    __syncthreads();
    for (int s = 0; s < LOG2_N; ++s) {
        int half    = 1 << s;
        int partner = tid ^ half;
        int j       = tid & (half - 1);
        int is_lower = (tid >> s) & 1;
        float wr = tw_r[s*(FFT_N/2)+j], wi = tw_i[s*(FFT_N/2)+j];
        float ar = sr[tid], ai = si[tid];
        float br = sr[partner], bi = si[partner];
        float tbr = wr*br - wi*bi, tbi = wr*bi + wi*br;
        float tar = wr*ar - wi*ai, tai = wr*ai + wi*ar;
        __syncthreads();
        if (!is_lower) { sr[tid] = ar+tbr; si[tid] = ai+tbi; }
        else           { sr[tid] = br-tar; si[tid] = bi-tai; }
        __syncthreads();
    }
    dst_r[batch*FFT_N+tid] = sr[tid];
    dst_i[batch*FFT_N+tid] = si[tid];
}

int main() {
    int tw_len = LOG2_N * (FFT_N/2);
    float *hsr = new float[FFT_N*BATCHES], *hsi = new float[FFT_N*BATCHES];
    float *htwr = new float[tw_len], *htwi = new float[tw_len];
    for (int i = 0; i < FFT_N*BATCHES; i++) { hsr[i] = float(i%7)/7.0f; hsi[i] = 0; }
    for (int s = 0; s < LOG2_N; s++) for (int j = 0; j < FFT_N/2; j++) {
        float ang = -2.0f * PI_F * j / float(1<<(s+1));
        htwr[s*(FFT_N/2)+j] = cosf(ang); htwi[s*(FFT_N/2)+j] = sinf(ang);
    }
    size_t sz = FFT_N*BATCHES*sizeof(float), sz_tw = tw_len*sizeof(float);
    float *dsr,*dsi,*ddr,*ddi,*dtwr,*dtwi;
    cudaMalloc(&dsr,sz); cudaMalloc(&dsi,sz); cudaMalloc(&ddr,sz); cudaMalloc(&ddi,sz);
    cudaMalloc(&dtwr,sz_tw); cudaMalloc(&dtwi,sz_tw);
    cudaMemcpy(dsr,hsr,sz,cudaMemcpyHostToDevice); cudaMemcpy(dsi,hsi,sz,cudaMemcpyHostToDevice);
    cudaMemcpy(dtwr,htwr,sz_tw,cudaMemcpyHostToDevice); cudaMemcpy(dtwi,htwi,sz_tw,cudaMemcpyHostToDevice);
    for (int i = 0; i < WARMUP; i++) { fft_forward<<<BATCHES,FFT_N>>>(dsr,dsi,ddr,ddi,dtwr,dtwi); cudaDeviceSynchronize(); }
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < REPS; i++) { fft_forward<<<BATCHES,FFT_N>>>(dsr,dsi,ddr,ddi,dtwr,dtwi); cudaDeviceSynchronize(); }
    auto t1 = std::chrono::steady_clock::now();
    printf("%.4f\n", std::chrono::duration<double,std::milli>(t1-t0).count() / REPS);
    cudaFree(dsr); cudaFree(dsi); cudaFree(ddr); cudaFree(ddi); cudaFree(dtwr); cudaFree(dtwi);
    delete[] hsr; delete[] hsi; delete[] htwr; delete[] htwi;
}
