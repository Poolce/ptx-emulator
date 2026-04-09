#pragma once

#include "global_context.h"
#include "module.h"
#include "types.h"

namespace Emulator
{

class RtStream
{
  private:
    std::shared_ptr<GlobalContext> gpu_context_ = nullptr;
    std::shared_ptr<Ptx::Module> ptx_module_ = nullptr;

  public:
    RtStream(const std::shared_ptr<Ptx::Module>& module);

    void KernelLaunch(const std::string& func, dim3 gridDim, dim3 blockDim, void** args, size_t sharedMem);
};

} // namespace Emulator
