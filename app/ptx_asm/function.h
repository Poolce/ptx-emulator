#pragma once

#include "instructions.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Emulator
{
namespace Ptx
{

using BasicBlockAddrMap = std::unordered_map<std::string, uint64_t>;
using InstructionList = std::vector<std::shared_ptr<Instruction>>;

class Function
{
  private:
    uint64_t pc_;
    BasicBlockAddrMap basic_blocks_;

    std::string name_;
    std::vector<FuncAttr> attrs_;
    std::unordered_map<std::string, FunctionParameter> params_;
    FuncType type_ = FuncType::Undefined;

  public:
    static std::pair<std::shared_ptr<Function>, InstructionList> Make(uint64_t pc,
                                                                      const std::string& attrs,
                                                                      const std::string& type,
                                                                      const std::string& name,
                                                                      const std::string& params,
                                                                      const std::string& content);

    void Dump();
    bool isEntry() const;
    uint64_t getOffset() const;
    uint64_t GetBasicBlockOffset(const std::string& bb_name) const;
    std::unordered_map<std::string, FunctionParameter> getParameters() const;
    const std::string &GetName() const { return name_; }
};
} // namespace Ptx

} // namespace Emulator
