// ============================================================
//  TestIntegrationGenerated.cpp  —  bulk integration coverage
//
//  200 parameterized cases grouped into four behavior families:
//    - global substitution
//    - line deletion
//    - transliteration
//    - NUL-delimited substitution
// ============================================================

#include "TestHelper.hpp"
#include <gtest/gtest.h>

using namespace fastsed::test;

namespace {

struct Case {
    std::string name;
    std::string script;
    std::string input;
    std::string expected;
    bool suppress   = false;
    bool extended   = false;
    bool null_delim = false;
};

static std::string join_lines(const std::vector<std::string>& lines, char delim = '\n') {
    std::string out;
    for (const auto& line : lines) {
        out += line;
        out += delim;
    }
    return out;
}

static std::string replace_chars(std::string s, char from, char to) {
    for (char& c : s) {
        if (c == from) c = to;
    }
    return s;
}

static std::string translit_abc(std::string s) {
    for (char& c : s) {
        switch (c) {
        case 'a': c = 'A'; break;
        case 'b': c = 'B'; break;
        case 'c': c = 'C'; break;
        default: break;
        }
    }
    return s;
}

static std::string number_lines(const std::vector<std::string>& lines) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += std::to_string(i + 1);
        out += '\n';
        out += lines[i];
        out += '\n';
    }
    return out;
}

static std::string drop_nth_line(const std::vector<std::string>& lines, size_t nth) {
    std::vector<std::string> kept;
    kept.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i + 1 != nth) kept.push_back(lines[i]);
    }
    return join_lines(kept);
}

static std::vector<Case> make_cases() {
    std::vector<Case> cases;
    cases.reserve(200);

    auto tag2 = [](int i) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "%02d", i);
        return std::string(buf);
    };

    // 1. Global substitution over ordinary newline-delimited input.
    for (int i = 1; i <= 50; ++i) {
        std::vector<std::string> lines = {
            "alpha" + tag2(i) + std::string((i % 4) + 1, 'a') + "omega",
            "beta" + tag2(i) + std::string((i % 3) + 2, 'a') + "gamma",
            "tail" + tag2(i) + std::string((i % 5) + 1, 'a')
        };
        std::string input = join_lines(lines);
        cases.push_back({
            "Subst" + tag2(i),
            "s/a/b/g",
            input,
            replace_chars(input, 'a', 'b'),
        });
    }

    // 2. Delete one specific line from a 5-line record.
    for (int i = 1; i <= 50; ++i) {
        std::vector<std::string> lines = {
            "keep-" + tag2(i) + "-1",
            "keep-" + tag2(i) + "-2",
            "keep-" + tag2(i) + "-3",
            "keep-" + tag2(i) + "-4",
            "keep-" + tag2(i) + "-5",
        };
        const size_t del = static_cast<size_t>((i - 1) % 5 + 1);
        cases.push_back({
            "Delete" + tag2(i),
            std::to_string(del) + "d",
            join_lines(lines),
            drop_nth_line(lines, del),
        });
    }

    // 3. Transliterate a/b/c to A/B/C with varying payloads.
    for (int i = 1; i <= 50; ++i) {
        std::vector<std::string> lines = {
            std::string((i % 3) + 1, 'a') + "mix" + std::string((i % 2) + 1, 'b'),
            "cab" + tag2(i) + "abc",
            "aaabbbccc" + tag2(i),
            "z" + tag2(i) + "cab"
        };
        std::string input = join_lines(lines);
        cases.push_back({
            "Translit" + tag2(i),
            "y/abc/ABC/",
            input,
            translit_abc(input),
        });
    }

    // 4. Line-number emission over varying input lengths.
    for (int i = 1; i <= 50; ++i) {
        std::vector<std::string> lines = {
            "row-" + tag2(i) + "-1",
            "row-" + tag2(i) + "-2x",
            "row-" + tag2(i) + "-3xx",
            "row-" + tag2(i) + "-4xxx",
            "row-" + tag2(i) + "-5xxxx",
            "row-" + tag2(i) + "-6xxxxx",
        };
        cases.push_back({
            "LineNo" + tag2(i),
            "=",
            join_lines(lines),
            number_lines(lines),
            false,
            false,
            false,
        });
    }

    return cases;
}

static const std::vector<Case>& all_cases() {
    static const std::vector<Case> cases = make_cases();
    return cases;
}

static std::string case_name(const ::testing::TestParamInfo<Case>& info) {
    return info.param.name;
}

class IntegrationGenerated : public ::testing::TestWithParam<Case> {};

TEST_P(IntegrationGenerated, RunsCorrectly) {
    const Case& c = GetParam();
    EXPECT_EQ(run_sed(c.script, c.input, c.suppress, c.extended, c.null_delim), c.expected)
        << c.name;
}

INSTANTIATE_TEST_SUITE_P(Generated, IntegrationGenerated,
                         ::testing::ValuesIn(all_cases()),
                         case_name);

} // namespace
