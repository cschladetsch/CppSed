# fastsed

A fast, feature-complete `sed(1)` clone written in C++23.

Supports the full POSIX command set plus GNU sed extensions: `e F Q R T W z`
step addresses (`first~step`), `s///` flags `i g e nth`, `\l \u \L \U \E` in
replacements, and `--sandbox` mode.

## Layout

```
fastsed/
  Include/fastsed/     Production headers (one per module)
  Source/              Production sources + Main.cpp
  Test/
    Include/           Test-only headers (TestHelper.hpp)
    Source/            GoogleTest suites (one per module + integration)
  Ext/googletest/      GoogleTest submodule (cloned on first build)
  Bin/                 All build artefacts (CMake intermediate + binaries)
    fastsed            Main binary
    Tests/
      fastsed_tests    Test binary
  b                    Build script
  rt                   Test runner
  CMakeLists.txt
```

## Quick start

```bash
./b          # configure + build (Release)
./b Debug    # debug build

./rt         # build then run all 148 tests
./rt --rebuild           # wipe Bin/, full rebuild, then run tests
./rt --filter Integration # run only tests matching a pattern
./rt --repeat 5          # run the suite 5 times
./rt --verbose           # print per-test timing
```

## Architecture

| Header | Responsibility |
|---|---|
| `OutBuf.hpp` | 64 KiB buffered writer — avoids per-line `write()` syscall |
| `Regex.hpp` | POSIX `regcomp`/`regexec` wrapper — compiled once, matched many |
| `Replacement.hpp` | Pre-parsed `s///` replacement token list |
| `Address.hpp` | Address kinds: line, `$`, `/re/`, `first~step` |
| `Command.hpp` | Parse tree (`Cmd`/`CmdVec`) and flat IR (`FlatCmd`) |
| `Parser.hpp` | Recursive-descent sed script parser |
| `Linker.hpp` | Flattens parse tree to `FlatCmd[]`, resolves label jumps |
| `Engine.hpp` | PC-based execution loop, hold space, deferred output |
| `LineSource.hpp` | `Boost.Iostreams` `mapped_file_source` + stdin lookahead |
| `Options.hpp` | `Boost.Program_options` CLI parser |

The engine never recurses into nested `{…}` blocks at runtime. The linker
converts them to a flat array with pre-computed jump indices, so `b`/`t`/`T`
and block entry/exit are O(1) branches.

## Boost components

| Component | Used for |
|---|---|
| `Boost.Iostreams` `mapped_file_source` | Zero-copy mmap of input files |
| `Boost.Program_options` | `--expression`, `--file`, `--sandbox`, `-i`, etc. |
| `Boost.Filesystem` | Temp file paths for in-place (`-i`) editing |
| `Boost.Process` | `exec_shell()` for the `e` command (replaces `popen`) |

POSIX `regcomp`/`regexec` is used instead of `Boost.Regex` — it is
measurably faster for the short patterns typical in sed scripts.

## Building on Windows

```powershell
cmake -B Bin `
      -DBOOST_ROOT="D:/boost/boost_1_91_0" `
      -DBOOST_LIBRARYDIR="D:/boost/boost_1_91_0/stage/lib" `
      -DCMAKE_BUILD_TYPE=Release
cmake --build Bin
```

## Tests

148 GoogleTest cases across five suites:

| Suite | Tests | Covers |
|---|---|---|
| `ParseRepl` / `ApplyRepl` | 18 | Replacement token parsing, case conversion, back-refs |
| `Parser` | 38 | Every address type, command, flag, block, negation |
| `Linker` | 18 | Flatten, jump targets, label resolution, death tests |
| `DoTrans` / `DoList` / `DoSubst` | 27 | `y`, `l`, `s` helpers with all flag combos |
| `Integration` | 47 | End-to-end via `run_sed()` in `Test/Include/TestHelper.hpp` |

`TestHelper.hpp` redirects `g_out` to a pipe and feeds input via a
`mkstemp` temp file, so each test is fully isolated with no global state
leakage.
