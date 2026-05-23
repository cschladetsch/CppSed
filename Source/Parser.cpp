// ============================================================
//  Parser.cpp  —  sed script recursive-descent parser
// ============================================================

#include "fastsed/Parser.hpp"
#include <cctype>

namespace fastsed {

// ── Character access ──────────────────────────────────────────
char Parser::peek()   const { return pos < src.size() ? src[pos] : '\0'; }
char Parser::get()          { return pos < src.size() ? src[pos++] : '\0'; }
bool Parser::at_end() const { return pos >= src.size(); }
void Parser::skip_ws()      { while (!at_end() && (peek()==' '||peek()=='\t')) get(); }
bool Parser::eat(char c)    { if (peek()==c) { get(); return true; } return false; }

bool Parser::first_line_is_hash_n() const {
    return src.size() >= 2 && src[0] == '#' && src[1] == 'n';
}

// ── Regex body (delimiter already consumed) ───────────────────
string Parser::parse_re_body(char d) {
    string p;
    while (!at_end() && peek() != d) {
        char c = get();
        if (c == '\\' && !at_end()) {
            char nc = get();
            if (nc == d) p += d;
            else         { p += c; p += nc; }
        } else {
            p += c;
        }
    }
    if (at_end()) die(std::format("unterminated regex (expected '{}')", d));
    get(); // consume closing delimiter
    return p;
}

// ── Address ───────────────────────────────────────────────────
Addr Parser::parse_addr() {
    Addr a;
    if (at_end()) return a;
    char c = peek();

    if (c == '$') { get(); a.k = AK::Last; return a; }

    if (c >= '0' && c <= '9') {
        long n = 0;
        while (!at_end() && peek() >= '0' && peek() <= '9')
            n = n * 10 + (get() - '0');
        if (eat('~')) {
            long st = 0;
            while (!at_end() && peek() >= '0' && peek() <= '9')
                st = st * 10 + (get() - '0');
            a.k = AK::Step; a.first = n; a.step = st;
        } else {
            a.k = (n == 0 ? AK::Line0 : AK::Line); a.line = n;
        }
        return a;
    }

    if (c == '/' || c == '\\') {
        char d = '/';
        if (c == '\\') { get(); d = get(); }
        else            { get(); }
        string pat = parse_re_body(d);
        a.k = AK::RE;
        a.re = make_shared<RE>();
        if (!pat.empty())
            a.re->compile(pat, ext ? REG_EXTENDED : 0);
        return a;
    }
    return a; // AK::None
}

// ── Text argument for a / c / i ───────────────────────────────
string Parser::parse_text() {
    string t;
    skip_ws();
    if (peek() == '\\') {
        get();
        if (peek() == '\n') get(); // leading \<newline>
    }
    while (!at_end()) {
        char c = get();
        if (c == '\\') {
            if (at_end() || peek() == '\n') {
                if (!at_end()) get();
                t += '\n';
            } else {
                char nc = get();
                if      (nc == 'n')  t += '\n';
                else if (nc == '\\') t += '\\';
                else                 { t += '\\'; t += nc; }
            }
        } else if (c == '\n') {
            break;
        } else {
            t += c;
        }
    }
    return t;
}

// ── Label / filename helpers ───────────────────────────────────
string Parser::parse_label() {
    string l; skip_ws();
    while (!at_end() && peek()!='\n' && peek()!=';' && peek()!='}')
        l += get();
    while (!l.empty() && (l.back()==' ' || l.back()=='\t'))
        l.pop_back();
    return l;
}

string Parser::parse_filename() {
    string f; skip_ws();
    while (!at_end() && peek()!='\n' && peek()!=';') f += get();
    while (!f.empty() && f.back()==' ') f.pop_back();
    if (f.empty()) die("missing filename argument");
    return f;
}

// ── y command ─────────────────────────────────────────────────
void Parser::parse_y(Cmd& cmd) {
    if (at_end()) die("y: missing delimiter");
    char d = get();
    auto yarg = [&] {
        string s;
        while (!at_end() && peek() != d) {
            char c = get();
            if (c == '\\') {
                char nc = get();
                if      (nc == 'n') s += '\n';
                else if (nc == d)   s += d;
                else                { s += '\\'; s += nc; }
            } else s += c;
        }
        if (at_end()) die("y: unterminated");
        get();
        return s;
    };
    cmd.ytbl[0] = yarg();
    cmd.ytbl[1] = yarg();
    if (cmd.ytbl[0].size() != cmd.ytbl[1].size())
        die("y: source and dest strings are different lengths");
}

// ── s command ─────────────────────────────────────────────────
void Parser::parse_s(Cmd& cmd) {
    if (at_end()) die("s: missing delimiter");
    char d = get();

    // regex
    string pat;
    while (!at_end() && peek() != d) {
        char c = get();
        if (c=='\\') { char nc=get(); if(nc==d) pat+=d; else{pat+=c;pat+=nc;} }
        else pat += c;
    }
    if (at_end()) die("s: unterminated regex");
    get();

    // replacement
    string repl_s;
    while (!at_end() && peek() != d) {
        char c = get();
        if (c=='\\') { char nc=get(); if(nc==d) repl_s+=d; else{repl_s+=c;repl_s+=nc;} }
        else repl_s += c;
    }
    if (at_end()) die("s: unterminated replacement");
    get();

    // flags
    bool icase = false;
    while (!at_end()) {
        char f = peek();
        if      (f == 'g') { get(); cmd.s.global = true; }
        else if (f == 'p') { get(); cmd.s.print  = true; }
        else if (f=='i'||f=='I') { get(); icase  = true; }
        else if (f == 'e') { get(); cmd.s.exec   = true; }
        else if (f>='1'&&f<='9') { get(); cmd.s.nth = f-'0'; }
        else if (f == 'w') { get(); cmd.s.wfile = parse_filename(); break; }
        else if (f=='m'||f=='M') { get(); /* multi-line (partial) */ }
        else break;
    }

    cmd.s.icase = icase;
    cmd.s.re    = make_shared<RE>();
    if (!pat.empty()) {
        int fl = (ext ? REG_EXTENDED : 0) | (icase ? REG_ICASE : 0);
        cmd.s.re->compile(pat, fl);
    }
    cmd.s.repl = parse_repl(repl_s);
}

// ── Main parse loop ───────────────────────────────────────────
CmdVec Parser::parse_cmds(bool in_block) {
    CmdVec cmds;
    for (;;) {
        while (!at_end() &&
               (peek()=='\n'||peek()==';'||peek()==' '||peek()=='\t'))
            get();
        if (at_end()) {
            if (in_block) die("unmatched '{'");
            break;
        }
        if (peek() == '#') {
            while (!at_end() && peek() != '\n') get();
            continue;
        }
        if (peek() == '}') {
            get();
            if (!in_block) die("unexpected '}'");
            break;
        }
        cmds.push_back(parse_one());
    }
    return cmds;
}

Cmd Parser::parse_one() {
    Cmd cmd;

    // addresses
    Addr a0 = parse_addr();
    if (a0.k != AK::None) {
        cmd.a[0] = a0; cmd.naddr = 1;
        skip_ws();
        if (eat(',')) {
            skip_ws();
            Addr a1 = parse_addr();
            if (a1.k == AK::None) die("expected second address after ','");
            cmd.a[1] = a1; cmd.naddr = 2;
        }
    }
    skip_ws();
    while (eat('!')) { cmd.neg = !cmd.neg; skip_ws(); }
    skip_ws();
    if (at_end()) die("unexpected end of script");

    cmd.op = get();
    switch (cmd.op) {
    case 'a': case 'c': case 'i':
        cmd.text = parse_text(); break;
    case 'b': case 't': case 'T': case ':':
        cmd.text = parse_label(); break;
    case 'q': case 'Q':
        skip_ws();
        if (!at_end() && peek()>='0' && peek()<='9') {
            cmd.ecode = 0;
            while (!at_end() && peek()>='0' && peek()<='9')
                cmd.ecode = cmd.ecode*10 + (get()-'0');
        }
        break;
    case 'r': case 'R': case 'w': case 'W':
        cmd.text = parse_filename(); break;
    case 's': parse_s(cmd); break;
    case 'y': parse_y(cmd); break;
    case 'l':
        skip_ws();
        if (!at_end() && peek()>='0' && peek()<='9') {
            cmd.lwidth = 0;
            while (!at_end() && peek()>='0' && peek()<='9')
                cmd.lwidth = cmd.lwidth*10 + (get()-'0');
        }
        break;
    case '{':
        cmd.block = parse_cmds(/*in_block=*/true); break;
    case 'e':
        if (!at_end() && peek()!='\n' && peek()!=';' && peek()!='}')
            cmd.text = parse_filename();
        break;
    case 'v':
        while (!at_end() && peek()!='\n' && peek()!=';') get();
        break;
    case 'd': case 'D': case 'g': case 'G': case 'h': case 'H':
    case 'n': case 'N': case 'p': case 'P': case 'x': case 'z':
    case '=': case 'F':
        break;
    default:
        die(std::format("unknown command '{}'", cmd.op));
    }
    return cmd;
}

} // namespace fastsed
