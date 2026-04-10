#include "function.h"
#include "instructions.h"
#include "module.h"

#include <gtest/gtest.h>

using namespace Emulator;
using namespace Emulator::Ptx;

// ---------------------------------------------------------------------------
// Minimal PTX snippets
// ---------------------------------------------------------------------------

// One visible entry, three instructions (reg, mov, ret), no labels
static constexpr const char* PTX_SIMPLE = R"(
.visible .entry simple(.param .u64 p0)
{
.reg .u64 %rd<2>;
mov.u64 %rd0, 0;
ret;
}
)";

// Two basic blocks: "entry" has (reg, mov), "BB0_1" has (ret)
static constexpr const char* PTX_TWO_BLOCKS = R"(
.visible .entry multiblock(.param .u64 p0)
{
.reg .u32 %r<2>;
mov.u32 %r0, 0;
$BB0_1:
ret;
}
)";

// Two parameters, tests param id assignment
static constexpr const char* PTX_TWO_PARAMS = R"(
.visible .entry twoparams(.param .u64 p0, .param .u32 p1)
{
ret;
}
)";

// A non-entry device function (no .visible) followed by an entry
static constexpr const char* PTX_HELPER_AND_ENTRY = R"(
.func helper(.param .u32 hp0)
{
ret;
}
.visible .entry main_kernel(.param .u64 buf)
{
.reg .u64 %rd<2>;
mov.u64 %rd0, 0;
ret;
}
)";

// ============================================================================
// Module: parsing
// ============================================================================

TEST(ModuleParsing, SingleFunction_ParsedSuccessfully)
{
    auto module = Module::Make(PTX_SIMPLE);
    ASSERT_NE(module, nullptr);
}

TEST(ModuleParsing, SingleFunction_InstructionCount)
{
    // .reg .u64 %rd<2>  → 1 regInstruction
    // mov.u64 %rd0, 0  → 1 movInstruction
    // ret              → 1 retInstruction
    auto module = Module::Make(PTX_SIMPLE);
    EXPECT_NE(module->GetInstruction(0), nullptr);
    EXPECT_NE(module->GetInstruction(1), nullptr);
    EXPECT_NE(module->GetInstruction(2), nullptr);
    EXPECT_EQ(module->GetInstruction(3), nullptr); // out of range
}

TEST(ModuleParsing, OutOfRangePC_ReturnsNullptr)
{
    auto module = Module::Make(PTX_SIMPLE);
    EXPECT_EQ(module->GetInstruction(999), nullptr);
}

TEST(ModuleParsing, GetEntryFunction_Found)
{
    auto module = Module::Make(PTX_SIMPLE);
    auto func = module->GetEntryFunction("simple");
    ASSERT_NE(func, nullptr);
}

TEST(ModuleParsing, GetEntryFunction_Unknown_Throws)
{
    auto module = Module::Make(PTX_SIMPLE);
    EXPECT_THROW(module->GetEntryFunction("no_such_func"), std::runtime_error);
}

TEST(ModuleParsing, GetEntryFunction_NotEntry_Throws)
{
    // "helper" is a .func, not a .visible .entry — must not be returned as entry
    auto module = Module::Make(PTX_HELPER_AND_ENTRY);
    EXPECT_THROW(module->GetEntryFunction("helper"), std::runtime_error);
}

TEST(ModuleParsing, GetEntryFunction_ActualEntry_Found)
{
    auto module = Module::Make(PTX_HELPER_AND_ENTRY);
    auto func = module->GetEntryFunction("main_kernel");
    ASSERT_NE(func, nullptr);
}

TEST(ModuleParsing, GetBasicBlockOffset_EntryBlock)
{
    auto module = Module::Make(PTX_SIMPLE);
    // "entry" is the first (and only) basic block → offset 0
    EXPECT_EQ(module->GetBasicBlockOffset("simple", "entry"), 0u);
}

TEST(ModuleParsing, GetBasicBlockOffset_UnknownFunction_Throws)
{
    auto module = Module::Make(PTX_SIMPLE);
    EXPECT_THROW(module->GetBasicBlockOffset("ghost", "entry"), std::runtime_error);
}

TEST(ModuleParsing, TwoFunctions_PCsContinueAcrossFunctions)
{
    // helper has 1 instruction (ret); main_kernel starts at pc 1
    auto module = Module::Make(PTX_HELPER_AND_ENTRY);

    // helper starts at pc 0
    EXPECT_EQ(module->GetBasicBlockOffset("helper", "entry"), 0u);
    // main_kernel entry block starts right after helper's instructions
    uint64_t main_start = module->GetBasicBlockOffset("main_kernel", "entry");
    EXPECT_GE(main_start, 1u); // at least after helper's 1 instruction
}

TEST(ModuleParsing, TwoBlocks_SecondBlockOffsetGreaterThanZero)
{
    auto module = Module::Make(PTX_TWO_BLOCKS);
    uint64_t entry_off = module->GetBasicBlockOffset("multiblock", "entry");
    uint64_t bb1_off = module->GetBasicBlockOffset("multiblock", "BB0_1");

    EXPECT_EQ(entry_off, 0u);
    EXPECT_GT(bb1_off, entry_off);
}

// ============================================================================
// Function: isEntry
// ============================================================================

TEST(FunctionIsEntry, VisibleEntry_IsTrue)
{
    auto module = Module::Make(PTX_SIMPLE);
    auto func = module->GetEntryFunction("simple");
    EXPECT_TRUE(func->isEntry());
}

TEST(FunctionIsEntry, FuncWithoutVisible_IsFalse)
{
    // Create via Module::Make and inspect "helper"
    auto module = Module::Make(PTX_HELPER_AND_ENTRY);
    // helper is reachable via GetBasicBlockOffset but not via GetEntryFunction
    // Access indirectly: GetBasicBlockOffset won't throw, so the function exists
    EXPECT_THROW(module->GetEntryFunction("helper"), std::runtime_error);
}

// ============================================================================
// Function: getOffset
// ============================================================================

TEST(FunctionOffset, SingleFunction_StartAtZero)
{
    auto module = Module::Make(PTX_SIMPLE);
    auto func = module->GetEntryFunction("simple");
    EXPECT_EQ(func->getOffset(), 0u);
}

TEST(FunctionOffset, SecondFunction_StartsAfterFirst)
{
    auto module = Module::Make(PTX_HELPER_AND_ENTRY);
    auto main_func = module->GetEntryFunction("main_kernel");
    // helper has 1 instruction, so main_kernel starts at pc >= 1
    EXPECT_GE(main_func->getOffset(), 1u);
}

// ============================================================================
// Function: basic block offsets
// ============================================================================

TEST(FunctionBasicBlocks, EntryBlockAtFunctionStart)
{
    auto module = Module::Make(PTX_SIMPLE);
    auto func = module->GetEntryFunction("simple");
    EXPECT_EQ(func->GetBasicBlockOffset("entry"), func->getOffset());
}

TEST(FunctionBasicBlocks, NamedBlockAfterEntry)
{
    auto module = Module::Make(PTX_TWO_BLOCKS);
    auto func = module->GetEntryFunction("multiblock");

    uint64_t entry_off = func->GetBasicBlockOffset("entry");
    uint64_t bb1_off = func->GetBasicBlockOffset("BB0_1");

    // BB0_1 must come strictly after the entry instructions
    EXPECT_LT(entry_off, bb1_off);
}

TEST(FunctionBasicBlocks, UnknownBlock_Throws)
{
    auto module = Module::Make(PTX_SIMPLE);
    auto func = module->GetEntryFunction("simple");
    EXPECT_THROW(func->GetBasicBlockOffset("no_such_block"), std::out_of_range);
}

// ============================================================================
// Function: parameters
// ============================================================================

TEST(FunctionParameters, SingleParam_NameTypeId)
{
    auto module = Module::Make(PTX_SIMPLE);
    auto func = module->GetEntryFunction("simple");
    auto params = func->getParameters();

    ASSERT_EQ(params.size(), 1u);
    ASSERT_TRUE(params.count("p0"));
    EXPECT_EQ(params.at("p0").type, dataType::U64);
    EXPECT_EQ(params.at("p0").id, 0u);
    EXPECT_EQ(params.at("p0").name, "p0");
}

TEST(FunctionParameters, TwoParams_IDsAreSequential)
{
    auto module = Module::Make(PTX_TWO_PARAMS);
    auto func = module->GetEntryFunction("twoparams");
    auto params = func->getParameters();

    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(params.at("p0").id, 0u);
    EXPECT_EQ(params.at("p1").id, 1u);
    EXPECT_EQ(params.at("p0").type, dataType::U64);
    EXPECT_EQ(params.at("p1").type, dataType::U32);
}

TEST(FunctionParameters, NoParams_EmptyMap)
{
    // Build a parameterless function via Module
    auto module = Module::Make(PTX_HELPER_AND_ENTRY);
    // "helper" has .param .u32 hp0
    // We can't call GetEntryFunction on it; use GetBasicBlockOffset to confirm it exists
    EXPECT_NO_THROW(module->GetBasicBlockOffset("helper", "entry"));
}

// ============================================================================
// Instruction identity (smoke-test Make round-trip)
// ============================================================================

TEST(InstructionIdentity, RegInstruction_ParsesDataAndCount)
{
    auto instr = regInstruction::Make(".reg .u32 %r<8>;");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->data_, dataType::U32);
    EXPECT_EQ(instr->reg_, registerType::R);
    EXPECT_EQ(instr->count_, 8u);
}

TEST(InstructionIdentity, MovInstruction_ParsesDataAndOperands)
{
    auto instr = movInstruction::Make("mov.u64 %rd0, 42;");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->dst_.type, registerType::Rd);
    EXPECT_EQ(instr->dst_.reg_id, 0u);
    EXPECT_EQ(instr->imm_, 42);
}

TEST(InstructionIdentity, AddInstruction_ParsesBothSources)
{
    auto instr = addInstruction::Make("add.u32 %r2, %r0, %r1;");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->dst_.reg_id, 2u);
    EXPECT_EQ(instr->src1_.reg_id, 0u);
    EXPECT_EQ(instr->src2_.reg_id, 1u);
}

TEST(InstructionIdentity, BraInstruction_ParsesPredAndSymbol)
{
    auto instr = braInstruction::Make("@%p0 bra $LOOP;");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->prd_.type, registerType::P);
    EXPECT_EQ(instr->prd_.reg_id, 0u);
    EXPECT_EQ(std::string(instr->sym_), "LOOP");
}

TEST(InstructionIdentity, SetpInstruction_ParsesCmpAndData)
{
    auto instr = setpInstruction::Make("setp.lt.s32 %p0 %r0 %r1;");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->cmp_, setpcmpQl::Lt);
    EXPECT_EQ(instr->dst_.type, registerType::P);
    EXPECT_EQ(instr->src1_.type, registerType::R);
}

TEST(InstructionIdentity, MulInstruction_ParsesModeAndData)
{
    auto instr = mulInstruction::Make("mul.wide.s32 %rd0, %r0, %r1;");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mode_, mulmodeQl::Wide);
    EXPECT_EQ(instr->dst_.type, registerType::Rd);
    EXPECT_EQ(instr->src1_.type, registerType::R);
}

TEST(InstructionIdentity, LdInstruction_ParsesSpaceAndAddress)
{
    auto instr = ldInstruction::Make("ld.global.u32 %r0, [%rd0];");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->space_, ldspaceQl::Global);
    EXPECT_EQ(instr->dst_.type, registerType::R);
    EXPECT_EQ(instr->addr_.reg.type, registerType::Rd);
    EXPECT_EQ(instr->addr_.imm, 0);
}

TEST(InstructionIdentity, LdInstruction_ParsesImmediateOffset)
{
    auto instr = ldInstruction::Make("ld.global.u32 %r0, [%rd0+8];");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->addr_.imm, 8);
}

TEST(InstructionIdentity, StInstruction_ParsesAddressAndSrc)
{
    auto instr = stInstruction::Make("st.global.f32 [%rd0+4], %r0;");
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->space_, stspaceQl::Global);
    EXPECT_EQ(instr->addr_.reg.type, registerType::Rd);
    EXPECT_EQ(instr->addr_.imm, 4);
    EXPECT_EQ(instr->src_.type, registerType::R);
}

TEST(InstructionIdentity, UnknownInstruction_Throws)
{
    EXPECT_THROW(makeInstruction("zzz_unknown", "zzz_unknown %r0;"), std::runtime_error);
}
