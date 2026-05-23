// ============================================================
//  TestParser.cpp  —  unit tests for the sed script parser
// ============================================================

#include "fastsed/Parser.hpp"
#include <gtest/gtest.h>

using namespace fastsed;

// ── Helpers ───────────────────────────────────────────────────
static CmdVec parse(const std::string& script, bool ext = false) {
    Parser p; p.src = script; p.ext = ext;
    return p.parse_cmds();
}

static Cmd first(const std::string& script, bool ext = false) {
    auto cmds = parse(script, ext);
    EXPECT_FALSE(cmds.empty());
    return cmds.empty() ? Cmd{} : cmds[0];
}

// ── Address: no address ───────────────────────────────────────
TEST(Parser, NoAddress) {
    auto c = first("d");
    EXPECT_EQ(c.op,    'd');
    EXPECT_EQ(c.naddr, 0);
    EXPECT_FALSE(c.neg);
}

// ── Address: line number ───────────────────────────────────────
TEST(Parser, LineAddress) {
    auto c = first("42d");
    EXPECT_EQ(c.naddr,     1);
    EXPECT_EQ(c.a[0].k,    AK::Line);
    EXPECT_EQ(c.a[0].line, 42);
}

TEST(Parser, LineZeroAddress) {
    auto c = first("0,/pat/d");
    EXPECT_EQ(c.a[0].k, AK::Line0);
}

// ── Address: $ ────────────────────────────────────────────────
TEST(Parser, LastLineAddress) {
    auto c = first("$p");
    EXPECT_EQ(c.naddr,  1);
    EXPECT_EQ(c.a[0].k, AK::Last);
}

// ── Address: regex ────────────────────────────────────────────
TEST(Parser, RegexAddress) {
    auto c = first("/hello/d");
    EXPECT_EQ(c.a[0].k, AK::RE);
    ASSERT_NE(c.a[0].re, nullptr);
    EXPECT_TRUE(c.a[0].re->ok);
    EXPECT_EQ(c.a[0].re->src, "hello");
}

TEST(Parser, RegexAddressAlternateDelimiter) {
    auto c = first("\\|hello|d");
    EXPECT_EQ(c.a[0].k, AK::RE);
    EXPECT_EQ(c.a[0].re->src, "hello");
}

TEST(Parser, EmptyRegexAddress) {
    // empty // → re-use last (re->ok == false)
    auto c = first("//d");
    EXPECT_EQ(c.a[0].k, AK::RE);
    ASSERT_NE(c.a[0].re, nullptr);
    EXPECT_FALSE(c.a[0].re->ok);
}

// ── Address: step (first~step) ────────────────────────────────
TEST(Parser, StepAddress) {
    auto c = first("0~2d");
    EXPECT_EQ(c.a[0].k,     AK::Step);
    EXPECT_EQ(c.a[0].first, 0);
    EXPECT_EQ(c.a[0].step,  2);
}

TEST(Parser, StepAddressOdd) {
    auto c = first("1~2p");
    EXPECT_EQ(c.a[0].first, 1);
    EXPECT_EQ(c.a[0].step,  2);
}

// ── Two-address range ─────────────────────────────────────────
TEST(Parser, TwoAddressRange) {
    auto c = first("3,7d");
    EXPECT_EQ(c.naddr,     2);
    EXPECT_EQ(c.a[0].line, 3);
    EXPECT_EQ(c.a[1].line, 7);
}

TEST(Parser, TwoAddressRegexRange) {
    auto c = first("/start/,/end/d");
    EXPECT_EQ(c.a[0].k, AK::RE);
    EXPECT_EQ(c.a[1].k, AK::RE);
}

TEST(Parser, TwoAddressMixed) {
    auto c = first("2,/end/d");
    EXPECT_EQ(c.a[0].k, AK::Line);
    EXPECT_EQ(c.a[1].k, AK::RE);
}

// ── Negation ──────────────────────────────────────────────────
TEST(Parser, NegatedCommand) {
    auto c = first("3!d");
    EXPECT_TRUE(c.neg);
    EXPECT_EQ(c.a[0].line, 3);
}

TEST(Parser, DoubleNegationCancels) {
    auto c = first("3!!d");
    EXPECT_FALSE(c.neg);
}

// ── Commands without arguments ────────────────────────────────
TEST(Parser, CommandsNoArgs) {
    for (char op : {'d','D','g','G','h','H','n','N','p','P','x','z','='}) {
        auto c = first(std::string(1, op));
        EXPECT_EQ(c.op, op) << "op=" << op;
    }
}

// ── s command ────────────────────────────────────────────────
TEST(Parser, SubstBasic) {
    auto c = first("s/foo/bar/");
    EXPECT_EQ(c.op, 's');
    ASSERT_NE(c.s.re, nullptr);
    EXPECT_EQ(c.s.re->src, "foo");
    ASSERT_FALSE(c.s.repl.empty());
    EXPECT_EQ(c.s.repl[0].lit, "bar");
    EXPECT_FALSE(c.s.global);
    EXPECT_FALSE(c.s.print);
}

TEST(Parser, SubstGlobal) {
    auto c = first("s/a/b/g");
    EXPECT_TRUE(c.s.global);
}

TEST(Parser, SubstPrint) {
    auto c = first("s/a/b/p");
    EXPECT_TRUE(c.s.print);
}

TEST(Parser, SubstNth) {
    auto c = first("s/a/b/3");
    EXPECT_EQ(c.s.nth, 3);
}

TEST(Parser, SubstIcase) {
    auto c = first("s/a/b/i");
    EXPECT_TRUE(c.s.icase);
}

TEST(Parser, SubstMultipleFlags) {
    auto c = first("s/a/b/gp");
    EXPECT_TRUE(c.s.global);
    EXPECT_TRUE(c.s.print);
}

TEST(Parser, SubstAlternateDelimiter) {
    auto c = first("s|foo|bar|");
    EXPECT_EQ(c.s.re->src, "foo");
}

// ── y command ────────────────────────────────────────────────
TEST(Parser, TranslitBasic) {
    auto c = first("y/abc/ABC/");
    EXPECT_EQ(c.op,       'y');
    EXPECT_EQ(c.ytbl[0],  "abc");
    EXPECT_EQ(c.ytbl[1],  "ABC");
}

TEST(Parser, TranslitNewlineEscape) {
    auto c = first("y/n/\\n/");
    EXPECT_EQ(c.ytbl[1], "\n");
}

// ── Text commands ─────────────────────────────────────────────
TEST(Parser, AppendText) {
    auto c = first("a\\ added text");
    EXPECT_EQ(c.op,   'a');
    EXPECT_EQ(c.text, " added text"); // leading space preserved (POSIX behaviour)
}

TEST(Parser, InsertText) {
    auto c = first("i\\ inserted");
    EXPECT_EQ(c.op,   'i');
    EXPECT_EQ(c.text, " inserted"); // leading space preserved
}

// ── Labels and branches ───────────────────────────────────────
TEST(Parser, LabelDefinition) {
    auto c = first(": myloop");
    EXPECT_EQ(c.op,   ':');
    EXPECT_EQ(c.text, "myloop");
}

TEST(Parser, BranchUnconditional) {
    auto c = first("b end");
    EXPECT_EQ(c.op,   'b');
    EXPECT_EQ(c.text, "end");
}

TEST(Parser, BranchEmpty) {
    auto c = first("b");
    EXPECT_EQ(c.op,   'b');
    EXPECT_TRUE(c.text.empty());
}

// ── Block ────────────────────────────────────────────────────
TEST(Parser, BlockWithCommands) {
    auto c = first("/foo/ { p\n d\n }");
    EXPECT_EQ(c.op,          '{');
    EXPECT_EQ(c.block.size(), 2u);
    EXPECT_EQ(c.block[0].op, 'p');
    EXPECT_EQ(c.block[1].op, 'd');
}

// ── q with exit code ─────────────────────────────────────────
TEST(Parser, QuitExitCode) {
    auto c = first("q 42");
    EXPECT_EQ(c.op,    'q');
    EXPECT_EQ(c.ecode, 42);
}

// ── Multiple commands ─────────────────────────────────────────
TEST(Parser, MultipleCommandsSemicolon) {
    auto cmds = parse("p;d");
    ASSERT_EQ(cmds.size(), 2u);
    EXPECT_EQ(cmds[0].op, 'p');
    EXPECT_EQ(cmds[1].op, 'd');
}

TEST(Parser, MultipleCommandsNewline) {
    auto cmds = parse("p\nd\n");
    ASSERT_EQ(cmds.size(), 2u);
}

// ── Comment ───────────────────────────────────────────────────
TEST(Parser, CommentIsIgnored) {
    auto cmds = parse("# this is a comment\np");
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].op, 'p');
}

// ── #n detection ─────────────────────────────────────────────
TEST(Parser, HashNDetected) {
    Parser p; p.src = "#n\np";
    EXPECT_TRUE(p.first_line_is_hash_n());
}

TEST(Parser, HashNNotFirst) {
    Parser p; p.src = "p\n#n";
    EXPECT_FALSE(p.first_line_is_hash_n());
}

// ── l width ──────────────────────────────────────────────────
TEST(Parser, ListDefaultWidth) {
    auto c = first("l");
    EXPECT_EQ(c.lwidth, 70);
}

TEST(Parser, ListCustomWidth) {
    auto c = first("l 40");
    EXPECT_EQ(c.lwidth, 40);
}
