// ============================================================
//  EngineHelpers.cpp  —  stateless engine helper functions
//
//  exec_shell  uses Boost.Process v1 (replaces popen)
//  do_list     visually unambiguous pattern-space print
//  do_trans    byte-level transliteration (y command)
//  do_subst    s/// with all flags
// ============================================================

#include "fastsed/Engine.hpp"

#include <boost/process.hpp>

namespace fastsed {
namespace bp = boost::process;

// ── exec_shell ───────────────────────────────────────────────
string exec_shell(const string& cmd) {
    bp::ipstream pipe;
    bp::child child(
        bp::search_path("sh"),
        bp::args({"-c", cmd}),
        bp::std_out > pipe
    );
    string result, line;
    while (std::getline(pipe, line))
        result += line + '\n';
    child.wait();
    while (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

// ── do_list ───────────────────────────────────────────────────
void do_list(const string& ps, long width, OutBuf& out) {
    if (width <= 0) width = 70;
    long col = 0;

    auto put_esc = [&](string_view sv) {
        for (char c : sv) {
            if (col >= width - 1) { out.write("\\\n"); col = 0; }
            out.put(c); ++col;
        }
    };

    for (unsigned char c : ps) {
        char tmp[8];
        string_view esc;
        switch (c) {
        case '\\': esc = "\\\\"; break;
        case '\a': esc = "\\a";  break;
        case '\b': esc = "\\b";  break;
        case '\f': esc = "\\f";  break;
        case '\r': esc = "\\r";  break;
        case '\t': esc = "\\t";  break;
        case '\v': esc = "\\v";  break;
        case '\n': esc = "\\n";  break;
        default:
            if (c >= 32 && c <= 126) {
                tmp[0] = static_cast<char>(c);
                esc = {tmp, 1};
            } else {
                int len = snprintf(tmp, sizeof tmp, "\\%03o", c);
                esc = {tmp, static_cast<size_t>(len)};
            }
            break;
        }
        put_esc(esc);
    }
    if (col >= width - 1) { out.write("\\\n"); col = 0; }
    out.put('$'); out.put('\n');
}

// ── do_trans ─────────────────────────────────────────────────
void do_trans(const string ytbl[2], string& ps) {
    unsigned char tbl[256];
    for (int i = 0; i < 256; ++i) tbl[i] = static_cast<unsigned char>(i);
    const string& s = ytbl[0];
    const string& d = ytbl[1];
    for (size_t i = 0; i < s.size() && i < d.size(); ++i)
        tbl[static_cast<unsigned char>(s[i])] =
            static_cast<unsigned char>(d[i]);
    for (char& c : ps)
        c = static_cast<char>(tbl[static_cast<unsigned char>(c)]);
}

// ── do_subst ─────────────────────────────────────────────────
static constexpr int NMATCH = 10;

bool do_subst(const SubstOpt& s, string& ps, const shared_ptr<RE>& last_re) {
    const RE* re = (s.re && s.re->ok) ? s.re.get() : last_re.get();
    if (!re || !re->ok) die("no previous regex");

    regmatch_t  pm[NMATCH];
    string      result;
    const char* p       = ps.c_str();
    bool        changed = false;
    int         mnum    = 0;
    int         eflags  = 0;

    for (;;) {
        if (!re->exec(p, NMATCH, pm, eflags)) break;
        ++mnum;

        bool do_it = s.global ? (s.nth == 0 || mnum >= s.nth)
                               : (mnum == (s.nth > 0 ? s.nth : 1));

        if (!do_it) {
            result.append(p, static_cast<size_t>(pm[0].rm_eo));
            p += pm[0].rm_eo;
            if (pm[0].rm_so == pm[0].rm_eo) { if (!*p) break; result += *p++; }
            eflags = REG_NOTBOL;
            continue;
        }

        result.append(p, static_cast<size_t>(pm[0].rm_so));
        result += apply_repl(s.repl, p, pm, NMATCH);
        p       += pm[0].rm_eo;
        changed  = true;

        if (!s.global) break;
        if (pm[0].rm_so == pm[0].rm_eo) { if (!*p) break; result += *p++; }
        eflags = REG_NOTBOL;
    }

    if (!changed) return false;
    result.append(p);
    ps = std::move(result);
    return true;
}

} // namespace fastsed
