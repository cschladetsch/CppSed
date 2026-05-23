// ============================================================
//  TestReplacement.cpp  —  unit tests for parse_repl / apply_repl
// ============================================================

#include "fastsed/Replacement.hpp"
#include "fastsed/Regex.hpp"
#include <gtest/gtest.h>
#include <regex.h>

using namespace fastsed;

// ── Helpers ───────────────────────────────────────────────────
// Run a POSIX regex match and return regmatch_t array
static std::vector<regmatch_t> do_match(const std::string& pattern,
                                         const std::string& text,
                                         size_t nmatch = 10) {
    regex_t re;
    EXPECT_EQ(regcomp(&re, pattern.c_str(), REG_EXTENDED), 0);
    std::vector<regmatch_t> pm(nmatch);
    int r = regexec(&re, text.c_str(), nmatch, pm.data(), 0);
    regfree(&re);
    if (r != 0) pm[0].rm_so = -1;
    return pm;
}

static std::string apply(const std::string& repl_str,
                          const std::string& pattern,
                          const std::string& text) {
    auto rv = parse_repl(repl_str);
    auto pm = do_match(pattern, text);
    return apply_repl(rv, text.c_str(), pm.data(), static_cast<int>(pm.size()));
}

// ── parse_repl ────────────────────────────────────────────────
TEST(ParseRepl, EmptyReplacement) {
    auto rv = parse_repl("");
    EXPECT_TRUE(rv.empty());
}

TEST(ParseRepl, LiteralOnly) {
    auto rv = parse_repl("hello");
    ASSERT_EQ(rv.size(), 1u);
    EXPECT_EQ(rv[0].k,   RTK::Lit);
    EXPECT_EQ(rv[0].lit, "hello");
}

TEST(ParseRepl, WholeMatchAmpersand) {
    auto rv = parse_repl("(&)");
    ASSERT_EQ(rv.size(), 3u);
    EXPECT_EQ(rv[0].k,   RTK::Lit); EXPECT_EQ(rv[0].lit, "(");
    EXPECT_EQ(rv[1].k,   RTK::All);
    EXPECT_EQ(rv[2].k,   RTK::Lit); EXPECT_EQ(rv[2].lit, ")");
}

TEST(ParseRepl, EscapedAmpersand) {
    auto rv = parse_repl("\\&");
    ASSERT_EQ(rv.size(), 1u);
    EXPECT_EQ(rv[0].k,   RTK::Lit);
    EXPECT_EQ(rv[0].lit, "&");
}

TEST(ParseRepl, BackReference) {
    auto rv = parse_repl("\\1-\\2");
    ASSERT_EQ(rv.size(), 3u);
    EXPECT_EQ(rv[0].k,   RTK::Ref); EXPECT_EQ(rv[0].ref, 1);
    EXPECT_EQ(rv[1].k,   RTK::Lit); EXPECT_EQ(rv[1].lit, "-");
    EXPECT_EQ(rv[2].k,   RTK::Ref); EXPECT_EQ(rv[2].ref, 2);
}

TEST(ParseRepl, NewlineEscape) {
    auto rv = parse_repl("a\\nb");
    ASSERT_EQ(rv.size(), 1u);
    EXPECT_EQ(rv[0].k,   RTK::Lit);
    EXPECT_EQ(rv[0].lit, "a\nb");
}

TEST(ParseRepl, CaseTokens) {
    auto rv = parse_repl("\\u\\L\\E");
    ASSERT_EQ(rv.size(), 3u);
    EXPECT_EQ(rv[0].k, RTK::UpperC);
    EXPECT_EQ(rv[1].k, RTK::LowerOn);
    EXPECT_EQ(rv[2].k, RTK::CaseOff);
}

TEST(ParseRepl, UnknownEscapePassthrough) {
    auto rv = parse_repl("\\x");
    ASSERT_EQ(rv.size(), 1u);
    EXPECT_EQ(rv[0].k,   RTK::Lit);
    EXPECT_EQ(rv[0].lit, "x");
}

// ── apply_repl ────────────────────────────────────────────────
TEST(ApplyRepl, LiteralReplacement) {
    EXPECT_EQ(apply("world", "hello", "hello"), "world");
}

TEST(ApplyRepl, WholeMatch) {
    EXPECT_EQ(apply("[&]", "foo", "foo bar"), "[foo]");
}

TEST(ApplyRepl, BackRef1) {
    EXPECT_EQ(apply("\\1!", "(hello) world", "hello world"), "hello!");
}

TEST(ApplyRepl, BackRef2) {
    EXPECT_EQ(apply("\\2-\\1", "(a)(b)", "ab"), "b-a");
}

TEST(ApplyRepl, UpperNextChar) {
    // \u uppercases the next character of the replacement output
    EXPECT_EQ(apply("\\uhello", ".*", "anything"), "Hello");
}

TEST(ApplyRepl, LowerNextChar) {
    EXPECT_EQ(apply("\\lHELLO", ".*", "anything"), "hELLO");
}

TEST(ApplyRepl, UpperOnRange) {
    EXPECT_EQ(apply("\\Uhello\\E world", ".*", "x"), "HELLO world");
}

TEST(ApplyRepl, LowerOnRange) {
    EXPECT_EQ(apply("\\LHELLO\\E world", ".*", "x"), "hello world");
}

TEST(ApplyRepl, EmptyReplacement) {
    EXPECT_EQ(apply("", "foo", "foo bar"), "");
}

TEST(ApplyRepl, NewlineInReplacement) {
    std::string result = apply("a\\nb", "x", "x");
    EXPECT_EQ(result, "a\nb");
}
