#include "gpu_config.h"

#include <fstream>
#include <stdexcept>

namespace Emulator
{

namespace
{
GpuConfig g_instance;
} // namespace

const GpuConfig& GpuConfig::instance()
{
    return g_instance;
}

void GpuConfig::SetInstance(const GpuConfig& cfg)
{
    g_instance = cfg;
}

namespace
{

std::string strip(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos)
    {
        return {};
    }
    size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

std::string stripComment(const std::string& s)
{
    bool in_quote = false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '"')
        {
            in_quote = !in_quote;
        }
        if (!in_quote && s[i] == '#')
        {
            return s.substr(0, i);
        }
    }
    return s;
}

std::string unquote(const std::string& s)
{
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

void ApplyConfig(GpuConfig& cfg, const std::string& section, const std::string& key, const std::string& val)
{
    if (section == "gpu")
    {
        if (key == "name")
        {
            cfg.name = val;
        }
        else if (key == "compute_capability")
        {
            cfg.compute_capability = val;
        }
    }
    else if (section == "warp")
    {
        if (key == "size")
        {
            cfg.warp_size = parseUint(val);
        }
    }
    else if (section == "shared_memory")
    {
        if (key == "total_size")
        {
            cfg.shared_memory_size = parseUint(val);
        }
        else if (key == "bank_count")
        {
            cfg.bank_count = parseUint(val);
        }
        else if (key == "bank_width")
        {
            cfg.bank_width = parseUint(val);
        }
        else if (key == "bank_groups")
        {
            cfg.bank_groups = parseUint(val);
        }
    }
    else if (section == "limits")
    {
        if (key == "max_threads_per_block")
        {
            cfg.max_threads_per_block = parseUint(val);
        }
        else if (key == "max_warps_per_sm")
        {
            cfg.max_warps_per_sm = parseUint(val);
        }
        else if (key == "max_blocks_per_sm")
        {
            cfg.max_blocks_per_sm = parseUint(val);
        }
        else if (key == "registers_per_thread")
        {
            cfg.registers_per_thread = parseUint(val);
        }
    }
}

uint32_t parseUint(const std::string& s)
{
    try
    {
        return static_cast<uint32_t>(std::stoul(s));
    }
    catch (...)
    {
        throw std::runtime_error("gpu_arch.toml: expected integer, got '" + s + "'");
    }
}

} // namespace

GpuConfig GpuConfig::LoadFromFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
    {
        throw std::runtime_error("Cannot open GPU config file: " + path);
    }

    GpuConfig cfg;
    std::string section;
    std::string line;

    while (std::getline(f, line))
    {
        line = strip(stripComment(line));
        if (line.empty())
        {
            continue;
        }

        if (line.front() == '[')
        {
            auto end = line.find(']');
            if (end == std::string::npos)
            {
                continue;
            }
            section = strip(line.substr(1, end - 1));
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }

        auto key = strip(line.substr(0, eq));
        auto val = strip(unquote(strip(line.substr(eq + 1))));
        ApplyConfig(cfg, section, key, val);
    }

    return cfg;
}

} // namespace Emulator
