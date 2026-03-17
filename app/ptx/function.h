#pragma once

#include "basic_block.h"

#include <set>

namespace Emulator
{
namespace Ptx
{
using BasicBlockList = std::vector<std::shared_ptr<BasicBlock>>;

enum class FuncType
{
    UNDEFINED,
    Entry,
    Func
};

enum class FuncAttr
{
    Visible
};

class Function
{
  private:
    BasicBlockList basic_blocks_ = BasicBlockList();
    std::string name_ = "";
    std::vector<FuncAttr> attrs_ = {};
    FuncType type_ = FuncType::UNDEFINED;

  public:
    static std::shared_ptr<Function>
    Make(const std::string& attrs, const std::string& type, const std::string& name, const std::string& content);

    void Dump();
};
} // namespace Ptx

} // namespace Emulator
