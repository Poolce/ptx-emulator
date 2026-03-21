#pragma once

#include "module.h"

namespace Emulator
{
class ExecutionModule
{
  private:
    std::shared_ptr<Ptx::Module> module_;

  public:
    ExecutionModule(const std::shared_ptr<Ptx::Module>& module) : module_(module) {}
};

} // namespace Emulator