# Building `fsed`: A High-Performance C++23 Stream Editor

I’ve been working on `fsed` (the engine behind the [CppSed](https://github.com/cschladetsch/CppSed) project), a high-performance, feature-complete `sed(1)` clone written from scratch in modern C++23.

Stream editors are often considered legacy tools, but they remain a cornerstone of automation, scripting, and code refactoring workflows. However, standard system `sed` implementations can have unpredictable backend behaviors depending on whether you are on macOS (BSD sed) or Linux (GNU sed), and they are not optimized for memory-efficient multi-gigabyte stream operations.

With `fsed`, the goal was narrow and uncompromising:
*   **Complete POSIX compliance** + crucial GNU extensions (`e`, `F`, `Q`, `R`, `T`, `W`, `z`).
*   **Measurable, predictable performance** that scales linearly with file size.
*   **Comprehensive test coverage** to guarantee regressions are visible instantly.

Here are the key architectural decisions and engineering details that make `fsed` extremely fast.

---

## 🚀 1. Zero-Copy I/O via Memory-Mapped Files
Rather than reading files line-by-line via standard file pointers or streams—which causes continuous heap allocations, user-space buffer copies, and high context-switch syscall overhead—`fsed` utilizes memory-mapped files using `Boost.Iostreams` (`mapped_file_source`).

This maps the target file directly into the virtual address space of the process. The operating system handles file caching, page loading, and lookahead prefetching at the kernel level. This allows `fsed` to scan, match, and replace patterns directly inside raw memory addresses with zero intermediate copy buffers. Stdin falls back dynamically to an optimized lookahead buffer, keeping execution paths uniform.

---

## 🏛️ 2. The Jump-Flattening Linker ($O(1)$ Branches)
Typical interpreters parse script hierarchies and execute them recursively. For nested blocks (like `{ command1; { command2; command3; } }`) or conditional labels (`:label ... t label`), recursive execution wastes stack frames and incurs substantial pointer-chasing overhead.

`fsed` takes a compiler-like approach:
1. **Parsing**: A recursive-descent parser compiles the sed script into a tree AST (`CmdVec`).
2. **Linking**: The `Linker` flattens the AST into a single flat vector of instructions (`FlatCmd[]`). During this phase, it resolves all label jump targets (`b`, `t`, `T` commands) and block boundary entry/exit positions, replacing them with absolute vector indices.
3. **Execution**: At runtime, block boundaries and branching commands are resolved in **$O(1)$ time** by simply adjusting the program counter (instruction pointer) to the pre-calculated index. The engine never has to recurse or dynamically search for labels at execution time.

```mermaid
graph TD
    A[AST Parse Tree] -->|Linker| B[Flat IR Vector: FlatCmd Vector]
    B -->|Resolve Jumps| C[Pre-computed Index Jumps]
    C -->|Run Loop| D[Instruction Pointer: O(1) Execution]
```

---

## ⚡ 3. Native POSIX Regex vs. C++ standard library `std::regex`
In modern C++, standard library features are usually preferred. However, C++'s standard regular expression library (`std::regex`) is notoriously slow, heavily criticized in system programming, and suffers from high memory footprint and dynamic allocations during execution (especially in GNU's `libstdc++` and LLVM's `libc++`).

`fsed` makes a deliberate, performance-first trade-off by wrapping native POSIX `regcomp`/`regexec` C APIs. By compiling patterns exactly once and matching repeatedly over direct pointer offsets without heap-allocating intermediate search states, it achieves exceptional matching throughput. For short regex patterns typical in stream scripts, POSIX regex matching measurably outperforms both `std::regex` and `Boost.Regex`.

---

## 💾 4. Custom Output Buffering (`OutBuf`)
To prevent the terminal or filesystem output from becoming a bottleneck, `fsed` implements a custom 64 KiB buffered writer (`OutBuf.hpp`). 

By bundling multiple lines of output and executing standard `write()` system calls only when the buffer is filled (or upon stream completion), `fsed` eliminates the high overhead of per-line syscalls. This makes bulk transformations on massive files up to several times faster than unbuffered stream operations.

---

## 🧪 5. Bulletproof Parity via Differential Testing
When replacing a legacy tool, correctness is just as important as speed. To guarantee `fsed` behaves exactly like system `sed`, the test harness runs **348 test cases** organized across six specific suites. 

This includes **200 parameterized generated integration tests** that sweep combinations of deletions, substitutions, and transliterations. The test harness isolates stdin and stdout to detect even the slightest behavioral drift, assuring that `fsed` can be dropped in as a safe, drop-in replacement.

---

## 🎯 Summary
In practice, `fsed` proves that stream editing doesn't need to be a black box of legacy C code. By combining a flat instruction loop with modern C++23 features and smart system-level abstractions, we get a compiler-grade editor that is fast, safe, and easily embeddable.

*Check out the repository details and benchmark results in [README.md](README.md)!*
