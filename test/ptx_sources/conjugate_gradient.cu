#include "cuda_runtime.h"
#include "cuemu_io.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#define CHECK_ERROR(x) assert(x == cudaError_t::cudaSuccess)

constexpr int N = 512;
constexpr int MAX_ITER = 20;
constexpr float TOL = 1e-4f;

// Матрично-векторное умножение: y = A * x
template <int block_size>
__global__ void gemv(const float* A, const float* x, float* y, int n)
{
    const int row = blockIdx.x * block_size + threadIdx.x;
    if (row < n)
    {
        float acc = 0.0f;
        for (int j = 0; j < n; ++j)
            acc += A[row * n + j] * x[j];
        y[row] = acc;
    }
}

// Скалярное произведение: result += dot(x, y)
template <int block_size>
__global__ void dot_product(const float* x, const float* y, float* result, int n)
{
    __shared__ float cache[block_size];
    const int tid = threadIdx.x;
    int i = blockIdx.x * block_size + tid;

    float temp = 0.0f;
    while (i < n)
    {
        temp += x[i] * y[i];
        i += blockDim.x * gridDim.x;
    }
    cache[tid] = temp;
    __syncthreads();

    for (int s = block_size / 2; s > 0; s >>= 1)
    {
        if (tid < s)
            cache[tid] += cache[tid + s];
        __syncthreads();
    }

    if (tid == 0)
        atomicAdd(result, cache[0]);
}

// Обновление векторов: x = x + alpha * p; r = r - alpha * Ap
template <int block_size>
__global__ void update_x_r(float* x, float* r, const float* p, const float* Ap, float alpha, int n)
{
    const int i = blockIdx.x * block_size + threadIdx.x;
    if (i < n)
    {
        x[i] += alpha * p[i];
        r[i] -= alpha * Ap[i];
    }
}

// Обновление вектора направления: p = r + beta * p
template <int block_size>
__global__ void update_p(float* p, const float* r, float beta, int n)
{
    const int i = blockIdx.x * block_size + threadIdx.x;
    if (i < n)
    {
        p[i] = r[i] + beta * p[i];
    }
}

static void cpu_cg(const float* A, const float* b, float* x, int n, int max_iter, float tol)
{
    std::vector<float> r(n), p(n), Ap(n);
    float r_old_dot = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        x[i] = 0.0f;
        r[i] = b[i];
        p[i] = b[i];
        r_old_dot += r[i] * r[i];
    }

    for (int iter = 0; iter < max_iter; ++iter)
    {
        // Ap = A * p
        for (int i = 0; i < n; ++i)
        {
            float acc = 0.0f;
            for (int j = 0; j < n; ++j)
                acc += A[i * n + j] * p[j];
            Ap[i] = acc;
        }

        // pAp = p * Ap
        float pAp = 0.0f;
        for (int i = 0; i < n; ++i)
            pAp += p[i] * Ap[i];

        float alpha = r_old_dot / pAp;
        float r_new_dot = 0.0f;

        // x = x + alpha * p; r = r - alpha * Ap
        for (int i = 0; i < n; ++i)
        {
            x[i] += alpha * p[i];
            r[i] -= alpha * Ap[i];
            r_new_dot += r[i] * r[i];
        }

        if (std::sqrt(r_new_dot) < tol)
            break;

        float beta = r_new_dot / r_old_dot;

        // p = r + beta * p
        for (int i = 0; i < n; ++i)
            p[i] = r[i] + beta * p[i];

        r_old_dot = r_new_dot;
    }
}

static float next_val(unsigned& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int>(state >> 8) & 0xFFFF) / 32768.0f - 1.0f;
}

int main()
{
    std::vector<float> tmpA(N * N);
    std::vector<float> A(N * N);
    std::vector<float> b(N);
    std::vector<float> x_true(N);

    unsigned state = 0xC0FFEE42u;
    CuemuIo::generate<float>("tmpA", tmpA, [&](size_t) { return next_val(state); });
    CuemuIo::generate<float>("x_true", x_true, [&](size_t) { return next_val(state); });

    // Делаем матрицу A симметричной и положительно-определенной (SPD)
    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < N; ++j)
        {
            A[i * N + j] = 0.5f * (tmpA[i * N + j] + tmpA[j * N + i]);
            if (i == j)
                A[i * N + j] += N; // Диагональное преобладание
        }
    }

    // Вычисляем референсный b = A * x_true, чтобы система гарантированно имела решение
    for (int i = 0; i < N; ++i)
    {
        float acc = 0.0f;
        for (int j = 0; j < N; ++j)
            acc += A[i * N + j] * x_true[j];
        b[i] = acc;
    }

    std::vector<float> x(N, 0.0f); // Стартовое приближение: x_0 = 0
    std::vector<float> r = b;      // r_0 = b - A*x_0 = b
    std::vector<float> p = b;      // p_0 = r_0

    float *gpuA, *gpuB, *gpuX, *gpuR, *gpuP, *gpuAp, *gpuDotRes;
    CHECK_ERROR(cudaMalloc((void**)&gpuA, N * N * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuB, N * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuX, N * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuR, N * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuP, N * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuAp, N * sizeof(float)));
    CHECK_ERROR(cudaMalloc((void**)&gpuDotRes, sizeof(float)));

    CHECK_ERROR(cudaMemcpy(gpuA, A.data(), N * N * sizeof(float), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(gpuX, x.data(), N * sizeof(float), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(gpuR, r.data(), N * sizeof(float), cudaMemcpyHostToDevice));
    CHECK_ERROR(cudaMemcpy(gpuP, p.data(), N * sizeof(float), cudaMemcpyHostToDevice));

    constexpr int bs = 256;
    const int grid_size = (N + bs - 1) / bs;

    float r_old_dot = 0.0f;
    for (int i = 0; i < N; ++i)
        r_old_dot += r[i] * r[i];

    int iter = 0;
    for (; iter < MAX_ITER; ++iter)
    {
        // 1. Ap = A * p
        gemv<bs><<<grid_size, bs>>>(gpuA, gpuP, gpuAp, N);

        // 2. pAp = dot(p, Ap)
        CHECK_ERROR(cudaMemset(gpuDotRes, 0, sizeof(float)));
        dot_product<bs><<<grid_size, bs>>>(gpuP, gpuAp, gpuDotRes, N);

        float pAp;
        CHECK_ERROR(cudaMemcpy(&pAp, gpuDotRes, sizeof(float), cudaMemcpyDeviceToHost));

        // 3. alpha = r_old_dot / pAp
        float alpha = r_old_dot / pAp;

        // 4. x = x + alpha * p; r = r - alpha * Ap
        update_x_r<bs><<<grid_size, bs>>>(gpuX, gpuR, gpuP, gpuAp, alpha, N);

        // 5. r_new_dot = dot(r, r)
        CHECK_ERROR(cudaMemset(gpuDotRes, 0, sizeof(float)));
        dot_product<bs><<<grid_size, bs>>>(gpuR, gpuR, gpuDotRes, N);

        float r_new_dot;
        CHECK_ERROR(cudaMemcpy(&r_new_dot, gpuDotRes, sizeof(float), cudaMemcpyDeviceToHost));

        if (std::sqrt(r_new_dot) < TOL)
            break;

        // 6. beta = r_new_dot / r_old_dot
        float beta = r_new_dot / r_old_dot;

        // 7. p = r + beta * p
        update_p<bs><<<grid_size, bs>>>(gpuP, gpuR, beta, N);

        r_old_dot = r_new_dot;
    }
    cudaDeviceSynchronize();

    std::vector<float> out(N);
    CHECK_ERROR(cudaMemcpy(out.data(), gpuX, N * sizeof(float), cudaMemcpyDeviceToHost));

    CHECK_ERROR(cudaFree(gpuA));
    CHECK_ERROR(cudaFree(gpuB));
    CHECK_ERROR(cudaFree(gpuX));
    CHECK_ERROR(cudaFree(gpuR));
    CHECK_ERROR(cudaFree(gpuP));
    CHECK_ERROR(cudaFree(gpuAp));
    CHECK_ERROR(cudaFree(gpuDotRes));

    std::vector<float> ref_out(N);
    cpu_cg(A.data(), b.data(), ref_out.data(), N, MAX_ITER, TOL);

    bool ok = true;
    for (int i = 0; i < N; ++i)
    {
        const float diff = std::abs(out[i] - ref_out[i]);
        const float denom = std::max(std::abs(ref_out[i]), 1e-6f);
        if (diff / denom > 1e-3f)
        {
            std::cerr << "FAIL: out[" << i << "] = " << out[i] << ", expected " << ref_out[i]
                      << "  (rel_diff=" << diff / denom << ")\n";
            ok = false;
            break;
        }
    }

    if (ok)
        std::cout << "SUCCESS! Converged in " << iter << " iterations." << std::endl;

    return ok ? 0 : 1;
}
