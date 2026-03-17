#pragma once

namespace Emulator
{
namespace Ptx
{

template <typename T>
T FromString(const std::string& str);

template <typename T>
std::string ToString(const T& type);

} // namespace Ptx
} // namespace Emulator