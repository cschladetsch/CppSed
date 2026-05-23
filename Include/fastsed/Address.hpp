#pragma once
// ============================================================
//  Address.hpp  —  sed address types
// ============================================================

#include "Common.hpp"
#include "Regex.hpp"

namespace fastsed {

enum class AK {
    None,   // no address (matches everything)
    Line,   // absolute line number  N
    Line0,  // line 0  (special: matches first line in 0,/re/ ranges)
    Last,   // $  — last line
    RE,     // /pattern/  or  \Xpattern X
    Step,   // first~step  (GNU extension)
};

struct Addr {
    AK             k     = AK::None;
    long           line  = 0;
    long           first = 0, step = 0; // for Step
    shared_ptr<RE> re;                  // for RE (nullptr → reuse last)
};

} // namespace fastsed
