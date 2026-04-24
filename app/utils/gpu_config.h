#pragma once

#include <cstdint>
#include <string>

namespace Emulator
{

struct GpuConfig
{
    std::string name = "Generic NVIDIA GPU";
    std::string compute_capability = "8.0";
    uint32_t warp_size = 32;
    uint32_t shared_memory_size = 49152;
    uint32_t bank_count = 32;
    uint32_t bank_width = 4;
    uint32_t bank_groups = 4;
    uint32_t max_threads_per_block = 1024;
    uint32_t max_warps_per_sm = 64;
    uint32_t max_blocks_per_sm = 32;
    uint32_t registers_per_thread = 255;

    static GpuConfig LoadFromFile(const std::string& path);

    static const GpuConfig& instance();
    static void SetInstance(const GpuConfig& cfg);
};

} // namespace Emulator
