#pragma once

#include "ptx_types.h"
#include "warp_context.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <tuple>
#include <utility>

// NOLINTBEGIN

namespace
{

template <typename E>
constexpr std::size_t enumCount = static_cast<std::size_t>(E::COUNT);

template <typename... Enums>
struct EnumTable
{
    static constexpr std::size_t total = (enumCount<Enums> * ...);

    template <std::size_t I, std::size_t K>
    static constexpr auto decode()
    {
        constexpr std::array<std::size_t, sizeof...(Enums)> sz{enumCount<Enums>...};
        std::size_t stride = 1;
        for (std::size_t i = K + 1; i < sizeof...(Enums); ++i)
            stride *= sz[i];
        return static_cast<std::tuple_element_t<K, std::tuple<Enums...>>>((I / stride) % sz[K]);
    }

    static std::size_t encode(Enums... vals)
    {
        std::size_t result = 0;
        ((result = result * enumCount<Enums> + static_cast<std::size_t>(vals)), ...);
        return result;
    }
};

} // namespace

namespace Emulator
{
namespace Ptx
{

class Instruction
{
  public:
    Instruction() = default;
    virtual void Execute(std::shared_ptr<WarpContext>& wc) = 0;
    virtual void Dump() = 0;
};

class regInstruction : public Instruction
{
  private:
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    dataType data_ = dataType();
    registerType reg_ = registerType();
    uint32_t count_ = uint32_t();

  public:
    regInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<regInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class sharedInstruction : public Instruction
{
  private:
    void ExecuteWarp(std::shared_ptr<WarpContext>& wc);

  public:
    uint32_t align_ = uint32_t();
    dataType data_ = dataType();
    symbolOperand symbol_ = symbolOperand();
    uint32_t count_ = uint32_t();

  public:
    sharedInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<sharedInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class pragmaInstruction : public Instruction
{
  private:
    void ExecuteWarp(std::shared_ptr<WarpContext>& wc);

  public:
    pragmavalQl val_ = pragmavalQl();

  public:
    pragmaInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<pragmaInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class absInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localAbsDataType data_ = localAbsDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();

  public:
    absInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<absInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class cvtaInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    bool to_ = bool();
    cvtaspaceQl space_ = cvtaspaceQl();
    localCvtaDataType data_ = localCvtaDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();

  public:
    cvtaInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<cvtaInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class setpInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    setpcmpQl cmp_ = setpcmpQl();
    setpboolQl bool_ = setpboolQl();
    localSetpDataType data_ = localSetpDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    registerOperand src2_ = registerOperand();
    immediateOperand imm_ = immediateOperand();

  public:
    setpInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<setpInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class addInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localAddDataType data_ = localAddDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    registerOperand src2_ = registerOperand();
    immediateOperand imm_ = immediateOperand();

  public:
    addInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<addInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class movInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localMovDataType data_ = localMovDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();
    sprRegisterOperand spr_ = sprRegisterOperand();
    immediateOperand imm_ = immediateOperand();
    symbolOperand symbol_ = symbolOperand();

  public:
    movInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<movInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class shlInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localShlDataType data_ = localShlDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();
    immediateOperand imm_ = immediateOperand();

  public:
    shlInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<shlInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class andInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localAndDataType data_ = localAndDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    registerOperand src2_ = registerOperand();
    immediateOperand imm_ = immediateOperand();

  public:
    andInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<andInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class mulInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    mulmodeQl mode_ = mulmodeQl();
    localMulDataType data_ = localMulDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    registerOperand src2_ = registerOperand();
    immediateOperand imm_ = immediateOperand();

  public:
    mulInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<mulInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class ex2Instruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    bool ftz_ = bool();
    localEx2DataType data_ = localEx2DataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();

  public:
    ex2Instruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<ex2Instruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class barInstruction : public Instruction
{
  private:
    void ExecuteWarp(std::shared_ptr<WarpContext>& wc);

  public:
    bool cta_ = bool();
    barmodeQl mode_ = barmodeQl();
    uint32_t id_ = uint32_t();

  public:
    barInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<barInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class stInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    stspaceQl space_ = stspaceQl();
    localStDataType data_ = localStDataType();
    addressOperand addr_ = addressOperand();
    registerOperand src_ = registerOperand();

  public:
    stInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<stInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class fmaInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    fmamodeQl mode_ = fmamodeQl();
    localFmaDataType data_ = localFmaDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    registerOperand src2_ = registerOperand();
    immediateOperand imm1_ = immediateOperand();
    registerOperand src3_ = registerOperand();
    immediateOperand imm2_ = immediateOperand();

  public:
    fmaInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<fmaInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class negInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localNegDataType data_ = localNegDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();
    immediateOperand imm_ = immediateOperand();

  public:
    negInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<negInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class subInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localSubDataType data_ = localSubDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    registerOperand src2_ = registerOperand();
    immediateOperand imm_ = immediateOperand();

  public:
    subInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<subInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class braInstruction : public Instruction
{
  private:
    void ExecuteBranch(std::shared_ptr<WarpContext>& wc);

  public:
    registerOperand prd_ = registerOperand();
    bool uni_ = bool();
    symbolOperand sym_ = symbolOperand();

  public:
    braInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<braInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class labelInstruction : public Instruction
{
  private:
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    symbolOperand sym_ = symbolOperand();

  public:
    labelInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<labelInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class ldInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    ldspaceQl space_ = ldspaceQl();
    localLdDataType data_ = localLdDataType();
    registerOperand dst_ = registerOperand();
    addressOperand addr_ = addressOperand();

  public:
    ldInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<ldInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class rcpInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    bool ftz_ = bool();
    localRcpDataType data_ = localRcpDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();

  public:
    rcpInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<rcpInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class tanhInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    bool ftz_ = bool();
    localTanhDataType data_ = localTanhDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();

  public:
    tanhInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<tanhInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class madInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    madmodeQl mode_ = madmodeQl();
    localMadDataType data_ = localMadDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    registerOperand src2_ = registerOperand();
    registerOperand src3_ = registerOperand();

  public:
    madInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<madInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class copysignInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localCopysignDataType data_ = localCopysignDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    registerOperand src2_ = registerOperand();
    immediateOperand imm_ = immediateOperand();

  public:
    copysignInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<copysignInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class retInstruction : public Instruction
{
  private:
    void ExecuteBranch(std::shared_ptr<WarpContext>& wc);

  public:
  public:
    retInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<retInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class selpInstruction : public Instruction
{
  private:
    template <dataType Data>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    localSelpDataType data_ = localSelpDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src1_ = registerOperand();
    immediateOperand imm1_ = immediateOperand();
    registerOperand src2_ = registerOperand();
    immediateOperand imm2_ = immediateOperand();
    registerOperand src3_ = registerOperand();

  public:
    selpInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<selpInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

class cvtInstruction : public Instruction
{
  private:
    template <dataType SrcData, dataType DstData>
    void ExecuteThread(uint32_t lid, std::shared_ptr<WarpContext>& wc);

  public:
    cvtmodeQl mode_ = cvtmodeQl();
    localCvtSrcDataType src_data_ = localCvtSrcDataType();
    localCvtDstDataType dst_data_ = localCvtDstDataType();
    registerOperand dst_ = registerOperand();
    registerOperand src_ = registerOperand();

  public:
    cvtInstruction(){};
    void Execute(std::shared_ptr<WarpContext>& wc) override;
    void Dump() override;

    static std::shared_ptr<cvtInstruction> Make(const std::string& line);
    friend std::shared_ptr<Instruction> makeInstruction(const std::string& line);
};

std::shared_ptr<Instruction> makeInstruction(const std::string& name, const std::string& line);

} // namespace Ptx
} // namespace Emulator

// NOLINTEND