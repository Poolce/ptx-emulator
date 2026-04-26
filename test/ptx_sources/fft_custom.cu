#include "cuda_runtime.h"
#include "cuemu_io.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#define CHECK_ERROR(x) assert(x == cudaError_t::cudaSuccess)

static constexpr int FFT_N = 32;
static constexpr int LOG2_N = 5;
static constexpr int BATCHES = 32;
static constexpr float PI_F = 3.14159265358979323846f;

__device__ __forceinline__ int bit_rev5(int x)
{
    return ((x & 1) << 4) | ((x & 2) << 2) | (x & 4) | ((x & 8) >> 2) | ((x & 16) >> 4);
}

__global__ void
fft_forward(const float* src_r, const float* src_i, float* dst_r, float* dst_i, const float* tw_r, const float* tw_i)
{
    const int tid = threadIdx.x;
    const int batch = blockIdx.x;

    __shared__ float sr[FFT_N];
    __shared__ float si[FFT_N];

    sr[tid] = src_r[batch * FFT_N + bit_rev5(tid)];
    si[tid] = src_i[batch * FFT_N + bit_rev5(tid)];
    __syncthreads();

    for (int s = 0; s < LOG2_N; ++s)
    {
        const int half = 1 << s;
        const int partner = tid ^ half;
        const int j = tid & (half - 1);
        const int is_lower = (tid >> s) & 1;

        const float wr = tw_r[s * (FFT_N / 2) + j];
        const float wi = tw_i[s * (FFT_N / 2) + j];

        const float a_r = sr[tid];
        const float a_i = si[tid];
        const float b_r = sr[partner];
        const float b_i = si[partner];

        const float tb_r = wr * b_r - wi * b_i;
        const float tb_i = wr * b_i + wi * b_r;
        const float ta_r = wr * a_r - wi * a_i;
        const float ta_i = wr * a_i + wi * a_r;

        __syncthreads();

        if (!is_lower)
        {
            sr[tid] = a_r + tb_r;
            si[tid] = a_i + tb_i;
        }
        else
        {
            sr[tid] = b_r - ta_r;
            si[tid] = b_i - ta_i;
        }

        __syncthreads();
    }

    dst_r[batch * FFT_N + tid] = sr[tid];
    dst_i[batch * FFT_N + tid] = si[tid];
}

static void cpu_dft(const float* in_r, const float* in_i, float* out_r, float* out_i)
{
    for (int k = 0; k < FFT_N; ++k)
    {
        float sum_r = 0.0f;
        float sum_i = 0.0f;
        for (int t = 0; t < FFT_N; ++t)
        {
            const float angle = -2.0f * PI_F * k * t / FFT_N;
            sum_r += in_r[t] * std::cos(angle) - in_i[t] * std::sin(angle);
            sum_i += in_r[t] * std::sin(angle) + in_i[t] * std::cos(angle);
        }
        out_r[k] = sum_r;
        out_i[k] = sum_i;
    }
}

int main()
{
    const int total = FFT_N * BATCHES;
    const int tw_len = LOG2_N * (FFT_N / 2);

    std::vector<float> h_tw_r(tw_len);
    std::vector<float> h_tw_i(tw_len);
    for (int s = 0; s < LOG2_N; ++s)
    {
        const int stage_size = 1 << (s + 1);
        const int half = stage_size / 2;
        for (int j = 0; j < FFT_N / 2; ++j)
        {
            const int j_mod = j % half;
            const float angle = -2.0f * PI_F * j_mod / stage_size;
            h_tw_r[s * (FFT_N / 2) + j] = std::cos(angle);
            h_tw_i[s * (FFT_N / 2) + j] = std::sin(angle);
        }
    }

    std::vector<float> h_src_r(total);
    std::vector<float> h_src_i(total);

    unsigned state = 0xDEADBEEFu;
    auto next_val = [&](size_t) -> float
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(static_cast<int>(state >> 8) & 0xFFFF) / 32768.0f - 1.0f;
    };

    CuemuIo::generate<float>("src_r", h_src_r, next_val);
    CuemuIo::generate<float>("src_i", h_src_i, next_val);

    float *d_src_r, *d_src_i, *d_dst_r, *d_dst_i, *d_tw_r, *d_tw_i;
    CHECK_ERROR(cudaMalloc((void**)&d_src_r, total * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&d_src_i, total * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&d_dst_r, total * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&d_dst_i, total * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&d_tw_r, tw_len * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&d_tw_i, tw_len * sizeof(float)));

    CHECK_ERROR(cudaMemcpy(d_src_r, h_src_r.data(), total * sizeof(float), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(d_src_i, h_src_i.data(), total * sizeof(float), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(d_tw_r, h_tw_r.data(), tw_len * sizeof(float), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(d_tw_i, h_tw_i.data(), tw_len * sizeof(float), cudaMemcpyHostToDevice));

    fft_forward<<<BATCHES, FFT_N>>>(d_src_r, d_src_i, d_dst_r, d_dst_i, d_tw_r, d_tw_i);
    cudaDeviceSynchronize();

    std::vector<float> h_dst_r(total);
    std::vector<float> h_dst_i(total);
    CHECK_ERROR(cudaMemcpy(h_dst_r.data(), d_dst_r, total * sizeof(float), cudaMemcpyDeviceToHost));
    CHECK_ERROR(cudaMemcpy(h_dst_i.data(), d_dst_i, total * sizeof(float), cudaMemcpyDeviceToHost));

    CHECK_ERROR(cudaFree(d_src_r));
    CHECK_ERROR(cudaFree(d_src_i));
    CHECK_ERROR(cudaFree(d_dst_r));
    CHECK_ERROR(cudaFree(d_dst_i));
    CHECK_ERROR(cudaFree(d_tw_r));
    CHECK_ERROR(cudaFree(d_tw_i));

    std::vector<float> ref_r(FFT_N);
    std::vector<float> ref_i(FFT_N);

    constexpr float tol = 1e-3f;
    bool ok = true;

    for (int b = 0; b < BATCHES && ok; ++b)
    {
        cpu_dft(h_src_r.data() + b * FFT_N, h_src_i.data() + b * FFT_N, ref_r.data(), ref_i.data());

        for (int k = 0; k < FFT_N; ++k)
        {
            const float gr = h_dst_r[b * FFT_N + k];
            const float gi = h_dst_i[b * FFT_N + k];
            const float rr = ref_r[k];
            const float ri = ref_i[k];

            const float diff = std::sqrt((gr - rr) * (gr - rr) + (gi - ri) * (gi - ri));
            const float mag = std::sqrt(rr * rr + ri * ri);
            const float denom = std::max(mag, 1e-4f);

            if (diff / denom > tol)
            {
                std::cerr << "FAIL batch=" << b << " freq=" << k << ": gpu=(" << gr << "," << gi << ")" << " ref=("
                          << rr << "," << ri << ")" << " rel_err=" << diff / denom << "\n";
                ok = false;
                break;
            }
        }
    }

    return ok ? 0 : 1;
}
