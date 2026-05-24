// ============================================================
//  TestEngineHelpers.cpp  —  unit tests for the stateless
//  engine helpers: do_trans, do_list, do_subst
// ============================================================

#include "fastsed/Engine.hpp"
#include "fastsed/OutBuf.hpp"
#include <gtest/gtest.h>
#include <unistd.h>

using namespace fastsed;

// ── do_trans ──────────────────────────────────────────────────
static std::string trans(const std::string &from, const std::string &to,
                         std::string input) {
  std::string ytbl[2] = {from, to};
  do_trans(ytbl, input);
  return input;
}

TEST(DoTrans, IdentityMapping) { EXPECT_EQ(trans("abc", "abc", "abc"), "abc"); }

TEST(DoTrans, SimpleUppercase) {
  EXPECT_EQ(trans("abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                  "hello"),
            "HELLO");
}

TEST(DoTrans, PartialMapping) {
  EXPECT_EQ(trans("aeiou", "AEIOU", "hello world"), "hEllO wOrld");
}

TEST(DoTrans, EmptyInput) { EXPECT_EQ(trans("abc", "ABC", ""), ""); }

TEST(DoTrans, NewlineMapped) { EXPECT_EQ(trans("\n", "X", "a\nb"), "aXb"); }

TEST(DoTrans, CharMappedToItself) { EXPECT_EQ(trans("a", "a", "aaa"), "aaa"); }

TEST(DoTrans, ByteRangeStable) {
  // Characters not in the source table must not be changed
  EXPECT_EQ(trans("a", "b", "xyz"), "xyz");
}

// ── do_list ───────────────────────────────────────────────────
// Capture do_list output via pipe
static std::string list_output(const std::string &ps, long width = 70) {
  int pipefd[2];
  pipe(pipefd);
  OutBuf tmp(pipefd[1]);
  do_list(ps, width, tmp);
  tmp.flush();
  close(pipefd[1]);

  std::string out;
  char buf[4096];
  ssize_t n;
  while ((n = read(pipefd[0], buf, sizeof buf)) > 0)
    out.append(buf, static_cast<size_t>(n));
  close(pipefd[0]);
  return out;
}

TEST(DoList, EmptyInput) { EXPECT_EQ(list_output(""), "$\n"); }

TEST(DoList, PrintableStraightThrough) {
  EXPECT_EQ(list_output("hi"), "hi$\n");
}

TEST(DoList, EscapesTab) { EXPECT_EQ(list_output("\t"), "\\t$\n"); }

TEST(DoList, EscapesNewline) { EXPECT_EQ(list_output("\n"), "\\n$\n"); }

TEST(DoList, EscapesBackslash) { EXPECT_EQ(list_output("\\"), "\\\\$\n"); }

TEST(DoList, EscapesNonPrintable) {
  // BEL = 0x07 → \a
  EXPECT_EQ(list_output("\a"), "\\a$\n");
}

TEST(DoList, OctalEscape) {
  // 0x01 → \001
  std::string s(1, '\x01');
  EXPECT_EQ(list_output(s), "\\001$\n");
}

TEST(DoList, WrapsAtWidth) {
  // 10 'a' chars with width=5: should wrap
  std::string out = list_output("aaaaaaaaaa", 5);
  // Each line except last ends with backslash-newline
  EXPECT_NE(out.find("\\\n"), std::string::npos);
  // Last line ends with $\n
  EXPECT_EQ(out.back(), '\n');
  EXPECT_NE(out.rfind('$'), std::string::npos);
}

// ── do_subst ──────────────────────────────────────────────────
static std::string subst(const std::string &pattern,
                         const std::string &repl_str, std::string ps,
                         bool global = false, int nth = 0, bool icase = false,
                         bool ext = false) {
  SubstOpt s;
  s.re = std::make_shared<RE>();
  s.re->compile(pattern, (ext ? REG_EXTENDED : 0) | (icase ? REG_ICASE : 0));
  s.repl = parse_repl(repl_str);
  s.global = global;
  s.nth = nth;

  shared_ptr<RE> last;
  bool changed = do_subst(s, ps, last);
  (void)changed;
  return ps;
}

TEST(DoSubst, NoMatch) { EXPECT_EQ(subst("x", "y", "hello"), "hello"); }

TEST(DoSubst, SimpleReplace) {
  EXPECT_EQ(subst("hello", "world", "hello"), "world");
}

TEST(DoSubst, ReplaceFirst) { EXPECT_EQ(subst("a", "b", "aaa"), "baa"); }

TEST(DoSubst, ReplaceGlobal) {
  EXPECT_EQ(subst("a", "b", "aaa", /*global=*/true), "bbb");
}

TEST(DoSubst, ReplaceSecond) {
  EXPECT_EQ(subst("a", "b", "aaa", false, 2), "aba");
}

TEST(DoSubst, ReplaceThird) {
  EXPECT_EQ(subst("a", "b", "aaaa", false, 3), "aaba");
}

TEST(DoSubst, GlobalFromSecond) {
  // 2g: replace from 2nd occurrence onwards
  EXPECT_EQ(subst("a", "b", "aaaa", true, 2), "abbb");
}

TEST(DoSubst, WholeMatchRef) {
  EXPECT_EQ(subst("hel+o", "[&]", "hello", false, 0, false, true), "[hello]");
}

TEST(DoSubst, CaseInsensitive) {
  EXPECT_EQ(subst("hello", "X", "HELLO", false, 0, true), "X");
}

TEST(DoSubst, BackReference) {
  // POSIX BRE: \(pat\) → \1
  EXPECT_EQ(subst("\\(hel\\)lo", "\\1p", "hello"), "help");
}

TEST(DoSubst, ZeroLengthMatchDoesNotLoop) {
  // a* matches at every position — ensure we don't infinite-loop
  std::string result = subst("a*", "X", "bbb", /*global=*/true, 0, false, true);
  // Should terminate and produce a valid result
  EXPECT_FALSE(result.empty());
}

TEST(DoSubst, ReturnsTrueOnChange) {
  SubstOpt s;
  s.re = std::make_shared<RE>();
  s.re->compile("a", 0);
  s.repl = parse_repl("b");
  s.global = false;
  s.nth = 0;
  std::string ps = "a";
  shared_ptr<RE> last;
  EXPECT_TRUE(do_subst(s, ps, last));
  EXPECT_EQ(ps, "b");
}

TEST(DoSubst, ReturnsFalseOnNoChange) {
  SubstOpt s;
  s.re = std::make_shared<RE>();
  s.re->compile("x", 0);
  s.repl = parse_repl("y");
  std::string ps = "hello";
  shared_ptr<RE> last;
  EXPECT_FALSE(do_subst(s, ps, last));
  EXPECT_EQ(ps, "hello"); // unchanged
}

TEST(DoSubst, UsesLastReWhenPatternEmpty) {
  SubstOpt s;
  s.re = std::make_shared<RE>(); // not compiled → empty
  s.repl = parse_repl("X");
  auto last = std::make_shared<RE>();
  last->compile("hello", 0);
  std::string ps = "say hello";
  do_subst(s, ps, last);
  EXPECT_EQ(ps, "say X");
}
