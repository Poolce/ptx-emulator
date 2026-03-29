#pragma once

#include "module.h"
#include "warp_context.h"

namespace Emulator
{

class ExecutionModule
{
  private:
    std::shared_ptr<Ptx::Module> module_;

  public:
    ExecutionModule(const std::shared_ptr<Ptx::Module>& module);
    std::shared_ptr<Ptx::Instruction> GetInstruction(std::shared_ptr<WarpContext>& wc) const;
};

} // namespace Emulator
