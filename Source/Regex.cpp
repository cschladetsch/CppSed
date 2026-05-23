// ============================================================
//  Regex.cpp  —  POSIX regex wrapper implementation
// ============================================================

#include "fastsed/Regex.hpp"

namespace fastsed {

RE::~RE() {
    if (ok) regfree(&r);
}

RE::RE(RE&& o) noexcept
    : r(o.r), ok(o.ok), src(std::move(o.src))
{
    o.ok = false;
}

RE& RE::operator=(RE&& o) noexcept {
    if (this != &o) {
        if (ok) regfree(&r);
        r   = o.r;
        ok  = o.ok;
        src = std::move(o.src);
        o.ok = false;
    }
    return *this;
}

void RE::compile(const string& pat, int flags) {
    if (ok) { regfree(&r); ok = false; }
    src = pat;
    if (int e = regcomp(&r, pat.c_str(), flags); e) {
        char eb[512];
        regerror(e, &r, eb, sizeof eb);
        die(std::format("regex '{}': {}", pat, eb));
    }
    ok = true;
}

bool RE::exec(const char* s, size_t nm, regmatch_t* pm, int flags) const noexcept {
    return ok && regexec(&r, s, nm, pm, flags) == 0;
}

bool RE::test(const char* s) const noexcept {
    return exec(s, 0, nullptr);
}

} // namespace fastsed
