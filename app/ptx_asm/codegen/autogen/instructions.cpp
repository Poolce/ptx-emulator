#include "instructions.h"

#include "constant.h"
#include "utils.h"

// NOLINTBEGIN

namespace Emulator
{
namespace Ptx
{

void regInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute reg\n";
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            ExecuteThread(lid, wc);
        }
    }
    wc->pc++;
}

void regInstruction::Dump()
{
    std::cout << "\t\t" << "reg" << "\n";
}

std::shared_ptr<regInstruction> regInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(\.reg\s\.([fsub]8|[fsub]16|[fsub]32|[fsub]64|pred)\s%(f|fd|p|r|rd|spr)<([0-9]+)>;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<regInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<dataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->reg_ = FromString<registerType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->count_ = FromString<uint32_t>(match[3].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction reg is not mutched. [" + line + "]");
    }
    return instr;
}

void sharedInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute shared\n";
    ExecuteWarp(wc);
    wc->pc++;
}

void sharedInstruction::Dump()
{
    std::cout << "\t\t" << "shared" << "\n";
}

std::shared_ptr<sharedInstruction> sharedInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(\.shared\s\.align\s([0-9]+)\s*\.([fsub]8|[fsub]16|[fsub]32|[fsub]64|pred)\s([A-z0-9_]+)\[([0-9]+)\];)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<sharedInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->align_ = FromString<uint32_t>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<dataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->symbol_ = FromString<symbolOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->count_ = FromString<uint32_t>(match[4].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction shared is not mutched. [" + line + "]");
    }
    return instr;
}

void pragmaInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute pragma\n";
    ExecuteWarp(wc);
    wc->pc++;
}

void pragmaInstruction::Dump()
{
    std::cout << "\t\t" << "pragma" << "\n";
}

std::shared_ptr<pragmaInstruction> pragmaInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern = R"ptx(\.pragma\s.(nounroll|used_bytes_mask).;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<pragmaInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->val_ = FromString<pragmavalQl>(match[1].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction pragma is not mutched. [" + line + "]");
    }
    return instr;
}

void absInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute abs\n";
    using ET = EnumTable<localAbsDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](absInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localAbsDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void absInstruction::Dump()
{
    std::cout << "\t\t" << "abs" << "\n";
}

std::shared_ptr<absInstruction> absInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern = R"ptx(abs\.(s32|s64|f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<absInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localAbsDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src_ = FromString<registerOperand>(match[3].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction abs is not mutched. [" + line + "]");
    }
    return instr;
}

void cvtaInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute cvta\n";
    using ET = EnumTable<localCvtaDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](cvtaInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localCvtaDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void cvtaInstruction::Dump()
{
    std::cout << "\t\t" << "cvta" << "\n";
}

std::shared_ptr<cvtaInstruction> cvtaInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(cvta\.(to)?\.?(const|global|local|param|param::entry|shared|shared::cta|shared::cluster)\.(u32|u64)\s(%[a-z]+[0-9]*,?)\s(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<cvtaInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->to_ = FromString<bool>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->space_ = FromString<cvtaspaceQl>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->data_ = FromString<localCvtaDataType>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->src_ = FromString<registerOperand>(match[5].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction cvta is not mutched. [" + line + "]");
    }
    return instr;
}

void setpInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute setp\n";
    using ET = EnumTable<localSetpDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](setpInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localSetpDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void setpInstruction::Dump()
{
    std::cout << "\t\t" << "setp" << "\n";
}

std::shared_ptr<setpInstruction> setpInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(setp\.(eq|ne|lt|le|gt|ge|lo|ls|hi|hs|equ|neu|ltu|leu|gtu|geu|num|nan)?\.?(and|or|xor)?\.?(b16|b32|b64|u16|u32|u64|s16|s32|s64|f32|f64)?\s(%[a-z]+[0-9]*,?)\s(%[a-z]+[0-9]*,?)\s(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<setpInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->cmp_ = FromString<setpcmpQl>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->bool_ = FromString<setpboolQl>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->data_ = FromString<localSetpDataType>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[5].str());
        }
        if (match[6].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[6].str());
        }
        if (match[7].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[7].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction setp is not mutched. [" + line + "]");
    }
    return instr;
}

void addInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute add\n";
    using ET = EnumTable<localAddDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](addInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localAddDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void addInstruction::Dump()
{
    std::cout << "\t\t" << "add" << "\n";
}

std::shared_ptr<addInstruction> addInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(add\.(u16|u32|u64|s16|s32|s64|f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<addInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localAddDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[5].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction add is not mutched. [" + line + "]");
    }
    return instr;
}

void movInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute mov\n";
    using ET = EnumTable<localMovDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](movInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localMovDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void movInstruction::Dump()
{
    std::cout << "\t\t" << "mov" << "\n";
}

std::shared_ptr<movInstruction> movInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(mov\.(pred|b16|b32|b64|u16|u32|u64|s16|s32|s64|f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(%[a-z]+\.?[xyz]?)?(0?x?[\-A-Fa-f0-9]+)?([A-z0-9_]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<movInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localMovDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->spr_ = FromString<sprRegisterOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[5].str());
        }
        if (match[6].matched)
        {
            instr->symbol_ = FromString<symbolOperand>(match[6].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction mov is not mutched. [" + line + "]");
    }
    return instr;
}

void shlInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute shl\n";
    using ET = EnumTable<localShlDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](shlInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localShlDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void shlInstruction::Dump()
{
    std::cout << "\t\t" << "shl" << "\n";
}

std::shared_ptr<shlInstruction> shlInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(shl\.(b16|b32|b64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<shlInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localShlDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[4].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction shl is not mutched. [" + line + "]");
    }
    return instr;
}

void andInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute and\n";
    using ET = EnumTable<localAndDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](andInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localAndDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void andInstruction::Dump()
{
    std::cout << "\t\t" << "and" << "\n";
}

std::shared_ptr<andInstruction> andInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(and\.(pred|b16|b32|b64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<andInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localAndDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[5].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction and is not mutched. [" + line + "]");
    }
    return instr;
}

void mulInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute mul\n";
    using ET = EnumTable<localMulDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](mulInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localMulDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void mulInstruction::Dump()
{
    std::cout << "\t\t" << "mul" << "\n";
}

std::shared_ptr<mulInstruction> mulInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(mul\.?(hi|lo|wide)?\.(f16|f32|f64|u16|u32|u64|s16|s32|s64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<mulInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->mode_ = FromString<mulmodeQl>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<localMulDataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[5].str());
        }
        if (match[6].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[6].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction mul is not mutched. [" + line + "]");
    }
    return instr;
}

void ex2Instruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute ex2\n";
    using ET = EnumTable<localEx2DataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](ex2Instruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localEx2DataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void ex2Instruction::Dump()
{
    std::cout << "\t\t" << "ex2" << "\n";
}

std::shared_ptr<ex2Instruction> ex2Instruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(ex2\.approx(\.ftz)\.(f16|f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<ex2Instruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->ftz_ = FromString<bool>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<localEx2DataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src_ = FromString<registerOperand>(match[4].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction ex2 is not mutched. [" + line + "]");
    }
    return instr;
}

void barInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute bar\n";
    ExecuteWarp(wc);
    wc->pc++;
}

void barInstruction::Dump()
{
    std::cout << "\t\t" << "bar" << "\n";
}

std::shared_ptr<barInstruction> barInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern = R"ptx(bar(\.cta)?\.(sync|arrive)\s*([0-9]+);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<barInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->cta_ = FromString<bool>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->mode_ = FromString<barmodeQl>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->id_ = FromString<uint32_t>(match[3].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction bar is not mutched. [" + line + "]");
    }
    return instr;
}

void stInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute st\n";
    using ET = EnumTable<localStDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](stInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localStDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void stInstruction::Dump()
{
    std::cout << "\t\t" << "st" << "\n";
}

std::shared_ptr<stInstruction> stInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(st\.(const|global|local|param|param::entry|param::func|shared|shared::cta|shared::cluster)?\.(b8|b16|b32|b64|u8|u16|u32|u64|s8|s16|s32|s64|f32|f64)?\s*([\[\]A-Za-z0-9_\+\-%]+),\s*(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<stInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->space_ = FromString<stspaceQl>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<localStDataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->addr_ = FromString<addressOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src_ = FromString<registerOperand>(match[4].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction st is not mutched. [" + line + "]");
    }
    return instr;
}

void fmaInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute fma\n";
    using ET = EnumTable<localFmaDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](fmaInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localFmaDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void fmaInstruction::Dump()
{
    std::cout << "\t\t" << "fma" << "\n";
}

std::shared_ptr<fmaInstruction> fmaInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(fma\.(rn|rz|rm|rp)\.(f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?,\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<fmaInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->mode_ = FromString<fmamodeQl>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<localFmaDataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[5].str());
        }
        if (match[6].matched)
        {
            instr->imm1_ = FromString<immediateOperand>(match[6].str());
        }
        if (match[7].matched)
        {
            instr->src3_ = FromString<registerOperand>(match[7].str());
        }
        if (match[8].matched)
        {
            instr->imm2_ = FromString<immediateOperand>(match[8].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction fma is not mutched. [" + line + "]");
    }
    return instr;
}

void negInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute neg\n";
    using ET = EnumTable<localNegDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](negInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localNegDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void negInstruction::Dump()
{
    std::cout << "\t\t" << "neg" << "\n";
}

std::shared_ptr<negInstruction> negInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(neg\.(s16|s32|s64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<negInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localNegDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[4].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction neg is not mutched. [" + line + "]");
    }
    return instr;
}

void subInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute sub\n";
    using ET = EnumTable<localSubDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](subInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localSubDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void subInstruction::Dump()
{
    std::cout << "\t\t" << "sub" << "\n";
}

std::shared_ptr<subInstruction> subInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(sub\.(f32|s64|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<subInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localSubDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[5].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction sub is not mutched. [" + line + "]");
    }
    return instr;
}

void braInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute bra\n";
    ExecuteBranch(wc);
}

void braInstruction::Dump()
{
    std::cout << "\t\t" << "bra" << "\n";
}

std::shared_ptr<braInstruction> braInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern = R"ptx(@?(%[a-z]+[0-9]*,?)?\s*bra(\.uni)?\s*\$([A-z0-9_]+);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<braInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->prd_ = FromString<registerOperand>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->uni_ = FromString<bool>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->sym_ = FromString<symbolOperand>(match[3].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction bra is not mutched. [" + line + "]");
    }
    return instr;
}

void labelInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute label\n";
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            ExecuteThread(lid, wc);
        }
    }
    wc->pc++;
}

void labelInstruction::Dump()
{
    std::cout << "\t\t" << "label" << "\n";
}

std::shared_ptr<labelInstruction> labelInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern = R"ptx(\$([A-z0-9_]+):)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<labelInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->sym_ = FromString<symbolOperand>(match[1].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction label is not mutched. [" + line + "]");
    }
    return instr;
}

void ldInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute ld\n";
    using ET = EnumTable<localLdDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](ldInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localLdDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void ldInstruction::Dump()
{
    std::cout << "\t\t" << "ld" << "\n";
}

std::shared_ptr<ldInstruction> ldInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(ld\.(const|global|local|param|param::entry|param::func|shared|shared::cta|shared::cluster)?\.(b8|b16|b32|b64|u8|u16|u32|u64|s8|s16|s32|s64|f32|f64)?\s*(%[a-z]+[0-9]*,?),\s*([\[\]A-Za-z0-9_\+\-%]+);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<ldInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->space_ = FromString<ldspaceQl>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<localLdDataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->addr_ = FromString<addressOperand>(match[4].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction ld is not mutched. [" + line + "]");
    }
    return instr;
}

void rcpInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute rcp\n";
    using ET = EnumTable<localRcpDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](rcpInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localRcpDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void rcpInstruction::Dump()
{
    std::cout << "\t\t" << "rcp" << "\n";
}

std::shared_ptr<rcpInstruction> rcpInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(rcp\.approx(\.ftz)\.(f16|f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<rcpInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->ftz_ = FromString<bool>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<localRcpDataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src_ = FromString<registerOperand>(match[4].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction rcp is not mutched. [" + line + "]");
    }
    return instr;
}

void tanhInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute tanh\n";
    using ET = EnumTable<localTanhDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](tanhInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localTanhDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void tanhInstruction::Dump()
{
    std::cout << "\t\t" << "tanh" << "\n";
}

std::shared_ptr<tanhInstruction> tanhInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(tanh\.approx(\.ftz)\.(f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<tanhInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->ftz_ = FromString<bool>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<localTanhDataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src_ = FromString<registerOperand>(match[4].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction tanh is not mutched. [" + line + "]");
    }
    return instr;
}

void madInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute mad\n";
    using ET = EnumTable<localMadDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](madInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localMadDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void madInstruction::Dump()
{
    std::cout << "\t\t" << "mad" << "\n";
}

std::shared_ptr<madInstruction> madInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(mad\.(hi|lo|wide)\.(u16|u32|u64|s16|s32|s64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<madInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->mode_ = FromString<madmodeQl>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->data_ = FromString<localMadDataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[5].str());
        }
        if (match[6].matched)
        {
            instr->src3_ = FromString<registerOperand>(match[6].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction mad is not mutched. [" + line + "]");
    }
    return instr;
}

void copysignInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute copysign\n";
    using ET = EnumTable<localCopysignDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](copysignInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localCopysignDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void copysignInstruction::Dump()
{
    std::cout << "\t\t" << "copysign" << "\n";
}

std::shared_ptr<copysignInstruction> copysignInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(copysign\.(f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<copysignInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localCopysignDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->imm_ = FromString<immediateOperand>(match[5].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction copysign is not mutched. [" + line + "]");
    }
    return instr;
}

void retInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute ret\n";
    ExecuteBranch(wc);
}

void retInstruction::Dump()
{
    std::cout << "\t\t" << "ret" << "\n";
}

std::shared_ptr<retInstruction> retInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern = R"ptx(ret;)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<retInstruction>();
    if (std::regex_match(line, match, re))
    {
    }
    else
    {
        throw std::runtime_error("Instruction ret is not mutched. [" + line + "]");
    }
    return instr;
}

void selpInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute selp\n";
    using ET = EnumTable<localSelpDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](selpInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localSelpDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void selpInstruction::Dump()
{
    std::cout << "\t\t" << "selp" << "\n";
}

std::shared_ptr<selpInstruction> selpInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(selp\.(b16|b32|b64|u16|u32|u64|s16|s32|s64|f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?,\s*(%[a-z]+[0-9]*,?)?(0?x?[\-A-Fa-f0-9]+)?,\s*(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<selpInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->data_ = FromString<localSelpDataType>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->src1_ = FromString<registerOperand>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->imm1_ = FromString<immediateOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->src2_ = FromString<registerOperand>(match[5].str());
        }
        if (match[6].matched)
        {
            instr->imm2_ = FromString<immediateOperand>(match[6].str());
        }
        if (match[7].matched)
        {
            instr->src3_ = FromString<registerOperand>(match[7].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction selp is not mutched. [" + line + "]");
    }
    return instr;
}

void cvtInstruction::Execute(std::shared_ptr<WarpContext>& wc)
{
    std::cout << "Execute cvt\n";
    using ET = EnumTable<localCvtSrcDataType, localCvtDstDataType>;

    static constexpr auto Table = []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        return std::array {
            (
                []<std::size_t I>()
                {
                    return +[](cvtInstruction* self, uint32_t lid, std::shared_ptr<WarpContext>& wc)
                    {
                        [&]<std::size_t... Ks>(std::index_sequence<Ks...>) {
                            self->ExecuteThread<GetParent(ET::decode<I, Ks>())...>(lid, wc);
                        }(std::index_sequence_for<localCvtSrcDataType, localCvtDstDataType>{});
                    };
                }.template operator()<Is>())...
        };
    }(std::make_index_sequence<ET::total>{});

    auto idx = ET::encode(src_data_, dst_data_);
    for (uint32_t lid = 0; lid < Emulator::WarpSize; lid++)
    {
        const bool prd = ((wc->execution_mask >> lid) & 1U) != 0U;
        if (prd)
        {
            Table[idx](this, lid, wc);
        }
    }
    wc->pc++;
}

void cvtInstruction::Dump()
{
    std::cout << "\t\t" << "cvt" << "\n";
}

std::shared_ptr<cvtInstruction> cvtInstruction::Make(const std::string& line)
{
    constexpr std::string_view Pattern =
        R"ptx(cvt\.?(rtz|rm|rz|rn|rp)?\.(u8|u16|u32|u64|s8|s16|s32|s64|f16|f32|f64)\.(u8|u16|u32|u64|s8|s16|s32|s64|f16|f32|f64)\s*(%[a-z]+[0-9]*,?),\s*(%[a-z]+[0-9]*,?);)ptx";
    static std::regex re(Pattern.data(), std::regex::ECMAScript | std::regex::optimize);
    std::smatch match;
    auto instr = std::make_shared<cvtInstruction>();
    if (std::regex_match(line, match, re))
    {
        if (match[1].matched)
        {
            instr->mode_ = FromString<cvtmodeQl>(match[1].str());
        }
        if (match[2].matched)
        {
            instr->src_data_ = FromString<localCvtSrcDataType>(match[2].str());
        }
        if (match[3].matched)
        {
            instr->dst_data_ = FromString<localCvtDstDataType>(match[3].str());
        }
        if (match[4].matched)
        {
            instr->dst_ = FromString<registerOperand>(match[4].str());
        }
        if (match[5].matched)
        {
            instr->src_ = FromString<registerOperand>(match[5].str());
        }
    }
    else
    {
        throw std::runtime_error("Instruction cvt is not mutched. [" + line + "]");
    }
    return instr;
}

std::shared_ptr<Instruction> makeInstruction(const std::string& name, const std::string& line)
{
    if (name == "reg")
    {
        return regInstruction::Make(line);
    }
    if (name == "shared")
    {
        return sharedInstruction::Make(line);
    }
    if (name == "pragma")
    {
        return pragmaInstruction::Make(line);
    }
    if (name == "abs")
    {
        return absInstruction::Make(line);
    }
    if (name == "cvta")
    {
        return cvtaInstruction::Make(line);
    }
    if (name == "setp")
    {
        return setpInstruction::Make(line);
    }
    if (name == "add")
    {
        return addInstruction::Make(line);
    }
    if (name == "mov")
    {
        return movInstruction::Make(line);
    }
    if (name == "shl")
    {
        return shlInstruction::Make(line);
    }
    if (name == "and")
    {
        return andInstruction::Make(line);
    }
    if (name == "mul")
    {
        return mulInstruction::Make(line);
    }
    if (name == "ex2")
    {
        return ex2Instruction::Make(line);
    }
    if (name == "bar")
    {
        return barInstruction::Make(line);
    }
    if (name == "st")
    {
        return stInstruction::Make(line);
    }
    if (name == "fma")
    {
        return fmaInstruction::Make(line);
    }
    if (name == "neg")
    {
        return negInstruction::Make(line);
    }
    if (name == "sub")
    {
        return subInstruction::Make(line);
    }
    if (name == "bra")
    {
        return braInstruction::Make(line);
    }
    if (name == "label")
    {
        return labelInstruction::Make(line);
    }
    if (name == "ld")
    {
        return ldInstruction::Make(line);
    }
    if (name == "rcp")
    {
        return rcpInstruction::Make(line);
    }
    if (name == "tanh")
    {
        return tanhInstruction::Make(line);
    }
    if (name == "mad")
    {
        return madInstruction::Make(line);
    }
    if (name == "copysign")
    {
        return copysignInstruction::Make(line);
    }
    if (name == "ret")
    {
        return retInstruction::Make(line);
    }
    if (name == "selp")
    {
        return selpInstruction::Make(line);
    }
    if (name == "cvt")
    {
        return cvtInstruction::Make(line);
    }
    throw std::runtime_error("Undefined instruction [" + line + "]");
}

} // namespace Ptx
} // namespace Emulator

// NOLINTEND

#include "executors.tpp"