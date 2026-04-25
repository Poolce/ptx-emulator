#include "profiler.h"

#include <chrono>
#include <cstdlib>
#include <ctime>

namespace Emulator
{

Profiler& Profiler::instance()
{
    static Profiler inst;
    return inst;
}

Profiler::Profiler()
{
    const char* env = std::getenv("CUEMU_COLLECT_PROFILING");
    if (env == nullptr || std::string_view(env) != "1")
    {
        return;
    }

    const char* out_env = std::getenv("CUEMU_PROFILING_OUTPUT");
    const std::string output_path = out_env ? std::string(out_env) : "profiling.txt";

    output_.open(output_path);
    if (!output_.is_open())
    {
        return;
    }
    enabled_ = true;
}

void Profiler::BeginLaunch(const std::string& func_name)
{
    if (!enabled_)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    output_ << "Launch " << launch_counter_++ << " " << func_name << "\n";
    output_.flush();
}

void Profiler::Flush(const WarpProfilingBuffer& buf)
{
    if (buf.empty())
    {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::string cur_func;
    std::string cur_bb;
    for (const auto& rec : buf)
    {
        if (rec.function_name != cur_func)
        {
            cur_func = rec.function_name;
            cur_bb = "";
            output_ << "Function " << cur_func << "\n";
        }
        if (rec.basic_block != cur_bb)
        {
            cur_bb = rec.basic_block;
            output_ << "Basic Block " << cur_bb << "\n";
        }
        output_ << FormatRecord(rec) << "\n";
    }
    output_.flush();
}

std::string Profiler::FormatRecord(const ProfilingRecord& rec) const
{
    std::ostringstream oss;
    oss << "[" << FormatTimestamp(rec.timestamp_ns) << "] ";
    oss << "[PC: 0x" << std::hex << std::setfill('0') << std::setw(16) << rec.pc << "]  ";
    oss << "Block: [" << std::dec << rec.block_id.x << ", " << rec.block_id.y << ", " << rec.block_id.z << "] ";
    oss << "WarpId: " << rec.warp_id << " ";
    oss << "Execution Mask 0x" << std::hex << std::setfill('0') << std::setw(8) << rec.execution_mask << " ";
    oss << std::dec << rec.instr_name;
    for (size_t i = 0; i < rec.metrics.size(); ++i)
    {
        oss << " " << rec.metrics[i].first << ": " << rec.metrics[i].second;
        if (i + 1 < rec.metrics.size())
        {
            oss << ",";
        }
    }
    return oss.str();
}

std::string Profiler::FormatTimestamp(uint64_t ns)
{
    using namespace std::chrono;
    const auto tp = system_clock::time_point(nanoseconds(ns));
    const auto t = system_clock::to_time_t(tp);
    const auto ms = duration_cast<milliseconds>(nanoseconds(ns) % seconds(1));

    std::tm tm_info{};
    localtime_r(&t, &tm_info);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_info);

    std::ostringstream oss;
    oss << buf << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

} // namespace Emulator
