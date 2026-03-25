#pragma once

#include "function.h"

namespace Emulator
{
namespace Ptx
{
using FunctionMap = std::unordered_map<std::string, std::shared_ptr<Function>>;

class Module
{
  private:
    FunctionMap function_map_;
    InstructionList instructions_;

  public:
    static std::shared_ptr<Module> Make(const std::string& ptx);

    void Dump();
    std::shared_ptr<Function> GetEntryFunc() const;
};
} // namespace Ptx
} // namespace Emulator