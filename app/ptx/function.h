#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>

#include "instructions.h"

namespace Emulator
{
namespace Ptx
{

using BasicBlockAddrMap = std::unordered_map<std::string, uint64_t>;
using InstructionList = std::vector<std::shared_ptr<Instruction>>;

enum class FuncType : uint8_t
{
    Undefined,
    Entry,
    Func
};

enum class FuncAttr : uint8_t
{
    Visible
};

class Function
{
  private:
    uint64_t pc_;
    BasicBlockAddrMap basic_blocks_;

    std::string name_;
    std::vector<FuncAttr> attrs_;
    FuncType type_ = FuncType::Undefined;

  public:
    static std::pair<std::shared_ptr<Function>, InstructionList>
    Make(uint64_t pc, const std::string& attrs, const std::string& type, const std::string& name, const std::string& content);

    void Dump();
    bool isEntry() const;
};
} // namespace Ptx

} // namespace Emulator
