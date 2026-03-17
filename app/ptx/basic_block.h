#pragma once

#include "instructions.h"

namespace Emulator
{
namespace Ptx
{

using InstructionList = std::vector<std::shared_ptr<Instruction>>;

class BasicBlock
{
  private:
    std::string name_ = "";
    InstructionList instr_list_ = InstructionList();

  public:
    static std::shared_ptr<BasicBlock> Make(const std::string& name, const std::string& content);

    void Dump();
};
} // namespace Ptx
} // namespace Emulator
