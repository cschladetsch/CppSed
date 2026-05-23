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

static bool repl_is_plain_literal(const ReplVec& rv, string_view& lit) {
    if (rv.size() != 1 || rv[0].k != RTK::Lit) return false;
    lit = rv[0].lit;
    return true;
}

static bool do_subst_prefix(const SubstOpt& s, string& ps) {
    string_view lit;
    if (!s.re || s.re->src != "^" || s.global || s.nth > 1 || !repl_is_plain_literal(s.repl, lit))
        return false;
    ps.insert(0, lit);
    return true;
}

static bool do_subst_literal(const RE& re, const SubstOpt& s, string& ps) {
    if (!re.is_literal() || s.icase) return false;

    string_view repl_lit;
    if (!repl_is_plain_literal(s.repl, repl_lit)) return false;

    const string& needle = re.src;
    if (needle.empty()) return false;

    string result;
    const size_t nlen = needle.size();
    const size_t extra = repl_lit.size() > nlen ? repl_lit.size() - nlen : 0;
    if (extra != 0 && s.global) {
        result.reserve(ps.size() + (ps.size() / nlen + 1) * extra);
    } else {
        result.reserve(ps.size());
    }

    const char* cur = ps.data();
    const char* const end = cur + ps.size();
    const char  first = needle.front();
    const int   nth   = s.nth > 0 ? s.nth : 1;
    int         mnum  = 0;
    bool        hit_replacement = false;

    while (cur < end) {
        const char* hit = static_cast<const char*>(
            std::memchr(cur, first, static_cast<size_t>(end - cur)));

        while (hit && static_cast<size_t>(end - hit) >= nlen &&
               std::memcmp(hit, needle.data(), nlen) != 0) {
            hit = static_cast<const char*>(
                std::memchr(hit + 1, first, static_cast<size_t>(end - (hit + 1))));
        }

        if (!hit) {
            result.append(cur, end - cur);
            break;
        }

        ++mnum;
        bool do_it = s.global ? (s.nth == 0 || mnum >= s.nth)
                               : (mnum == nth);

        if (do_it) {
            result.append(cur, hit - cur);
            result.append(repl_lit);
            cur = hit + nlen;
            hit_replacement = true;
            if (!s.global) {
                result.append(cur, end - cur);
                break;
            }
        } else {
            result.append(cur, (hit - cur) + nlen);
            cur = hit + nlen;
        }
    }

    if (!hit_replacement) return false;
    ps = std::move(result);
    return true;
}

bool do_subst(const SubstOpt& s, string& ps, const shared_ptr<RE>& last_re) {
    const RE* re = (s.re && s.re->ok) ? s.re.get() : last_re.get();
    if (!re || !re->ok) die("no previous regex");

    if (do_subst_prefix(s, ps)) return true;
    if (do_subst_literal(*re, s, ps)) return true;

    regmatch_t  pm[NMATCH];
    string      result;
    result.reserve(ps.size() + 32);
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
