#pragma once

#include "execution_module.h"
#include "global_context.h"
#include "module.h"
#include "types.h"

namespace Emulator
{

class RtStream
{
  private:
    std::shared_ptr<GlobalContext> context_ = nullptr;
    std::shared_ptr<ExecutionModule> execution_module_ = nullptr;

  public:
    RtStream(const std::shared_ptr<Ptx::Module>& module);

    void KernelLaunch(uint64_t func, dim3 gridDim, dim3 blockDim, void** args, size_t sharedMem);
};

} // namespace Emulator
