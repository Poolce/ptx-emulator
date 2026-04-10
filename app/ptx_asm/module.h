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
    std::shared_ptr<Function> GetEntryFunction(const std::string& func_name) const;
    std::shared_ptr<Instruction> GetInstruction(uint64_t pc) const;
    uint64_t GetBasicBlockOffset(const std::string& func_name, const std::string& sym) const;
};
} // namespace Ptx
} // namespace Emulator
