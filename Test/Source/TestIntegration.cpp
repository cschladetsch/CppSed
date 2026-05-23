// ============================================================
//  TestIntegration.cpp  —  end-to-end sed integration tests
//  Uses run_sed() from TestHelper.hpp which compiles a script,
//  feeds input through LineSource, and captures g_out output.
// ============================================================

#include "TestHelper.hpp"
#include <gtest/gtest.h>

using namespace fastsed::test;

// ── Substitution ──────────────────────────────────────────────
TEST(Integration, SubstSimple) {
    EXPECT_EQ(run_sed("s/hello/world/", "hello\n"), "world\n");
}

TEST(Integration, SubstGlobal) {
    EXPECT_EQ(run_sed("s/a/b/g", "aaa\n"), "bbb\n");
}

TEST(Integration, SubstNth) {
    EXPECT_EQ(run_sed("s/a/X/2", "aaa\n"), "aXa\n");
}

TEST(Integration, SubstGlobalFromNth) {
    EXPECT_EQ(run_sed("s/a/X/2g", "aaaa\n"), "aXXX\n");
}

TEST(Integration, SubstNoMatch) {
    EXPECT_EQ(run_sed("s/x/y/", "hello\n"), "hello\n");
}

TEST(Integration, SubstBackRef) {
    // POSIX BRE groups
    EXPECT_EQ(run_sed("s/\\(hel\\)lo/\\1p/", "hello\n"), "help\n");
}

TEST(Integration, SubstAmpersand) {
    EXPECT_EQ(run_sed("s/world/[&]/", "hello world\n"), "hello [world]\n");
}

TEST(Integration, SubstPrintFlag) {
    // s///p always prints the substituted line, even with -n
    EXPECT_EQ(run_sed("s/a/b/p", "a\n", /*suppress=*/true), "b\n");
}

TEST(Integration, SubstMultipleLines) {
    EXPECT_EQ(run_sed("s/x/y/g", "axa\nxbx\n"), "aya\nyby\n");
}

// ── Delete ───────────────────────────────────────────────────
TEST(Integration, DeleteLine) {
    EXPECT_EQ(run_sed("2d", "one\ntwo\nthree\n"), "one\nthree\n");
}

TEST(Integration, DeleteFirst) {
    EXPECT_EQ(run_sed("1d", "a\nb\n"), "b\n");
}

TEST(Integration, DeleteLast) {
    EXPECT_EQ(run_sed("$d", "a\nb\n"), "a\n");
}

TEST(Integration, DeleteAll) {
    EXPECT_EQ(run_sed("d", "a\nb\n"), "");
}

// ── Print / suppress ─────────────────────────────────────────
TEST(Integration, PrintWithSuppressN) {
    EXPECT_EQ(run_sed("2p", "a\nb\nc\n", /*suppress=*/true), "b\n");
}

TEST(Integration, PrintDuplicatesLine) {
    EXPECT_EQ(run_sed("1p", "a\nb\n"), "a\na\nb\n");
}

TEST(Integration, PrintFirstOfMultiline) {
    // P prints up to first \n in pattern space (after N)
    EXPECT_EQ(run_sed("N\nP", "a\nb\n"), "a\na\nb\n");
}

// ── Address types ─────────────────────────────────────────────
TEST(Integration, LineNumberAddress) {
    EXPECT_EQ(run_sed("3s/x/y/", "x\nx\nx\n"), "x\nx\ny\n");
}

TEST(Integration, LastLineAddress) {
    EXPECT_EQ(run_sed("$s/x/LAST/", "x\nx\n"), "x\nLAST\n");
}

TEST(Integration, RegexAddress) {
    EXPECT_EQ(run_sed("/b/d", "a\nb\nc\n"), "a\nc\n");
}

TEST(Integration, RangeAddress) {
    EXPECT_EQ(run_sed("2,3d", "1\n2\n3\n4\n"), "1\n4\n");
}

TEST(Integration, RegexRangeAddress) {
    EXPECT_EQ(run_sed("/start/,/end/d", "a\nstart\nmid\nend\nb\n"), "a\nb\n");
}

TEST(Integration, StepAddressEvenLines) {
    // 0~2: every 2nd line starting at 0 → lines 2, 4
    EXPECT_EQ(run_sed("0~2d", "1\n2\n3\n4\n"), "1\n3\n");
}

TEST(Integration, StepAddressOddLines) {
    EXPECT_EQ(run_sed("1~2d", "1\n2\n3\n4\n"), "2\n4\n");
}

TEST(Integration, NegatedAddress) {
    // !d: delete every line except line 2
    EXPECT_EQ(run_sed("2!d", "a\nb\nc\n"), "b\n");
}

// ── Hold space ───────────────────────────────────────────────
TEST(Integration, HoldGetReplace) {
    // h on line 1; g on line 2 → both lines print "a"
    EXPECT_EQ(run_sed("1h\n2g", "a\nb\n"), "a\na\n");
}

TEST(Integration, HoldAppendGet) {
    // H appends; G appends hold to pattern
    EXPECT_EQ(run_sed("H\n$!d\ng", "a\nb\n"), "\na\nb\n");
}

TEST(Integration, Exchange) {
    EXPECT_EQ(run_sed("1{h;d}\n2x", "foo\nbar\n"), "foo\n");
}

// ── Multiline (N / D / P) ────────────────────────────────────
TEST(Integration, AppendNextLine) {
    // N joins two lines with \n; p prints both
    EXPECT_EQ(run_sed("N\np", "a\nb\n", /*suppress=*/true), "a\nb\n");
}

TEST(Integration, DeleteFirstLineRestart) {
    // D deletes first line of PS, restarts without reading
    EXPECT_EQ(run_sed("N\nD", "a\nb\nc\n"), "c\n");
}

// ── Branch / label ────────────────────────────────────────────
TEST(Integration, UnconditionalBranch) {
    // b end skips everything, print runs default output
    EXPECT_EQ(run_sed("b end\ns/a/NEVER/\n:end", "a\n"), "a\n");
}

TEST(Integration, BranchIfSubstitution) {
    // Loop: keep replacing a→b until no 'a' left — tests t clears flag
    EXPECT_EQ(run_sed(":loop\ns/a/b/\nt loop", "aaa\n"), "bbb\n");
}

TEST(Integration, BranchIfNoSubstitution) {
    // T branches when s/// did NOT match
    EXPECT_EQ(run_sed("s/x/y/\nT done\ns/a/SKIPPED/\n:done", "a\n"), "a\n");
}

// ── Insert / Append / Change ─────────────────────────────────
TEST(Integration, InsertBeforeLine) {
    EXPECT_EQ(run_sed("2i\\ INSERTED", "a\nb\n"), "a\n INSERTED\nb\n");
}

TEST(Integration, AppendAfterLine) {
    EXPECT_EQ(run_sed("1a\\ APPENDED", "a\nb\n"), "a\n APPENDED\nb\n");
}

TEST(Integration, ChangeLine) {
    EXPECT_EQ(run_sed("2c\\ CHANGED", "a\nb\nc\n"), "a\n CHANGED\nc\n");
}

TEST(Integration, ChangeRangeLastLineOnly) {
    // c with 2-addr range prints text once on last matching line
    EXPECT_EQ(run_sed("1,3c\\ REPLACED", "a\nb\nc\nd\n"), " REPLACED\nd\n");
}

// ── Transliterate ────────────────────────────────────────────
TEST(Integration, Transliterate) {
    EXPECT_EQ(run_sed("y/abc/ABC/", "abc\n"), "ABC\n");
}

TEST(Integration, TransliteratePartial) {
    EXPECT_EQ(run_sed("y/aeiou/AEIOU/", "hello\n"), "hEllO\n");
}

// ── Zap / = / F ───────────────────────────────────────────────
TEST(Integration, ZapClearsPatternSpace) {
    EXPECT_EQ(run_sed("z", "hello\n"), "\n");
}

TEST(Integration, PrintLineNumber) {
    EXPECT_EQ(run_sed("=", "a\nb\n"), "1\na\n2\nb\n");
}

// ── Quit ─────────────────────────────────────────────────────
TEST(Integration, QuitAfterFirstLine) {
    EXPECT_EQ(run_sed("1q", "a\nb\nc\n"), "a\n");
}

TEST(Integration, QuitSilent) {
    // Q: quit without printing current line
    EXPECT_EQ(run_sed("2Q", "a\nb\nc\n"), "a\n");
}

// ── Multiple scripts separated by semicolons ─────────────────
TEST(Integration, ChainedCommands) {
    EXPECT_EQ(run_sed("s/a/b/;s/b/c/", "a\n"), "c\n");
}

// ── Empty input ───────────────────────────────────────────────
TEST(Integration, EmptyInput) {
    EXPECT_EQ(run_sed("s/a/b/", ""), "");
}

// ── Extended regex (-E) ───────────────────────────────────────
TEST(Integration, ExtendedRegexPlus) {
    EXPECT_EQ(run_sed("s/a+/X/", "aaa\n", false, /*extended=*/true), "X\n");
}

TEST(Integration, ExtendedRegexAlternation) {
    EXPECT_EQ(run_sed("s/cat|dog/pet/g", "cat and dog\n", false, true),
              "pet and pet\n");
}

TEST(Integration, ExtendedRegexGroups) {
    EXPECT_EQ(run_sed("s/(hel)lo/\\1p/", "hello\n", false, true), "help\n");
}
