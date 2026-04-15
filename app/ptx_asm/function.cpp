#include "function.h"

#include "utils.h"

#include <regex>
#include <stdexcept>
#include <string_view>

namespace Emulator
{
namespace Ptx
{

static std::unordered_map<std::string, FunctionParameter> parseParameters(const std::string& params)
{
    constexpr std::string_view Pattern = R"(\.param\s\.([fsub]8|[fsub]16|[fsub]32|[fsub]64|pred)\s([A-z0-9_]+),?)";
    static const std::regex Re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize | std::regex::multiline);

    std::unordered_map<std::string, FunctionParameter> res{};
    auto begin = std::sregex_iterator(params.begin(), params.end(), Re);
    auto end = std::sregex_iterator();
    uint8_t param_id = 0;
    for (auto it = begin; it != end; ++it)
    {
        const std::smatch& match = *it;
        if (match.size() == 3)
        {
            auto dtype = FromString<dataType>(match[1].str());
            auto name = match[2].str();
            res[name] = {.name = name, .type = dtype, .id = param_id};
            param_id++;
        }
        else
        {
            throw std::runtime_error("Function attribute is not matched");
        }
    }
    return res;
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

static InstructionList parseInstructions(const std::string& content)
{
    InstructionList instrs{};

    constexpr std::string_view Pattern = "^\\.?(@%p[0-9]+\\s)?([a-z]+2?).*$";
    static const std::regex Re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize | std::regex::multiline);
    auto begin = std::sregex_iterator(content.begin(), content.end(), Re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it)
    {
        const std::smatch& match = *it;
        if (match.size() >= 3)
        {
            std::string name = match[2].str();
            try {
                auto instr = makeInstruction(name, match[0].str());
                instrs.push_back(instr);
            } catch (const std::runtime_error& e) {
                throw std::runtime_error("Failed to parse instruction: " + match[0].str());
            }
        }
    }
    return instrs;
}

std::pair<std::shared_ptr<Function>, InstructionList> Function::Make(uint64_t pc,
                                                                     const std::string& attrs,
                                                                     const std::string& type,
                                                                     const std::string& name,
                                                                     const std::string& params,
                                                                     const std::string& content)
{
    auto func = std::make_shared<Function>();
    func->name_ = name;
    func->attrs_ = parseAttributes(attrs);
    func->params_ = parseParameters(params);
    func->type_ = FromString<FuncType>(type);
    func->pc_ = pc;

    InstructionList instrs{};
    constexpr std::string_view Pattern = "^\\$([A-Za-z0-9_]+):$";
    static const std::regex Re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize | std::regex::multiline);
    auto begin = std::sregex_iterator(content.begin(), content.end(), Re);
    auto end = std::sregex_iterator();
    std::string bb_name = "entry";
    int pos = 0;
    uint64_t local_pc = pc;

    for (auto it = begin; it != end; ++it)
    {
        const std::smatch& match = *it;
        if (match.size() == 2)
        {
            auto bb_content = content.substr(pos, match.position() - pos);
            auto loc_instrs = parseInstructions(bb_content);
            func->basic_blocks_[bb_name] = local_pc;
            instrs.insert(instrs.end(), loc_instrs.begin(), loc_instrs.end());
            local_pc += loc_instrs.size();
            pos = match.position();
            bb_name = match[1].str();
        }
        else
        {
            throw std::runtime_error("Basic block is not matched");
        }
    }
    auto bb_content = content.substr(pos, content.size() - pos - 1);
    auto loc_instrs = parseInstructions(bb_content);
    func->basic_blocks_[bb_name] = local_pc;
    instrs.insert(instrs.end(), loc_instrs.begin(), loc_instrs.end());
    return {func, instrs};
}

bool Function::isEntry() const
{
    bool fl = false;
    for (const auto& attr : attrs_)
    {
        if (attr == FuncAttr::Visible)
        {
            fl = true;
        }
    }
    return fl && (type_ == FuncType::Entry);
}

void Function::Dump()
{
    std::cout << pc_ << "\t" << name_ << ":\n";
    for (auto& [bb_name, bb_pc] : basic_blocks_)
    {
        std::cout << "\t" << bb_pc << "\t" << bb_name << ":\n";
    }
}

uint64_t Function::getOffset() const
{
    return pc_;
}

uint64_t Function::GetBasicBlockOffset(const std::string& bb_name) const
{
    return basic_blocks_.at(bb_name);
}

std::unordered_map<std::string, FunctionParameter> Function::getParameters() const
{
    return params_;
}

} // namespace Ptx
} // namespace Emulator
