#include "basic_block.h"

#include <iostream>
#include <regex>
#include <string_view>

namespace Emulator
{
namespace Ptx
{

std::shared_ptr<BasicBlock> BasicBlock::Make(const std::string& name, const std::string& content)
{
    auto bb = std::make_shared<BasicBlock>();
    bb->name_ = name;
    constexpr std::string_view kPattern = "^\\.?(@%p[0-9]+\\s)?([a-z]+).*$";
    static const std::regex kRe(kPattern.data(), std::regex::ECMAScript | std::regex::optimize | std::regex::multiline);

    auto begin = std::sregex_iterator(content.begin(), content.end(), kRe);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it)
    {
        const std::smatch& match = *it;
        if (match.size() >= 3)
        {
            std::string name = match[2].str();
            auto instr = makeInstruction(name, match[0].str());
            bb->instr_list_.push_back(instr);
        }
    }
    return bb;
}

void BasicBlock::Dump()
{
    std::cout << "\t" << name_ << ":\n";
    for (auto& instr : instr_list_)
    {
        instr->Dump();
    }
}

} // namespace Ptx
} // namespace Emulator
