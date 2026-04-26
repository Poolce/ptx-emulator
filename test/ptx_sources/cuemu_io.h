#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace CuemuIo
{

namespace Detail
{

inline std::string to_upper(const std::string& s)
{
    std::string r = s;
    for (char& c : r)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return r;
}

inline std::unordered_map<std::string, std::string> parse_spec(const std::string& spec)
{
    std::unordered_map<std::string, std::string> m;
    std::istringstream ss(spec);
    std::string token;
    bool first = true;
    while (std::getline(ss, token, ':'))
    {
        const auto eq = token.find('=');
        if (first)
        {
            m[""] = token;
            first = false;
        }
        else if (eq != std::string::npos)
        {
            m[token.substr(0, eq)] = token.substr(eq + 1);
        }
    }
    return m;
}

inline double get_d(const std::unordered_map<std::string, std::string>& m, const char* key, double def)
{
    auto it = m.find(key);
    return it != m.end() ? std::stod(it->second) : def;
}

inline unsigned long long get_seed(const std::unordered_map<std::string, std::string>& m)
{
    auto it = m.find("seed");
    return it != m.end() ? std::stoull(it->second) : 0ULL;
}

template <typename T>
void apply_spec(const std::string& spec, T* data, size_t n, std::function<T(size_t)>& default_gen)
{
    auto m = parse_spec(spec);
    auto mode = m[""];

    if (mode == "default" || mode.empty())
    {
        for (size_t i = 0; i < n; ++i)
        {
            data[i] = default_gen(i);
        }
    }
    else if (mode == "zeros")
    {
        std::fill(data, data + n, T(0));
    }
    else if (mode == "ones")
    {
        std::fill(data, data + n, T(1));
    }
    else if (mode == "constant")
    {
        const T v = static_cast<T>(get_d(m, "value", 0.0));
        std::fill(data, data + n, v);
    }
    else if (mode == "sequential")
    {
        const double start = get_d(m, "start", 0.0);
        const double step = get_d(m, "step", 1.0);
        for (size_t i = 0; i < n; ++i)
        {
            data[i] = static_cast<T>(start + (double(i) * step));
        }
    }
    else if (mode == "uniform")
    {
        const double lo = get_d(m, "lo", -1.0);
        const double hi = get_d(m, "hi", 1.0);
        std::mt19937_64 rng(get_seed(m));
        std::uniform_real_distribution<double> dist(lo, hi);
        for (size_t i = 0; i < n; ++i)
        {
            data[i] = static_cast<T>(dist(rng));
        }
    }
    else if (mode == "normal")
    {
        const double mean = get_d(m, "mean", 0.0);
        const double std = get_d(m, "std", 1.0);
        std::mt19937_64 rng(get_seed(m));
        std::normal_distribution<double> dist(mean, std);
        for (size_t i = 0; i < n; ++i)
        {
            data[i] = static_cast<T>(dist(rng));
        }
    }
    else
    {
        throw std::runtime_error(std::string("CuemuIo: unknown generation mode '") + mode + "'");
    }
}

} // namespace Detail

template <typename T>
void generate(const char* name, T* data, size_t n, std::function<T(size_t)> default_gen)
{
    const std::string key = "CUEMU_GEN_" + Detail::to_upper(name);
    const char* spec_env = std::getenv(key.c_str());
    if (spec_env && spec_env[0] != '\0')
    {
        Detail::apply_spec<T>(std::string(spec_env), data, n, default_gen);
    }
    else
    {
        for (size_t i = 0; i < n; ++i)
        {
            data[i] = default_gen(i);
        }
    }
}

template <typename T>
void generate(const char* name, std::vector<T>& v, std::function<T(size_t)> default_gen)
{
    generate<T>(name, v.data(), v.size(), default_gen);
}

} // namespace CuemuIo
