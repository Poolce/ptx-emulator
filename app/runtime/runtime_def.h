#pragma once

#include "types.h"

#include <cstdint>
#include <iostream>

using cudaStream_t = void*;

// NOLINTBEGIN

enum cudaError_t : std::uint8_t
{
    cudaSuccess,
    cudaErrorMemoryAllocation,
};

enum class cudaMemcpyKind : std::uint8_t
{
    cudaMemcpyHostToDevice = 0,
    cudaMemcpyDeviceToHost = 1,
    cudaMemcpyDeviceToDevice = 2,
    cudaMemcpyHostToHost = 3
};

extern "C"
{
    cudaError_t cudaMalloc(void** devPtr, size_t size);
    cudaError_t cudaFree(void* devPtr);
    cudaError_t cudaMemcpy(void* dst, const void* src, size_t count, cudaMemcpyKind kind);
    cudaError_t cudaMemset(void* devPtr, int value, size_t count);
    cudaError_t cudaGetLastError();
    const char* cudaGetErrorString(cudaError_t error);
    cudaError_t __cudaLaunchKernel(const void* func,
                                   dim3 gridDim,
                                   dim3 blockDim,
                                   void** args,
                                   size_t sharedMem,
                                   cudaStream_t stream);
    cudaError_t cudaDeviceSynchronize();
    void __cudaRegisterFunction(void** fatCubinHandle,
                                const char* hostFun,
                                char* deviceFun,
                                const char* deviceName,
                                int thread_limit,
                                uint3* tid,
                                uint3* bid,
                                dim3* bDim,
                                dim3* gDim,
                                int* wSize);
    void** __cudaRegisterFatBinary(void* fatCubin);
    void __cudaRegisterFatBinaryEnd(void* fatCubinHandle);
    void __cudaUnregisterFatBinary(void** fatCubinHandle);
}

// NOLINTEND
