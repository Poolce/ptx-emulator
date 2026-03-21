#include "function.h"

#include "utils.h"

#include <regex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace Emulator
{
namespace Ptx
{

namespace
{
const std::unordered_map<std::string, FuncType> StrToFuncType{
    {"", FuncType::Undefined},
    {"entry", FuncType::Entry},
    {"func", FuncType::Func},
};

const std::unordered_map<std::string, FuncAttr> StrToFuncAttr{
    {"visible", FuncAttr::Visible},
};
} // namespace

template <>
FuncType FromString(const std::string& str)
{
    return StrToFuncType.at(str);
}

template <>
FuncAttr FromString(const std::string& str)
{
    return StrToFuncAttr.at(str);
}

static std::vector<FuncAttr> parseAttributes(const std::string& attrs)
{
    constexpr std::string_view Pattern = "\\.([A-Za-z0-9_]+)";
    static const std::regex Re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize | std::regex::multiline);

    std::vector<FuncAttr> res{};
    auto begin = std::sregex_iterator(attrs.begin(), attrs.end(), Re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it)
    {
        const std::smatch& match = *it;
        if (match.size() == 2)
        {
            res.push_back(FromString<FuncAttr>(match[1]));
        }
        else
        {
            throw std::runtime_error("Function attribute is not matched");
        }
    }
    return res;
}

std::shared_ptr<Function>
Function::Make(const std::string& attrs, const std::string& type, const std::string& name, const std::string& content)
{
    auto func = std::make_shared<Function>();
    func->name_ = name;
    func->attrs_ = parseAttributes(attrs);
    func->type_ = FromString<FuncType>(type);
    constexpr std::string_view Pattern = "^\\$([A-Za-z0-9_]+):$";
    static const std::regex Re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize | std::regex::multiline);

    auto begin = std::sregex_iterator(content.begin(), content.end(), Re);
    auto end = std::sregex_iterator();
    std::string bb_name = "entry";
    int pos = 0;
    for (auto it = begin; it != end; ++it)
    {
        const std::smatch& match = *it;
        if (match.size() == 2)
        {
            auto bb_content = content.substr(pos, match.position() - pos);
            auto bb = BasicBlock::Make(bb_name, bb_content);
            func->basic_blocks_.push_back(bb);
            pos = match.position();
            bb_name = match[1].str();
        }
        else
        {
            throw std::runtime_error("Basic block is not matched");
        }
    }
    auto bb_content = content.substr(pos, content.size() - pos - 1);
    auto bb = BasicBlock::Make(bb_name, bb_content);
    func->basic_blocks_.push_back(bb);
    return func;
}

void Function::Dump()
{
    std::cout << name_ << ":\n";
    for (auto& bb : basic_blocks_)
    {
        bb->Dump();
    }
}

} // namespace Ptx
} // namespace Emulator
