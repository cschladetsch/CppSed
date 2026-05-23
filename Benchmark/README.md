# Benchmarks

This directory contains a small benchmark harness for comparing `fastsed`
against the system `sed`.

## What it does

- Generates deterministic input files in a temporary directory
- Verifies that `./Bin/fastsed` and `/usr/bin/sed` produce identical output
- Runs warmup and measured iterations with a Python `perf_counter()` timer
- Prints a compact summary with average and best wall-clock time per case
- Writes CSV, SVG, and PNG comparison artefacts under `Benchmark/Results/`

## Usage

```bash
./Benchmark/run.sh
./Benchmark/run.sh --runs 10 --warmup 3
./Benchmark/run.sh --no-build
./Benchmark/run.sh --csv-out /tmp/results.csv --png-out /tmp/results.png
```

## Benchmark cases

- `global-substitute`: global substitution over a log-like file
- `extended-capture`: extended regex capture and replacement
- `address-delete`: address-based deletion over a log-like file
- `delete-comments`: delete comment lines from a mixed config file
- `transliterate-upper`: transliterate ASCII letters to uppercase
- `nth-global`: replace every match from the second onward on a line
- `compress-spaces`: collapse runs of whitespace
- `strip-trailing`: remove trailing whitespace
- `blank-delete`: drop blank lines from paragraph-like text
- `csv-reorder`: reorder comma-separated columns with capture groups
- `path-rewrite`: rewrite a path prefix with an alternate delimiter
- `step-delete`: GNU step-address deletion on numeric input
- `email-redact`: redact email-like tokens in free-form text
- `error-prefix`: prefix matching log lines
- `status-rewrite`: capture and rewrite status fields

## Notes

- The harness currently targets GNU `sed` semantics because this repo is
  already tested primarily on Linux.
- Timings are wall-clock seconds from Python `time.perf_counter()`.
- Input data is generated at runtime and removed automatically.
