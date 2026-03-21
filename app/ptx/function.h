#pragma once

#include "basic_block.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Emulator
{
namespace Ptx
{
using BasicBlockList = std::vector<std::shared_ptr<BasicBlock>>;

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
    BasicBlockList basic_blocks_;
    std::string name_;
    std::vector<FuncAttr> attrs_;
    FuncType type_ = FuncType::Undefined;

  public:
    static std::shared_ptr<Function>
    Make(const std::string& attrs, const std::string& type, const std::string& name, const std::string& content);

    void Dump();
};
} // namespace Ptx

} // namespace Emulator
