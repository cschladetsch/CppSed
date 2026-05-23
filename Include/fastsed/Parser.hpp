#pragma once
// ============================================================
//  Parser.hpp  —  sed script parser
//  Converts a raw script string into a CmdVec parse tree.
// ============================================================

#include "Command.hpp"

namespace fastsed {

struct Parser {
    string_view src;
    size_t      pos = 0;
    bool        ext = false; // -E / -r  →  REG_EXTENDED

    // ── Low-level character access ────────────────────────────
    char peek()   const;
    char get();
    bool at_end() const;
    void skip_ws();
    bool eat(char c);

    // ── Sub-parsers ───────────────────────────────────────────
    string parse_re_body(char delim);
    Addr   parse_addr();
    string parse_text();        // for a / c / i
    string parse_label();       // for b / t / T / :
    string parse_filename();    // for r / R / w / W / e
    void   parse_y(Cmd& cmd);
    void   parse_s(Cmd& cmd);

    // ── Entry points ─────────────────────────────────────────
    // Parse commands until end-of-script or (in_block) until '}'.
    CmdVec parse_cmds(bool in_block = false);
    Cmd    parse_one();

    // Returns true if the very first two chars are '#' 'n'
    // (POSIX: enables -n mode).
    bool first_line_is_hash_n() const;
};

} // namespace fastsed
