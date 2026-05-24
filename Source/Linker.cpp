// ============================================================
//  Linker.cpp  —  parse tree → flat IR + label resolution
// ============================================================

#include "fastsed/Linker.hpp"

namespace fastsed {

void Linker::flatten(const CmdVec &cmds) {
  for (const Cmd &c : cmds) {
    FlatCmd fc;
    fc.op = c.op;
    fc.naddr = c.naddr;
    fc.neg = c.neg;
    fc.a[0] = c.a[0];
    fc.a[1] = c.a[1];
    fc.text = c.text;
    fc.s = c.s;
    fc.ytbl[0] = c.ytbl[0];
    fc.ytbl[1] = c.ytbl[1];
    fc.ecode = c.ecode;
    fc.lwidth = c.lwidth;
    fc.id = next_id++;

    if (c.op == ':') {
      if (labels.count(c.text))
        die(std::format("duplicate label '{}'", c.text));
      labels[c.text] = static_cast<int>(prog.size());
      prog.push_back(fc); // kept as NOP at runtime

    } else if (c.op == '{') {
      int open = static_cast<int>(prog.size());
      prog.push_back(fc);
      flatten(c.block);
      FlatCmd sentinel;
      sentinel.op = '}';
      sentinel.id = next_id++;
      prog.push_back(sentinel);
      // { jumps past } when address doesn't match
      prog[open].jmp = static_cast<int>(prog.size());

    } else if (c.op == 'b' || c.op == 't' || c.op == 'T') {
      fixups.push_back({static_cast<int>(prog.size()), c.text});
      prog.push_back(fc);

    } else {
      prog.push_back(fc);
    }
  }
}

void Linker::resolve() {
  for (auto &[idx, lbl] : fixups) {
    if (lbl.empty()) {
      prog[idx].jmp = static_cast<int>(prog.size()); // end of script
    } else {
      auto it = labels.find(lbl);
      if (it == labels.end())
        die(std::format("undefined label '{}'", lbl));
      prog[idx].jmp = it->second;
    }
  }
}

} // namespace fastsed
