#include "basic_block.h"

#include <gtest/gtest.h>

#include <sstream>

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

TEST(PtxBasicBlock, DumpContainsNameAndInstruction)
{
    auto bb = Emulator::Ptx::BasicBlock::Make("entry", "add.s32 %r1, %r2, %r3;\n");

    CoutCapture cap;
    bb->Dump();
    const auto out = cap.Str();

    EXPECT_NE(out.find("entry"), std::string::npos);
    EXPECT_NE(out.find("add"), std::string::npos);
}
