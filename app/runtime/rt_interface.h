#pragma once

#include "execution_module.h"
#include "global_context.h"
#include "module.h"

namespace Emulator
{

class RtInterface
{
  private:
    std::shared_ptr<Ptx::Module> ptx_module_ = nullptr;
    std::shared_ptr<GlobalContext> context_ = nullptr;
    std::shared_ptr<ExecutionModule> execution_module_ = nullptr;

  public:
    RtInterface() = default;
    RtInterface(RtInterface&&) = delete;
    RtInterface(RtInterface&) = delete;
    RtInterface& operator=(const RtInterface&) = delete;
    RtInterface& operator=(RtInterface&&) = default;
    ~RtInterface() = default;

  public:
    void LoadPtx();
};

} // namespace Emulator
