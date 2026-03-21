#include "instructions.h"
#include "module.h"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

namespace
{

class CoutCapture
{
  public:
    CoutCapture() : old_(std::cout.rdbuf(buf_.rdbuf())) {}
    ~CoutCapture() { std::cout.rdbuf(old_); }
    std::string Str() const { return buf_.str(); }

  private:
    std::ostringstream buf_;
    std::streambuf* old_;
};

} // namespace

TEST(PtxInstructionFactory, ThrowsOnUnknownInstruction)
{
    EXPECT_THROW((void)Emulator::Ptx::makeInstruction("not_an_instr", "not_an_instr;"), std::runtime_error);
}

TEST(PtxModule, MakeAndDumpParsesEntryFunction)
{
    const std::string ptx = R"(
.entry foo( )
{
add.s32 %r1, %r2, %r3;
}
)";

    auto m = Emulator::Ptx::Module::Make(ptx);

    CoutCapture cap;
    m->Dump();
    const auto out = cap.Str();

    EXPECT_NE(out.find("foo"), std::string::npos);
    EXPECT_NE(out.find("entry"), std::string::npos); // basic block name
    EXPECT_NE(out.find("add"), std::string::npos);
}
