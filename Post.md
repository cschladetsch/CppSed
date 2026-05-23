# Fast, Compatible, Measurable

I’ve been working on `fsed`, a fast, feature-complete `sed(1)` clone written in C++23.

The goals are straightforward:
- stay compatible with POSIX `sed`
- support the GNU extensions people actually use
- keep the implementation measurable, testable, and fast

Recent work focused on reducing hot-path overhead and making performance visible with tracked benchmark outputs. The benchmark results are versioned in the repo, so changes are easy to review instead of being hand-waved.

What I value in this kind of project:
- small, explicit interfaces
- reproducible benchmarks
- tests that lock down behavior
- a build that stays boring once it is working

There is still more to do, but the direction is clear: make the tool practical first, then make it faster without breaking correctness.
