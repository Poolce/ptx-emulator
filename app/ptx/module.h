#pragma once

#include "function.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Emulator
{
namespace Ptx
{
using FunctionMap = std::unordered_map<std::string, std::shared_ptr<Function>>;

class Module
{
  private:
    FunctionMap function_map_ = FunctionMap();

  public:
    void Dump();
    static std::shared_ptr<Module> Make(const std::string& ptx);
};
} // namespace Ptx
} // namespace Emulator