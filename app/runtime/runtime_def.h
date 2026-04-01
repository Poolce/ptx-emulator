#pragma once

#include <cstdint>
#include <iostream>

typedef void* cudaStream_t;

enum cudaError_t
{
    cudaSuccess,
    cudaErrorMemoryAllocation,
};

enum class cudaMemcpyKind
{
    cudaMemcpyHostToDevice = 0,
    cudaMemcpyDeviceToHost = 1,
    cudaMemcpyDeviceToDevice = 2,
    cudaMemcpyHostToHost = 3
};

struct dim3
{
    using type = uint32_t;
    uint32_t x, y, z;
    dim3(uint32_t _x = 1, uint32_t _y = 1, uint32_t _z = 1) : x(_x), y(_y), z(_z) {}
};

typedef dim3 uint3;

template <class T>
struct _v3
{
    using type = T;

    type x = 0;
    type y = 0;
    type z = 0;
};

template <class T>
struct _v4
{
    using type = T;

    _v4(const _v3<T>& vec) : x{vec.x}, y{vec.y}, z{vec.z} {}

    type x = 0;
    type y = 0;
    type z = 0;
    type w = 0;
};

using int4 = _v4<int64_t>;
using uint4 = _v4<uint64_t>;
using uint4_32 = _v4<uint32_t>;

extern "C"
{
    cudaError_t cudaMalloc(void** devPtr, size_t size);
    cudaError_t cudaFree(void* devPtr);
    cudaError_t cudaMemcpy(void* dst, const void* src, size_t count, cudaMemcpyKind kind);
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
