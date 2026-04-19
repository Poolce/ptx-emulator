#include "runtime_def.h"

#include "logger.h"
#include "rt_interface.h"

#include <dlfcn.h>

#include <cstring>
#include <memory>

using namespace Emulator;

namespace
{

std::unique_ptr<RtInterface> interface;

} // namespace

cudaError_t cudaMalloc(void** devPtr, size_t size)
{
    LOG_DEBUG("cudaMalloc size=" + std::to_string(size));
    *devPtr = malloc(size);
    if (*devPtr == nullptr)
    {
        LOG_ERROR("cudaMalloc failed: allocation of " + std::to_string(size) + " bytes returned nullptr");
        return cudaError_t::cudaErrorMemoryAllocation;
    }
    return cudaError_t::cudaSuccess;
}

cudaError_t cudaFree(void* devPtr)
{
    LOG_DEBUG("cudaFree");
    free(devPtr);
    return cudaError_t::cudaSuccess;
}

cudaError_t cudaMemcpy([[maybe_unused]] void* dst,
                       [[maybe_unused]] const void* src,
                       [[maybe_unused]] size_t count,
                       [[maybe_unused]] cudaMemcpyKind kind)
{
    LOG_DEBUG("cudaMemcpy count=" + std::to_string(count));
    memcpy(dst, src, count);
    return cudaError_t::cudaSuccess;
}

cudaError_t cudaGetLastError()
{
    LOG_DEBUG("cudaGetLastError");
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
    LOG_DEBUG("__cudaLaunchKernel intercepted");
    try
    {
        if (!interface)
        {
            throw std::runtime_error(
                "Runtime interface not initialized. Ensure __cudaRegisterFatBinary was called first.");
        }
        uint64_t stream_id = interface->MakeStream();
        auto func_descr = (uint64_t)(func);
        interface->KernelLaunch(func_descr, gridDim, blockDim, args, sharedMem, stream_id);
    }
    catch (const std::runtime_error& e)
    {
        LOG_ERROR(std::string("__cudaLaunchKernel: ") + e.what());
        throw;
    }
    return cudaError_t::cudaSuccess;
}

cudaError_t cudaDeviceSynchronize()
{
    LOG_DEBUG("cudaDeviceSynchronize");
    interface->RemoveAllStreams();
    return cudaError_t::cudaSuccess;
}

void __cudaRegisterFatBinaryEnd([[maybe_unused]] void* fatCubinHandle)
{
    LOG_DEBUG("__cudaRegisterFatBinaryEnd intercepted");
    try
    {
        decltype(auto) orig = reinterpret_cast<void (*)(void*)>(dlsym(RTLD_NEXT, "__cudaRegisterFatBinaryEnd"));
        if (!orig)
        {
            throw std::runtime_error("Error while loading original function.");
        }
        orig(fatCubinHandle);
    }
    catch (const std::runtime_error& e)
    {
        LOG_ERROR(std::string("__cudaRegisterFatBinaryEnd: ") + e.what());
        throw;
    }
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
    LOG_DEBUG(std::string("__cudaRegisterFunction deviceFun=") + deviceFun);
    interface->RegFunction((uint64_t)(hostFun), std::string(deviceFun));
}

void** __cudaRegisterFatBinary(void* fatCubin)
{
    LOG_DEBUG("__cudaRegisterFatBinary intercepted");
    try
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
    catch (const std::runtime_error& e)
    {
        LOG_ERROR(std::string("__cudaRegisterFatBinary: ") + e.what());
        throw;
    }
}

void __cudaUnregisterFatBinary(void** fatCubinHandle)
{
    LOG_DEBUG("__cudaUnregisterFatBinary intercepted");
    try
    {
        interface = nullptr;
        decltype(auto) orig = reinterpret_cast<void* (*)(void**)>(dlsym(RTLD_NEXT, "__cudaUnregisterFatBinary"));
        if (!orig)
        {
            throw std::runtime_error("Error while loading original function.");
        }
        orig(fatCubinHandle);
    }
    catch (const std::runtime_error& e)
    {
        LOG_ERROR(std::string("__cudaUnregisterFatBinary: ") + e.what());
        throw;
    }
}
