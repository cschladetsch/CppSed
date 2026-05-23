// ============================================================
//  Common.cpp  —  global definitions and diagnostics
// ============================================================

#include "fastsed/OutBuf.hpp"
#include "fastsed/Common.hpp"
#include "fastsed/Print.hpp"

namespace fastsed {

OutBuf g_out;          // primary stdout buffer
int    g_exit_code = 0;

[[noreturn]] void die(string_view msg) {
    g_out.flush();
    std::println(stderr, "sed: {}", msg);
    std::exit(g_exit_code ? g_exit_code : 1);
}

void warn(string_view msg) {
    std::println(stderr, "sed: warning: {}", msg);
}

} // namespace fastsed
