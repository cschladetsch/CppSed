# CppSed / fastsed

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support/23)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A high-performance, feature-complete clone of `sed(1)` written in modern C++23. It implements the full POSIX stream editor command set along with high-value GNU sed extensions (such as in-place editing, case conversion, and sandbox mode).

---

## 🎯 Core Philosophy

`fastsed` was built with three strict principles:
1. **Keep it Correct**: Full POSIX standard compliance, thoroughly validated with a comprehensive integration test suite.
2. **Keep it Practical**: Native support for crucial GNU extensions (`e`, `F`, `Q`, `R`, `T`, `W`, `z`), step addresses, and in-place substitutions that developers use daily.
3. **Keep it Fast**: Zero unnecessary allocations on hot execution paths, zero-copy memory mapping for file inputs, and an optimizing jump-flattening compiler/linker model.

---

## 🏛️ Architecture & Execution Pipeline

Unlike traditional interpreter-based stream editors, `fastsed` parses scripts into a high-level AST, which a linker then flattens into a jump-optimized instruction list (Flat IR) before executing. This avoids deep recursion and yields $O(1)$ jumps for labels, block entry/exit (`{...}`), and conditional branches (`t`, `T`, `b`).

```mermaid
graph TD
    A[CLI Input / Options] --> B(Parser)
    B -->|Parse Tree: CmdVec| C(Linker)
    C -->|Flat IR: FlatCmd[] with Pre-computed Jumps| D(Engine)
    E[(Input Files / stdin)] -->|mmap / LineSource| D
    D -->|Line Loop / Regex / Exec| F(OutBuf)
    F -->|Buffered write| G[stdout / In-place file]
```

### Component Breakdown

| Header | Responsibility | Description |
| :--- | :--- | :--- |
| `Options.hpp` | CLI Arguments | Handles program options (`--expression`, `--file`, `--sandbox`, `-i`, etc.) via `Boost.Program_options`. |
| `Parser.hpp` | AST Generation | A recursive-descent parser that compiles raw sed scripts into an abstract syntax tree of address-command pairings. |
| `Linker.hpp` | Branch Resolution | Flattens the parsed AST into a flat instruction vector (`FlatCmd[]`) and resolves branch target labels to absolute indices for $O(1)$ runtime jumps. |
| `LineSource.hpp` | High-Speed I/O | Feeds lines efficiently using `Boost.Iostreams` memory-mapped files (`mapped_file_source`) with fallback stdin lookahead buffering. |
| `Regex.hpp` | Regular Expressions | POSIX `regcomp`/`regexec` wrapper. By compiling patterns once and matching repeatedly, it significantly outperforms `Boost.Regex` for standard sed workloads. |
| `Replacement.hpp` | Substitution Parsing | Parses substitution patterns (`s/find/replace/`) into specialized token structures to efficiently execute backreferences and case conversions. |
| `Engine.hpp` | Execution Core | A high-performance instruction pointer loop carrying out actions on the flat instruction stream, keeping state in hold and pattern spaces. |
| `OutBuf.hpp` | Buffered Output | A custom 64 KiB buffered stdout writer, bypassing per-line system call overheads to prevent output bottlenecks. |

---

## ⚡ Boost Dependencies

`fastsed` leverages Boost for system-level integrations while keeping regex operations in native POSIX space:

| Boost Component | Usage |
| :--- | :--- |
| `Boost.Iostreams` | Zero-copy `mapped_file_source` mmap of input files. |
| `Boost.Program_options` | Standard and clean CLI option parsing and validation. |
| `Boost.Filesystem` | Safe temporary file path handling for reliable in-place (`-i`) file edits. |
| `Boost.Process` | Shell-out operations via `exec_shell` for the `e` command, replacing slower `popen` calls. |

---

## 🛠️ Build and Installation

### Prerequisites
- **Compiler**: A C++23 compliant compiler (e.g., GCC 13+, Clang 16+, or MSVC 2022).
- **Build System**: CMake 3.20+
- **Libraries**: Boost libraries 1.70+

### Linux / macOS

We provide helper scripts for typical configurations:

```bash
# Clone the repository and submodules
git clone https://github.com/cschladetsch/CppSed.git
cd CppSed

# Configure and compile in Release mode (builds into Bin/)
./b

# Build in Debug mode
./b Debug

# Clean and perform a full rebuild
./rt --rebuild

# Run all 348 unit/integration tests
./rt
```

To install the built executable as `fsed` and install the man page:
```bash
# Default prefix is ~/.local (user) or /usr/local (root)
./install.sh

# Install to custom directory
./install.sh --prefix "/opt/fastsed"
```

### Windows (PowerShell)

Ensure Boost is installed and set the paths accordingly:

```powershell
cmake -B Bin `
      -DBOOST_ROOT="C:/local/boost_1_84_0" `
      -DBOOST_LIBRARYDIR="C:/local/boost_1_84_0/lib64-msvc-14.3" `
      -DCMAKE_BUILD_TYPE=Release
cmake --build Bin --config Release
```

---

## 📖 Command Reference & Examples

### Basic Substitutions & Prints
```bash
# Standard substitution (replaces first occurrence on line)
echo 'alpha beta' | ./Bin/fastsed 's/beta/gamma/'

# Global replacement with case insensitivity (GNU extension)
echo 'Alpha alpha' | ./Bin/fastsed 's/alpha/beta/gi'

# Print ONLY matching lines (-n suppresses automatic printing)
./Bin/fastsed -n '/[Ee]rror/p' server.log
```

### GNU Sed Extensions
```bash
# Step addresses: Apply command to every 3rd line starting at line 2
seq 10 | ./Bin/fastsed -n '2~3p'

# In-place file editing with backup generation
./Bin/fastsed -i.bak 's/localhost/db.internal/g' config.yaml

# Evaluate replacement string as a shell command (GNU 'e' flag)
echo 'date' | ./Bin/fastsed 's/.*/&/e'

# Process lines delimited by NUL (\0) instead of newline (\n)
printf 'first\0second\0third\0' | ./Bin/fastsed -z 's/second/SECOND/'
```

### Advanced Case Control in Substitution
```bash
# Convert matching group to lowercase (\L...\E) or uppercase (\U...\E)
echo 'HELLO world' | ./Bin/fastsed -E 's/(HELLO) (world)/\L\1\E \U\2\E/'
# Output: hello WORLD
```

### 🔒 Sandbox Mode
For security-conscious environments, you can disable commands that interface with the shell or the filesystem (such as `e`, `r`, `R`, `w`, and `W`):

```bash
# Will abort if execution contains shell commands or file reads/writes
./Bin/fastsed --sandbox -f script.sed input.txt
```

---

## 📊 Benchmarks

The benchmark suite compares `fastsed` execution times directly against native `/usr/bin/sed`. Results are stored under `Benchmark/Results/`.

To run the benchmarking suite locally:
```bash
./Benchmark/run.sh --runs 10 --warmup 3
```

---

## 🧪 Testing Suite

`fastsed` features robust test coverage consisting of **348 test cases** organized across six specific suites to prevent regressions:

| Suite | Focus | Covers |
| :--- | :--- | :--- |
| `ParseRepl` / `ApplyRepl` | Replacements | Regex substitutions, backreferences, and case-conversion tokens. |
| `Parser` | Grammar | Addresses, Address-ranges, command-structures, blocks, and negation operators. |
| `Linker` | Optimization | AST flattening, label indexes, jump resolution, and error validation. |
| `DoTrans` / `DoList` / `DoSubst` | Stream Helpers | In-memory operations for `y`, `l`, and `s` commands. |
| `Integration` | End-to-end | Execution against redirected buffers and CLI parameter sweeps. |
| `Generated` | Differential | 200 parameterized tests verifying behavior parity. |

Run tests at any time with:
```bash
./rt
```
