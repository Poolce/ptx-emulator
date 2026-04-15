#include "cuda_runtime.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#define CHECK_ERROR(x) assert(x == cudaError_t::cudaSuccess);

// Each block processes one row of length block_size (= warp size = 32).
// Warp-shuffle butterfly reduction computes the row sum without shared memory.
template <int block_size>
__global__ void softmax(const float* input, float* output)
{
    const int row = blockIdx.x;
    const int tid = threadIdx.x;

    float val = expf(input[row * block_size + tid]);

    // Butterfly reduction: after the loop every lane holds the full warp sum
    float sum = val;
    for (int offset = block_size / 2; offset > 0; offset >>= 1)
        sum += __shfl_xor_sync(0xFFFFFFFFu, sum, offset);

    output[row * block_size + tid] = val / sum;
}

void launch_cuda_softmax(const float* input, float* output, int rows)
{
    constexpr int block_size = 32;
    const int total = rows * block_size;

    float *gpuIn, *gpuOut;

    CHECK_ERROR(cudaMalloc((void**)&gpuIn, total * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuOut, total * sizeof(float)));

    CHECK_ERROR(cudaMemcpy(gpuIn, input, total * sizeof(float), cudaMemcpyHostToDevice));

    softmax<block_size><<<rows, block_size>>>(gpuIn, gpuOut);
    cudaDeviceSynchronize();

    CHECK_ERROR(cudaMemcpy(output, gpuOut, total * sizeof(float), cudaMemcpyDeviceToHost));

    CHECK_ERROR(cudaFree(gpuIn));
    CHECK_ERROR(cudaFree(gpuOut));
}

static void cpu_softmax(const float* input, float* output, int rows, int cols)
{
    for (int r = 0; r < rows; ++r)
    {
        const float* in = input + r * cols;
        float* out = output + r * cols;

        float sum = 0.0f;
        for (int i = 0; i < cols; ++i)
        {
            out[i] = std::exp(in[i]);
            sum += out[i];
        }
        for (int i = 0; i < cols; ++i)
            out[i] /= sum;
    }
}

int main()
{
    constexpr int rows = 4;
    constexpr int cols = 32;

    std::vector<float> in(rows * cols);
    std::vector<float> out(rows * cols, 0.0f);
    std::vector<float> ref(rows * cols, 0.0f);

    // Values linearly spread over [-2, 2]
    for (int i = 0; i < rows * cols; ++i)
        in[i] = -2.0f + 4.0f * float(i) / float(rows * cols - 1);

    launch_cuda_softmax(in.data(), out.data(), rows);
    cpu_softmax(in.data(), ref.data(), rows, cols);

    constexpr float tol = 1e-4f;
    bool ok = true;

    for (int i = 0; i < rows * cols; ++i)
    {
        float diff = std::abs(out[i] - ref[i]);
        float scale = std::max(std::abs(ref[i]), 1e-6f);
        if (diff / scale > tol)
        {
            std::cerr << "FAIL: softmax[" << i / cols << "][" << i % cols << "] = " << out[i] << ", expected " << ref[i]
                      << "  (rel_diff=" << diff / scale << ")\n";
            ok = false;
        }
    }

    // Each row must sum to 1
    for (int r = 0; r < rows && ok; ++r)
    {
        float row_sum = 0.0f;
        for (int c = 0; c < cols; ++c)
            row_sum += out[r * cols + c];
        if (std::abs(row_sum - 1.0f) > tol)
        {
            std::cerr << "FAIL: row " << r << " sums to " << row_sum << "\n";
            ok = false;
        }
    }

    if (ok)
        std::cout << "OK: all " << rows << " rows pass softmax verification\n";

    return ok ? 0 : 1;
}
