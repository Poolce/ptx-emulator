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
    constexpr std::string_view Pattern = R"((.*)\s\.(entry|func)\s([A-Za-z0-9_]+)\(([\s\S]+)\)\n?\{([\s\S]+)})";
    static const std::regex Re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);

    auto begin = std::sregex_iterator(ptx.begin(), ptx.end(), Re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it)
    {
        const std::smatch& match = *it;
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

std::shared_ptr<Function> Module::GetEntryFunc() const
{
    for (const auto& [name, func] : function_map_)
    {
        if (func->isEntry())
            return func;
    }
    throw std::runtime_error("Entry function did not found");
}

} // namespace Ptx
} // namespace Emulator
