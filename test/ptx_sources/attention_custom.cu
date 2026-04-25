#include "cuda_runtime.h"
#include "cuemu_io.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#define CHECK_ERROR(x) assert(x == cudaError_t::cudaSuccess)

constexpr int S = 32;
constexpr int D = 32;

template <int block_size>
__global__ void matmul(const float* A, int K, const float* B, int N, float* C)
{
    const int row = blockIdx.y * block_size + threadIdx.y;
    const int col = blockIdx.x * block_size + threadIdx.x;

    float acc = 0.0f;
    for (int k = 0; k < K; ++k)
        acc += A[row * K + k] * B[k * N + col];

    C[row * N + col] = acc;
}

template <int block_size>
__global__ void scale_softmax(const float* input, float* output, float inv_scale)
{
    const int row = blockIdx.x;
    const int tid = threadIdx.x;

    float val = expf(input[row * block_size + tid] * inv_scale);

    float sum = val;
    for (int offset = block_size / 2; offset > 0; offset >>= 1)
        sum += __shfl_xor_sync(0xFFFFFFFFu, sum, offset);

    output[row * block_size + tid] = val / sum;
}

static void cpu_matmul(const float* A, int M, int K, const float* B, int N, float* C)
{
    for (int i = 0; i < M; ++i)
    {
        for (int j = 0; j < N; ++j)
        {
            float acc = 0.0f;
            for (int k = 0; k < K; ++k)
            {
                acc += A[i * K + k] * B[k * N + j];
            }
            C[i * N + j] = acc;
        }
    }
}

static void cpu_scale_softmax(const float* input, float* output, int rows, int cols, float inv_scale)
{
    for (int r = 0; r < rows; ++r)
    {
        const float* in = input + r * cols;
        float* out = output + r * cols;

        float sum = 0.0f;
        for (int i = 0; i < cols; ++i)
        {
            out[i] = std::exp(in[i] * inv_scale);
            sum += out[i];
        }
        for (int i = 0; i < cols; ++i)
        {
            out[i] /= sum;
        }
    }
}

static float next_val(unsigned& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int>(state >> 8) & 0xFFFF) / 32768.0f - 1.0f;
}

int main()
{
    std::vector<float> Q(S * D);
    std::vector<float> K(S * D);
    std::vector<float> V(S * D);
    std::vector<float> Kt(D * S);

    unsigned state = 0xC0FFEE42u;
    cuemu_io::generate<float>("Q", Q, [&](size_t) { return next_val(state); });
    cuemu_io::generate<float>("K", K, [&](size_t) { return next_val(state); });
    cuemu_io::generate<float>("V", V, [&](size_t) { return next_val(state); });

    for (int i = 0; i < S; ++i)
    {
        for (int d = 0; d < D; ++d)
        {
            Kt[d * S + i] = K[i * D + d];
        }
    }

    float *gpuQ, *gpuKt, *gpuV, *gpuScores, *gpuAttn, *gpuOut;
    CHECK_ERROR(cudaMalloc((void**)&gpuQ, S * D * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuKt, D * S * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuV, S * D * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuScores, S * S * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuAttn, S * S * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuOut, S * D * sizeof(float)));

    CHECK_ERROR(cudaMemcpy(gpuQ, Q.data(), S * D * sizeof(float), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(gpuKt, Kt.data(), D * S * sizeof(float), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(gpuV, V.data(), S * D * sizeof(float), cudaMemcpyHostToDevice));

    constexpr int bs = 32;
    const float inv_scale = 1.0f / std::sqrt(static_cast<float>(D));

    matmul<bs><<<dim3(S / bs, S / bs), dim3(bs, bs)>>>(gpuQ, D, gpuKt, S, gpuScores);

    scale_softmax<bs><<<S, bs>>>(gpuScores, gpuAttn, inv_scale);

    matmul<bs><<<dim3(D / bs, S / bs), dim3(bs, bs)>>>(gpuAttn, S, gpuV, D, gpuOut);

    cudaDeviceSynchronize();

    std::vector<float> attn(S * S);
    std::vector<float> out(S * D);
    CHECK_ERROR(cudaMemcpy(attn.data(), gpuAttn, S * S * sizeof(float), cudaMemcpyDeviceToHost));
    CHECK_ERROR(cudaMemcpy(out.data(), gpuOut, S * D * sizeof(float), cudaMemcpyDeviceToHost));

    CHECK_ERROR(cudaFree(gpuQ));
    CHECK_ERROR(cudaFree(gpuKt));
    CHECK_ERROR(cudaFree(gpuV));
    CHECK_ERROR(cudaFree(gpuScores));
    CHECK_ERROR(cudaFree(gpuAttn));
    CHECK_ERROR(cudaFree(gpuOut));

    std::vector<float> ref_scores(S * S);
    std::vector<float> ref_attn(S * S);
    std::vector<float> ref_out(S * D);

    cpu_matmul(Q.data(), S, D, Kt.data(), S, ref_scores.data());
    cpu_scale_softmax(ref_scores.data(), ref_attn.data(), S, S, inv_scale);
    cpu_matmul(ref_attn.data(), S, S, V.data(), D, ref_out.data());

    constexpr float tol = 1e-3f;
    bool ok = true;

    for (int i = 0; i < S && ok; ++i)
    {
        for (int j = 0; j < D; ++j)
        {
            const int idx = i * D + j;
            const float diff = std::abs(out[idx] - ref_out[idx]);
            const float denom = std::max(std::abs(ref_out[idx]), 1e-6f);
            if (diff / denom > tol)
            {
                std::cerr << "FAIL: out[" << i << "][" << j << "] = " << out[idx] << ", expected " << ref_out[idx]
                          << "  (rel_diff=" << diff / denom << ")\n";
                ok = false;
                break;
            }
        }
    }

    for (int r = 0; r < S && ok; ++r)
    {
        float row_sum = 0.0f;
        for (int c = 0; c < S; ++c)
        {
            row_sum += attn[r * S + c];
        }
        if (std::abs(row_sum - 1.0f) > tol)
        {
            std::cerr << "FAIL: attn row " << r << " sums to " << row_sum << "\n";
            ok = false;
        }
    }

    return ok ? 0 : 1;
}
