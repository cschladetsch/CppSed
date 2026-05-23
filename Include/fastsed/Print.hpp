#pragma once
// ============================================================
//  Print.hpp  —  std::println shim for libstdc++ < 14
//  std::format is available in GCC 13; std::println is not.
//  Remove this header and switch to <print> once on GCC 14+.
// ============================================================
#include <cstdio>
#include <format>
#include <string>

namespace std {

template<typename... Args>
void println(FILE* f, std::format_string<Args...> fmt, Args&&... args) {
    auto s = std::format(fmt, std::forward<Args>(args)...);
    s += '\n';
    std::fputs(s.c_str(), f);
}

template<typename... Args>
void print(FILE* f, std::format_string<Args...> fmt, Args&&... args) {
    auto s = std::format(fmt, std::forward<Args>(args)...);
    std::fputs(s.c_str(), f);
}

// Overloads that default to stdout
template<typename... Args>
void println(std::format_string<Args...> fmt, Args&&... args) {
    println(stdout, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
    print(stdout, fmt, std::forward<Args>(args)...);
}

} // namespace std
