#include "runtime_def.h"

#include "rt_interface.h"

#include <dlfcn.h>
#include <string.h>

#include <memory>

using namespace Emulator;

namespace
{

std::unique_ptr<RtInterface> interface;

} // namespace

cudaError_t cudaMalloc(void** devPtr, size_t size)
{
    *devPtr = malloc(size);
    if (*devPtr == nullptr)
    {
        return cudaError_t::cudaErrorMemoryAllocation;
    }
    return cudaError_t::cudaSuccess;
}

cudaError_t cudaFree(void* devPtr)
{
    free(devPtr);
    return cudaError_t::cudaSuccess;
}

cudaError_t cudaMemcpy([[maybe_unused]] void* dst,
                       [[maybe_unused]] const void* src,
                       [[maybe_unused]] size_t count,
                       [[maybe_unused]] cudaMemcpyKind kind)
{
    memcpy(dst, src, count);
    return cudaError_t::cudaSuccess;
}

cudaError_t cudaGetLastError()
{
    return cudaError_t::cudaSuccess;
}

const char* cudaGetErrorString([[maybe_unused]] cudaError_t error)
{
    return "No error\n";
}

cudaError_t __cudaLaunchKernel([[maybe_unused]] const void* func,
                               [[maybe_unused]] dim3 gridDim,
                               [[maybe_unused]] dim3 blockDim,
                               [[maybe_unused]] void** args,
                               [[maybe_unused]] size_t sharedMem,
                               [[maybe_unused]] cudaStream_t stream)
{
    std::cout << "cudaLaunchKernel " << std::hex << (uint64_t)func << "\n";

    uint64_t stream_id = interface->MakeStream();
    uint64_t func_descr = (uint64_t)(func);
    interface->KernelLaunch(func_descr, gridDim, blockDim, args, sharedMem, stream_id);
    return cudaError_t::cudaSuccess;
}

cudaError_t cudaDeviceSynchronize()
{
    interface->RemoveAllStreams();
    return cudaError_t::cudaSuccess;
}

void __cudaRegisterFatBinaryEnd([[maybe_unused]] void* fatCubinHandle)
{
    decltype(auto) orig = reinterpret_cast<void (*)(void*)>(dlsym(RTLD_NEXT, "__cudaRegisterFatBinaryEnd"));
    if (!orig)
    {
        throw std::runtime_error("Error while loading original function.");
    }
    orig(fatCubinHandle);
}

void __cudaRegisterFunction([[maybe_unused]] void** fatCubinHandle,
                            [[maybe_unused]] const char* hostFun,
                            [[maybe_unused]] char* deviceFun,
                            [[maybe_unused]] const char* deviceName,
                            [[maybe_unused]] int thread_limit,
                            [[maybe_unused]] uint3* tid,
                            [[maybe_unused]] uint3* bid,
                            [[maybe_unused]] dim3* bDim,
                            [[maybe_unused]] dim3* gDim,
                            [[maybe_unused]] int* wSize)
{
    interface->RegFunction((uint64_t)(hostFun), std::string(deviceFun));
}

void** __cudaRegisterFatBinary(void* fatCubin)
{
    interface = std::make_unique<RtInterface>();
    interface->LoadPtx();
    decltype(auto) orig = reinterpret_cast<void** (*)(void*)>(dlsym(RTLD_NEXT, "__cudaRegisterFatBinary"));
    if (!orig)
    {
        throw std::runtime_error("Error while loading original function.");
    }
    return orig(fatCubin);
}

void __cudaUnregisterFatBinary(void** fatCubinHandle)
{
    interface = nullptr;
    decltype(auto) orig = reinterpret_cast<void* (*)(void**)>(dlsym(RTLD_NEXT, "__cudaUnregisterFatBinary"));
    if (!orig)
    {
        throw std::runtime_error("Error while loading original function.");
    }
    orig(fatCubinHandle);
}