#pragma once

namespace Emulator {
namespace Constsant {

// Nvidia GForce 1050Ti

constexpr uint32_t SM_COUNT = 6;
constexpr uint32_t CUDA_CORE_PER_SM = 128;
constexpr uint32_t CUDA_CORE_COUNT = SM_COUNT * CUDA_CORE_PER_SM;
constexpr uint32_t WARP_SIZE = 32;

// Memory Space
constexpr uint64_t GLOBAL_SPACE_SIZE = 4 * 1024 * 1024 * 1024;  // 4Gb
constexpr uint64_t SM_SHARED_SPACE_SIZE = 96 * 1024;            // 96Kb

}  // namespace Constsant
}  // namespace Emulator
