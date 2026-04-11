#include "rt_interface.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace Emulator
{

void RtInterface::LoadPtx()
{
    const char* env = std::getenv("CUEMU_TARGET_EXEC");
    if (!env)
    {
        throw std::runtime_error("Environment variable CUEMU_TARGET_EXEC not set.");
    }
    auto object = fs::path(env);

    if (!fs::is_regular_file(object))
    {
        throw std::runtime_error("Invalid object path.");
    }

    std::array<char, 512> buffer;
    std::string result;
    auto cmd = std::string("cuobjdump -ptx ") + object.string();

    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("Decoding object error");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }

    if (result.empty())
    {
        throw std::runtime_error("Ptx section is empty");
    }

    ptx_module_ = Ptx::Module::Make(result);
}

uint64_t RtInterface::MakeStream()
{
    if (!ptx_module_)
    {
        throw std::runtime_error("Module is not defined");
    }
    uint64_t stream_id = streams_.size();
    streams_.push_back(std::make_unique<RtStream>(ptx_module_));
    return stream_id;
}

void RtInterface::RemoveAllStreams()
{
    streams_.clear();
    functions_.clear();
}

void RtInterface::RegFunction(uint64_t descr, const std::string& name)
{
    functions_[descr] = name;
}

std::string RtInterface::GetFunctionName(uint64_t descr) const
{
    return functions_.at(descr);
}

void RtInterface::KernelLaunch([[maybe_unused]] uint64_t func,
                               [[maybe_unused]] dim3 gridDim,
                               [[maybe_unused]] dim3 blockDim,
                               [[maybe_unused]] void** args,
                               [[maybe_unused]] size_t sharedMem,
                               [[maybe_unused]] uint64_t stream_id)
{
    auto func_name = GetFunctionName(func);
    streams_.at(stream_id)->KernelLaunch(func_name, gridDim, blockDim, args, sharedMem);
}

} // namespace Emulator
