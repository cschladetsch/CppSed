// ============================================================
//  TestLinker.cpp  —  unit tests for the linker / flat IR
// ============================================================

#include "fastsed/Linker.hpp"
#include "fastsed/Parser.hpp"
#include <gtest/gtest.h>

using namespace fastsed;

// ── Helpers ───────────────────────────────────────────────────
static Linker link(const std::string &script, bool ext = false) {
  Parser p;
  p.src = script;
  p.ext = ext;
  auto tree = p.parse_cmds();
  Linker lk;
  lk.flatten(tree);
  lk.resolve();
  return lk;
}

static char op(const Linker &lk, int i) {
  return lk.prog.at(static_cast<size_t>(i)).op;
}

// ── Flat instruction sequence ─────────────────────────────────
TEST(Linker, SingleCommand) {
  auto lk = link("d");
  ASSERT_EQ(lk.prog.size(), 1u);
  EXPECT_EQ(op(lk, 0), 'd');
}

TEST(Linker, TwoCommands) {
  auto lk = link("p\nd");
  ASSERT_EQ(lk.prog.size(), 2u);
  EXPECT_EQ(op(lk, 0), 'p');
  EXPECT_EQ(op(lk, 1), 'd');
}

// ── Block flattening ─────────────────────────────────────────
TEST(Linker, BlockFlattenedWithSentinel) {
  // /x/ { p \n d \n }  →  { p d }
  auto lk = link("/x/ {\np\nd\n}");
  // prog: [{, p, d, }]
  ASSERT_EQ(lk.prog.size(), 4u);
  EXPECT_EQ(op(lk, 0), '{');
  EXPECT_EQ(op(lk, 1), 'p');
  EXPECT_EQ(op(lk, 2), 'd');
  EXPECT_EQ(op(lk, 3), '}');
}

TEST(Linker, BlockJumpSkipsPastSentinel) {
  auto lk = link("/x/ { p\n d\n }");
  // {.jmp should point past }, i.e. index 4
  EXPECT_EQ(lk.prog[0].jmp, 4);
}

TEST(Linker, NestedBlock) {
  auto lk = link("/a/ { /b/ { p\n } }");
  // outer{, inner{, p, inner}, outer}
  ASSERT_EQ(lk.prog.size(), 5u);
  EXPECT_EQ(op(lk, 0), '{');
  EXPECT_EQ(op(lk, 1), '{');
  EXPECT_EQ(op(lk, 2), 'p');
  EXPECT_EQ(op(lk, 3), '}');
  EXPECT_EQ(op(lk, 4), '}');
  // outer block jmp should point past outer }
  EXPECT_EQ(lk.prog[0].jmp, 5);
  // inner block jmp should point past inner }
  EXPECT_EQ(lk.prog[1].jmp, 4);
}

// ── Label resolution ─────────────────────────────────────────
TEST(Linker, LabelKeptAsNop) {
  auto lk = link(":top\nd");
  ASSERT_EQ(lk.prog.size(), 2u);
  EXPECT_EQ(op(lk, 0), ':');
  EXPECT_EQ(op(lk, 1), 'd');
  EXPECT_EQ(lk.labels.at("top"), 0);
}

TEST(Linker, BranchResolvesToLabel) {
  auto lk = link(":loop\np\nb loop");
  // prog: [: p b]
  ASSERT_EQ(lk.prog.size(), 3u);
  EXPECT_EQ(op(lk, 2), 'b');
  EXPECT_EQ(lk.prog[2].jmp, 0); // branches back to :loop at index 0
}

TEST(Linker, BranchEmptyLabelResolvesToEnd) {
  auto lk = link("p\nb");
  EXPECT_EQ(lk.prog[1].jmp, static_cast<int>(lk.prog.size()));
}

TEST(Linker, TCommandResolvesToLabel) {
  auto lk = link("s/a/b/\nt done\n:done\nd");
  // prog: [s, t, :done, d]
  int done_idx = lk.labels.at("done");
  EXPECT_EQ(lk.prog[1].jmp, done_idx);
}

TEST(Linker, TCommandEmptyLabel) {
  auto lk = link("s/a/b/\nt");
  EXPECT_EQ(lk.prog[1].jmp, static_cast<int>(lk.prog.size()));
}

TEST(Linker, UndefinedLabelDies) {
  // parse succeeds, resolve should die — test via EXPECT_EXIT
  EXPECT_EXIT(link("b nowhere"), ::testing::ExitedWithCode(1),
              "undefined label");
}

TEST(Linker, DuplicateLabelDies) {
  EXPECT_EXIT(link(":x\n:x\nd"), ::testing::ExitedWithCode(1),
              "duplicate label");
}

// ── IDs are unique and stable ────────────────────────────────
TEST(Linker, CommandIDsAreUnique) {
  auto lk = link("p\nd\np");
  std::set<int> ids;
  for (auto &fc : lk.prog) {
    EXPECT_EQ(ids.count(fc.id), 0u) << "duplicate id " << fc.id;
    ids.insert(fc.id);
  }
}

// ── Address and flags survive flattening ─────────────────────
TEST(Linker, AddressSurvivesFlattening) {
  auto lk = link("3,7d");
  ASSERT_GE(lk.prog.size(), 1u);
  EXPECT_EQ(lk.prog[0].naddr, 2);
  EXPECT_EQ(lk.prog[0].a[0].line, 3);
  EXPECT_EQ(lk.prog[0].a[1].line, 7);
}

TEST(Linker, NegationSurvivesFlattening) {
  auto lk = link("3!d");
  EXPECT_TRUE(lk.prog[0].neg);
}

TEST(Linker, SubstOptSurvivesFlattening) {
  auto lk = link("s/foo/bar/g");
  EXPECT_TRUE(lk.prog[0].s.global);
  ASSERT_NE(lk.prog[0].s.re, nullptr);
  EXPECT_EQ(lk.prog[0].s.re->src, "foo");
}
