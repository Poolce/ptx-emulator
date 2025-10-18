namespace Emulator {
namespace Ptx {

enum class RegType {
  p,
  rb,
  rs8,
  ru8,
  r16,
  rs16,
  ru16,
  r,
  rs32,
  ru32,
  rd,
  rs64,
  ru64
};

template <RegType rtype>
struct Btype<rtype> {};

template <>
struct Btype<RegType::p> {
  using type = bool;
};

template <>
struct Btype<RegType::rb> {
  using type = uint8_t;
};

template <>
struct Btype<RegType::rs8> {
  using type = int8_t;
};

template <>
struct Btype<RegType::ru8> {
  using type = uint8_t;
};

template <>
struct Btype<RegType::r16> {
  using type = uint16_t;
};

template <>
struct Btype<RegType::rs16> {
  using type = int16_t;
};

template <>
struct Btype<RegType::ru16> {
  using type = uint16_t;
};

template <>
struct Btype<RegType::r> {
  using type = uint32_t;
};

template <>
struct Btype<RegType::rs32> {
  using type = int32_t;
};

template <>
struct Btype<RegType::ru32> {
  using type = uint32_t;
};

template <>
struct Btype<RegType::rd> {
  using type = uint64_t;
};

template <>
struct Btype<RegType::ru64> {
  using type = uint64_t;
};

template <>
struct Btype<RegType::rs64> {
  using type = int64_t;
};

}  // namespace Ptx
}  // namespace Emulator
