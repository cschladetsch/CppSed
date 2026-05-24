#pragma once
// ============================================================
//  Engine.hpp  —  sed execution engine
// ============================================================

#include "fastsed/Command.hpp"
#include "fastsed/OutBuf.hpp"

namespace fastsed {

// Forward declaration — LineSource is in its own header
class LineSource;

// ── Flow-control return codes from run() ─────────────────────
enum class FC {
  Normal,     // end of script reached normally
  NewCycle,   // d / D / c — start next cycle immediately
  Quit,       // q — print + exit
  QuitSilent, // Q / end-of-N — exit without extra print
};

// ── Helpers (defined in EngineHelpers.cpp) ───────────────────
string exec_shell(const string &cmd);
void do_list(const string &ps, long width, OutBuf &out);
void do_trans(const string ytbl[2], string &ps);
bool do_subst(const SubstOpt &s, string &ps, const shared_ptr<RE> &last_re);

// ── Engine ───────────────────────────────────────────────────
struct Engine {
  // ── Configuration (set before process()) ─────────────────
  const vector<FlatCmd> *prog = nullptr;
  OutBuf *out = &g_out;
  bool suppress = false; // -n
  bool sandbox = false;  // --sandbox
  bool separate = false; // -s

  // ── State ─────────────────────────────────────────────────
  string ps, hs; // pattern space, hold space
  long lineno = 0;
  bool last_line = false;
  string fname, prev_fname;
  bool sub_made = false;

  // Per-command two-address range tracking.
  // range_at[id] == -1 → inactive; else → lineno when entered.
  vector<long> range_at;
  int total_cmds = 0;

  shared_ptr<RE> last_re; // most recently used regex (empty // re-use)

  struct Deferred {
    bool is_file, one_line;
    string val;
  };
  vector<Deferred> deferred; // a / r / R output queue

  std::map<string, FILE *> wfiles; // open write-file handles

  // ── Initialisation ────────────────────────────────────────
  void init(int ncmds);

  // ── Address matching ──────────────────────────────────────
  bool addr_single(const Addr &a) const;
  bool matches(const FlatCmd &c);
  bool range_ending(const FlatCmd &c) const;

  // ── Output helpers ────────────────────────────────────────
  void write_to_file(const string &path, string_view sv);
  void flush_deferred();
  void close_wfiles();

  // ── Core loop ─────────────────────────────────────────────
  // Executes the flat program for one input cycle.
  FC run(LineSource &src);

  // Top-level: drives the read → run → output loop.
  void process(LineSource &src);
};

} // namespace fastsed
