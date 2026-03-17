#include "module.h"

#include "function.h"

#include <iostream>
#include <regex>
#include <stdexcept>
#include <string_view>

namespace Emulator
{
namespace Ptx
{

std::shared_ptr<Module> Module::Make(const std::string& ptx)
{
    auto module = std::make_shared<Module>();
    constexpr std::string_view kPattern = R"((.*)\s\.(entry|func)\s([A-z0-9_]+)\(([\s\S]+)\)\n?\{([\s\S]+)})";
    static const std::regex kRe(kPattern.data(), std::regex::ECMAScript | std::regex::optimize);

    auto begin = std::sregex_iterator(ptx.begin(), ptx.end(), kRe);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it)
    {
        std::smatch match = *it;
        if (match.size() == 6)
        {
            std::string attrs = match[1].str();
            std::string type = match[2].str();
            std::string name = match[3].str();
            std::string content = match[4].str() + match[5].str();
            auto func = Function::Make(attrs, type, name, content);
            module->function_map_[name] = func;
        }
        else
        {
            throw std::runtime_error("Function is not matched");
        }
    }
    return module;
}

void Module::Dump()
{
    for (auto& [name, func] : function_map_)
    {
        std::cout << "\n\n";
        func->Dump();
    }
}
} // namespace Ptx
} // namespace Emulator
