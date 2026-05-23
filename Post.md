# `fsed`

I’ve been working on `fsed`, a fast `sed(1)` clone in C++23.

The focus is narrow:
- POSIX compatibility
- GNU extensions that matter
- measurable performance
- tracked benchmark outputs
- grouped test coverage that keeps regressions visible

Recent work reduced hot-path overhead, refreshed the benchmark results in the repo, and added 200 generated integration tests so changes are visible instead of anecdotal.

The goal is simple: keep it correct, keep it practical, keep it fast.
