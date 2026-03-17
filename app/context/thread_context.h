#pragma once

namespace Emulator
{
namespace Context
{

template <Ptx::RegType RegType>
class RegisterOwner
{
  private:
    using BType = Ptx::Btype<RegType>::type;

    std::vector<BType> regs_;

  public:
    BType GetValue(uint32_t id) const;
    SetValue(uint32_t id, BType val);
};

class ThreadContext
{
  private:
    std::shared_ptr<WarpContext> warp_context_;

    std::shared_ptr<ptx::Module> ptx_module_;

  private:
    std::map<Ptx::SprReg, uint64_t> spr_regs_;

    std::map<Ptx::RegType, RegisterOwner> reg_owners_;

  public:
    BlockContext() = default;
    ~BlockContext() = default;

  public:
    template <typename T>
    T SharedLoad(uint64_t address);

    template <typename T>
    void SharedStore(uint64_t address, T value);

    template <typename T>
    T GlobalLoad(uint64_t address);

    template <typename T>
    void GlobalStore(uint64_t address, T value);

    template <Ptx::RegType RegType_, Ptx::Btype<RegType>::type btype_>
    btype_ GetRegister(uint32_t id);

    template <Ptx::RegType RegType_, Ptx::Btype<RegType>::type btype_>
    void SetRegister(uint32_t id, btype_ value);
};

} // namespace Context
} // namespace Emulator
