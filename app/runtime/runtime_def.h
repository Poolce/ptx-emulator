#pragma once

#include "types.h"

#include <cstdint>
#include <iostream>

using cudaStream_t = void*; // NOLINT(readability-identifier-naming)

enum cudaError_t : std::uint8_t // NOLINT(readability-identifier-naming,performance-enum-size)
{
    cudaSuccess,               // NOLINT(readability-identifier-naming)
    cudaErrorMemoryAllocation, // NOLINT(readability-identifier-naming)
};

enum class cudaMemcpyKind : std::uint8_t // NOLINT(readability-identifier-naming,performance-enum-size)
{
    cudaMemcpyHostToDevice = 0,   // NOLINT(readability-identifier-naming)
    cudaMemcpyDeviceToHost = 1,   // NOLINT(readability-identifier-naming)
    cudaMemcpyDeviceToDevice = 2, // NOLINT(readability-identifier-naming)
    cudaMemcpyHostToHost = 3      // NOLINT(readability-identifier-naming)
};

extern "C"
{
    cudaError_t cudaMalloc(void** devPtr, size_t size);
    cudaError_t cudaFree(void* devPtr);
    cudaError_t cudaMemcpy(void* dst, const void* src, size_t count, cudaMemcpyKind kind);
    cudaError_t cudaGetLastError();
    const char* cudaGetErrorString(cudaError_t error);
    cudaError_t
    __cudaLaunchKernel(const void* func, // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
                       dim3 gridDim,
                       dim3 blockDim,
                       void** args,
                       size_t sharedMem,
                       cudaStream_t stream);
    cudaError_t cudaDeviceSynchronize();
    void
    __cudaRegisterFunction(void** fatCubinHandle, // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
                           const char* hostFun,
                           char* deviceFun,
                           const char* deviceName,
                           int thread_limit,
                           uint3* tid,
                           uint3* bid,
                           dim3* bDim,
                           dim3* gDim,
                           int* wSize);
    void**
    __cudaRegisterFatBinary(void* fatCubin); // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
    void __cudaRegisterFatBinaryEnd(
        void* fatCubinHandle); // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
    void __cudaUnregisterFatBinary(
        void** fatCubinHandle); // NOLINT(bugprone-reserved-identifier,readability-identifier-naming)
}
