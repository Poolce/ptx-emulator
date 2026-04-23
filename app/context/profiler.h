#pragma once

#include "types.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Emulator
{

struct ProfilingRecord
{
    uint64_t timestamp_ns = 0;
    uint64_t pc = 0;
    dim3 block_id{};
    uint32_t warp_id = 0;
    uint32_t execution_mask = 0;
    std::string function_name;
    std::string basic_block;
    std::string instr_name;
    std::vector<std::pair<std::string, std::string>> metrics;
};

using WarpProfilingBuffer = std::vector<ProfilingRecord>;

class Profiler
{
  public:
    static Profiler& instance();

    bool IsEnabled() const { return enabled_; }
    void Flush(const WarpProfilingBuffer& buf);

  private:
    Profiler();

    std::string FormatRecord(const ProfilingRecord& rec) const;
    static std::string FormatTimestamp(uint64_t ns);

    bool enabled_ = false;
    std::ofstream output_;
    std::mutex mutex_;
};

} // namespace Emulator
