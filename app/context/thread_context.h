#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace Emulator {
namespace Ptx {
class Module;
enum class RegType;
template <RegType>
struct Btype;
enum class SprReg;
} // namespace Ptx

namespace Context {

class WarpContext;

template <Ptx::RegType RegType>
class RegisterOwner
{
  private:
    using BType = typename Ptx::Btype<RegType>::type;

    std::vector<BType> regs_;

  public:
    BType GetValue(uint32_t id) const;
    void SetValue(std::uint32_t id, BType val);
};

class ThreadContext
{
  private:
    std::shared_ptr<WarpContext> warp_context_;

    std::shared_ptr<Ptx::Module> ptx_module_;

    std::map<Ptx::SprReg, std::uint64_t> spr_regs_;

    std::map<Ptx::RegType, RegisterOwner> reg_owners_;

  public:
    ThreadContext() = default;
    ~ThreadContext() = default;

    template <typename T>
    T SharedLoad(std::uint64_t address);

    template <typename T>
    void SharedStore(std::uint64_t address, T value);

    template <typename T>
    T GlobalLoad(std::uint64_t address);

    template <typename T>
    void GlobalStore(std::uint64_t address, T value);

    template <Ptx::RegType RegType>
    typename Ptx::Btype<RegType>::type GetRegister(std::uint32_t id);

    template <Ptx::RegType RegType>
    void SetRegister(std::uint32_t id, typename Ptx::Btype<RegType>::type value);
};

} // namespace Context
} // namespace Emulator
