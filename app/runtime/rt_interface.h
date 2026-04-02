#pragma once

#include "rt_stream.h"
#include "types.h"

namespace Emulator
{

class RtInterface
{
  private:
    std::shared_ptr<Ptx::Module> ptx_module_ = nullptr;
    std::unordered_map<uint64_t, std::string> functions_;

    std::vector<std::unique_ptr<RtStream>> streams_{};

  public:
    RtInterface() = default;
    RtInterface(RtInterface&&) = delete;
    RtInterface(RtInterface&) = delete;
    RtInterface& operator=(const RtInterface&) = delete;
    RtInterface& operator=(RtInterface&&) = default;
    ~RtInterface() = default;

  public:
    void LoadPtx();
    uint64_t MakeStream();
    void RemoveAllStreams();

    void RegFunction(uint64_t descr, const std::string& name);
    std::string GetFunctionName(uint64_t descr) const;

    void KernelLaunch(uint64_t func, dim3 gridDim, dim3 blockDim, void** args, size_t sharedMem, uint64_t stream_id);
};

} // namespace Emulator
