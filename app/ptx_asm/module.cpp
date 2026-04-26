#include "module.h"

#include "function.h"
#include "logger.h"

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
    LOG_INFO("PTX parsing: scanning functions");
    auto module = std::make_shared<Module>();
    constexpr std::string_view Pattern = R"((.*)\s\.(entry|func)\s([A-Za-z0-9_]+)\(([^)]+)\)\n?\{([^}]+)})";
    static const std::regex Re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);

    auto begin = std::sregex_iterator(ptx.begin(), ptx.end(), Re);
    auto end = std::sregex_iterator();

    uint64_t pc = 0;
    for (auto it = begin; it != end; ++it)
    {
        const std::smatch& match = *it;
        if (match.size() == 6)
        {
            std::string attrs = match[1].str();
            std::string type = match[2].str();
            std::string name = match[3].str();
            std::string params = match[4].str();
            std::string content = match[5].str();

            const auto& [func, func_instrs] = Function::Make(pc, attrs, type, name, params, content);

            module->instructions_.insert(module->instructions_.end(), func_instrs.begin(), func_instrs.end());
            pc += func_instrs.size();
            module->function_map_[name] = func;
        }
        else
        {
            throw std::runtime_error("Function is not matched");
        }
    }
    LOG_INFO("PTX parsing done: " + std::to_string(module->function_map_.size()) + " function(s), " +
             std::to_string(module->instructions_.size()) + " instruction(s)");
    return module;
}

std::shared_ptr<Instruction> Module::GetInstruction(uint64_t pc) const
{
    if (pc >= instructions_.size())
    {
        return nullptr;
    }
    return instructions_.at(pc);
}

std::shared_ptr<Function> Module::GetEntryFunction(const std::string& func_name) const
{
    auto func_it = function_map_.find(func_name);
    if (func_it != function_map_.end() && func_it->second->isEntry())
    {
        return func_it->second;
    }
    throw std::runtime_error("Entry function " + func_name + " not found");
}

void Module::Dump()
{
    uint64_t pc = 0;
    for (const auto& instr : instructions_)
    {
        std::cout << pc << "\t";
        instr->Dump();
        pc++;
    }

    for (const auto& [f_name, func] : function_map_)
    {
        func->Dump();
    }
}

uint64_t Module::GetBasicBlockOffset(const std::string& func_name, const std::string& sym) const
{
    auto func_it = function_map_.find(func_name);
    if (func_it == function_map_.end())
    {
        throw std::runtime_error("Function " + func_name + " not found.");
    }
    return func_it->second->GetBasicBlockOffset(sym);
}

std::string Module::GetBasicBlockAt(const std::string& func_name, uint64_t pc) const
{
    auto func_it = function_map_.find(func_name);
    if (func_it == function_map_.end())
    {
        return "";
    }
    return func_it->second->GetBasicBlockAt(pc);
}

} // namespace Ptx
} // namespace Emulator
