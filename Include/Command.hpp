#pragma once
// ============================================================
//  Command.hpp  —  command parse tree and flat IR
//
//  Two representations:
//    Cmd / CmdVec   — nested parse tree produced by Parser
//    FlatCmd        — flat array with resolved jump indices
//                     produced by Linker; used at runtime
// ============================================================

#include "fastsed/Address.hpp"
#include "fastsed/Replacement.hpp"

namespace fastsed {

// ── Substitution options (s command) ─────────────────────────
struct SubstOpt {
    shared_ptr<RE> re;
    ReplVec        repl;
    bool           global = false; // g flag
    bool           print  = false; // p flag
    bool           exec   = false; // e flag (GNU)
    bool           icase  = false; // i/I flag
    int            nth    = 0;     // Nth occurrence (0 = first, or all+global)
    string         wfile;          // w flag output file
};

// ── Nested parse tree node ────────────────────────────────────
struct Cmd;
using CmdVec = vector<Cmd>;

struct Cmd {
    char     op    = 0;
    int      naddr = 0;
    bool     neg   = false;
    Addr     a[2];

    string   text;       // a c i b t T : r R w W e v
    SubstOpt s;          // s
    string   ytbl[2];    // y  [0]=source  [1]=dest
    CmdVec   block;      // {  nested commands
    int      ecode = 0;  // q Q  exit code
    long     lwidth = 70;// l  line wrap width
};

// ── Flat IR node (post-linking) ───────────────────────────────
struct FlatCmd {
    char     op    = 0;
    int      naddr = 0;
    bool     neg   = false;
    Addr     a[2];

    string   text;
    SubstOpt s;
    string   ytbl[2];
    int      ecode  = 0;
    long     lwidth = 70;

    int      id  = -1; // stable index (range-active tracking)
    int      jmp = -1; // b/t/T → label idx;  { → first idx past }
};

} // namespace fastsed
