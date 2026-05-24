#pragma once
// ============================================================
//  Linker.hpp  —  parse-tree → flat IR + label resolution
//
//  flatten()  recursively converts nested CmdVec → FlatCmd[],
//             inserting synthetic '}' sentinels and recording
//             fixup slots for b / t / T.
//  resolve()  patches every fixup slot with the resolved index.
// ============================================================

#include "fastsed/Command.hpp"
#include <map>

namespace fastsed {

struct Linker {
  vector<FlatCmd> prog;
  std::map<string, int> labels;
  vector<std::pair<int, string>> fixups; // {prog-idx, label-name}
  int next_id = 0;

  void flatten(const CmdVec &cmds);
  void resolve();
};

} // namespace fastsed
