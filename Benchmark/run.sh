#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

RUNS=7
WARMUP=2
DO_BUILD=1
FASTSED_BIN="${FASTSED_BIN:-./Bin/fastsed}"
SED_BIN="${SED_BIN:-/usr/bin/sed}"
RESULTS_DIR="${RESULTS_DIR:-Benchmark/Results}"
CSV_OUT="${CSV_OUT:-${RESULTS_DIR}/latest_results.csv}"
SVG_OUT="${SVG_OUT:-${RESULTS_DIR}/latest_results.svg}"
PNG_OUT="${PNG_OUT:-${RESULTS_DIR}/latest_results.png}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --runs) RUNS="$2"; shift 2 ;;
        --warmup) WARMUP="$2"; shift 2 ;;
        --no-build) DO_BUILD=0; shift ;;
        --csv-out) CSV_OUT="$2"; shift 2 ;;
        --svg-out) SVG_OUT="$2"; shift 2 ;;
        --png-out) PNG_OUT="$2"; shift 2 ;;
        *)
            echo "[bench] unknown argument: $1"
            echo "usage: ./Benchmark/run.sh [--runs N] [--warmup N] [--no-build] [--csv-out path] [--svg-out path] [--png-out path]"
            exit 1
            ;;
    esac
done

if [[ "$DO_BUILD" -eq 1 ]]; then
    ./b Release
fi

if [[ ! -x "$FASTSED_BIN" ]]; then
    echo "[bench] missing fastsed binary: $FASTSED_BIN"
    exit 1
fi

if [[ ! -x "$SED_BIN" ]]; then
    echo "[bench] missing sed binary: $SED_BIN"
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "[bench] python3 is required"
    exit 1
fi

mkdir -p "$RESULTS_DIR"
mkdir -p "$(dirname "$CSV_OUT")"
mkdir -p "$(dirname "$SVG_OUT")"
mkdir -p "$(dirname "$PNG_OUT")"

printf 'name,fastsed_avg,fastsed_best,sed_avg,sed_best,ratio\n' > "$CSV_OUT"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

LOG_INPUT="$TMP_DIR/log.txt"
CONFIG_INPUT="$TMP_DIR/config.txt"
REPEAT_INPUT="$TMP_DIR/repeat.txt"
TEXT_INPUT="$TMP_DIR/text.txt"
CSV_INPUT="$TMP_DIR/records.csv"
PATH_INPUT="$TMP_DIR/paths.txt"
NUM_INPUT="$TMP_DIR/numbers.txt"
MAIL_INPUT="$TMP_DIR/mail.txt"

gen_log() {
    awk '
        BEGIN {
            for (i = 1; i <= 250000; ++i) {
                level = (i % 19 == 0) ? "error" : ((i % 11 == 0) ? "warn" : "info");
                kind  = (i % 7 == 0) ? "acct" : "user";
                code  = (i % 29 == 0) ? 500 : ((i % 13 == 0) ? 404 : 200);
                printf "%06d level=%s actor=%s-%d region=ap-southeast-%d status=%d latency=%dms payload=%d\n",
                       i, level, kind, i * 17, i % 3, code, (i * 37) % 900, i * 97;
            }
        }
    ' > "$LOG_INPUT"
}

gen_config() {
    awk '
        BEGIN {
            for (i = 1; i <= 180000; ++i) {
                if (i % 5 == 0) {
                    printf "# comment %06d keep parser busy\n", i;
                } else {
                    printf "key_%06d=value_%06d flag=%d bucket=%d\n", i, i * 3, i % 2, i % 17;
                }
            }
        }
    ' > "$CONFIG_INPUT"
}

gen_repeat() {
    awk '
        BEGIN {
            for (i = 1; i <= 220000; ++i)
                print "one two two two three two two tail=" i;
        }
    ' > "$REPEAT_INPUT"
}

gen_text() {
    awk '
        BEGIN {
            for (i = 1; i <= 160000; ++i) {
                if (i % 9 == 0) {
                    print "";
                } else {
                    printf "section_%06d    alpha   beta    gamma   delta_%d   \n", i, i % 23;
                }
            }
        }
    ' > "$TEXT_INPUT"
}

gen_csv() {
    awk -F, '
        BEGIN {
            for (i = 1; i <= 180000; ++i) {
                printf "acct-%06d,team-%02d,region-%d,owner-%06d\n", i, i % 40, i % 6, i * 11;
            }
        }
    ' > "$CSV_INPUT"
}

gen_paths() {
    awk '
        BEGIN {
            for (i = 1; i <= 180000; ++i) {
                printf "/srv/app/service_%02d/node_%06d/config.yaml\n", i % 25, i;
            }
        }
    ' > "$PATH_INPUT"
}

gen_numbers() {
    awk '
        BEGIN {
            for (i = 1; i <= 250000; ++i) print i;
        }
    ' > "$NUM_INPUT"
}

gen_mail() {
    awk '
        BEGIN {
            for (i = 1; i <= 180000; ++i) {
                printf "user%06d@example%d.internal role=team-%02d backup=ops%06d@example%d.internal\n",
                       i, i % 19, i % 31, i * 3, i % 13;
            }
        }
    ' > "$MAIL_INPUT"
}

gen_log
gen_config
gen_repeat
gen_text
gen_csv
gen_paths
gen_numbers
gen_mail

time_once() {
    python3 Benchmark/time_command.py "$@"
}

validate_sample() {
    local sample="$1"
    if [[ ! "$sample" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
        echo "[bench] invalid timing sample: $sample"
        exit 1
    fi
}

summarize_values() {
    printf '%s\n' "$@" | awk '
        BEGIN { min = -1; sum = 0; n = 0; }
        NF == 0 { next; }
        {
            v = $1 + 0.0;
            if (min < 0 || v < min) min = v;
            sum += v;
            n += 1;
        }
        END {
            if (n == 0) exit 1;
            printf "%.6f %.6f", sum / n, min;
        }
    '
}

benchmark_case() {
    local name="$1"
    local input="$2"
    local flags="$3"
    local script="$4"

    local fastsed_out="$TMP_DIR/${name}.fastsed.out"
    local sed_out="$TMP_DIR/${name}.sed.out"
    local -a flag_words=()
    local -a fastsed_samples=()
    local -a sed_samples=()
    if [[ -n "$flags" ]]; then
        read -r -a flag_words <<< "$flags"
    fi

    "$FASTSED_BIN" "${flag_words[@]}" -f "$script" "$input" > "$fastsed_out"
    "$SED_BIN" "${flag_words[@]}" -f "$script" "$input" > "$sed_out"

    if ! cmp -s "$fastsed_out" "$sed_out"; then
        echo "[bench] output mismatch for $name"
        diff -u "$sed_out" "$fastsed_out" | sed -n '1,80p' || true
        exit 1
    fi

    echo "[bench] $name"

    for ((i = 0; i < WARMUP; ++i)); do
        time_once "$FASTSED_BIN" "${flag_words[@]}" -f "$script" "$input" >/dev/null
        time_once "$SED_BIN" "${flag_words[@]}" -f "$script" "$input" >/dev/null
    done

    for ((i = 0; i < RUNS; ++i)); do
        fastsed_sample="$(time_once "$FASTSED_BIN" "${flag_words[@]}" -f "$script" "$input")"
        sed_sample="$(time_once "$SED_BIN" "${flag_words[@]}" -f "$script" "$input")"
        validate_sample "$fastsed_sample"
        validate_sample "$sed_sample"
        fastsed_samples+=("$fastsed_sample")
        sed_samples+=("$sed_sample")
    done

    read -r fastsed_avg fastsed_best <<< "$(summarize_values "${fastsed_samples[@]}")"
    read -r sed_avg sed_best <<< "$(summarize_values "${sed_samples[@]}")"

    ratio="$(awk -v fastsed_avg="$fastsed_avg" -v sed_avg="$sed_avg" 'BEGIN { if (sed_avg > 0.0) printf "%.6f", fastsed_avg / sed_avg; else printf "0.000000"; }')"
    printf '%s,%s,%s,%s,%s,%s\n' \
        "$name" "$fastsed_avg" "$fastsed_best" "$sed_avg" "$sed_best" "$ratio" >> "$CSV_OUT"

    awk -v name="$name" \
        -v fastsed_avg="$fastsed_avg" -v fastsed_best="$fastsed_best" \
        -v sed_avg="$sed_avg" -v sed_best="$sed_best" -v ratio="$ratio" '
        BEGIN {
            printf "%-20s  fastsed avg=%8.4fs best=%8.4fs  sed avg=%8.4fs best=%8.4fs  ratio=%6.3fx\n",
                   name, fastsed_avg, fastsed_best, sed_avg, sed_best, ratio;
        }
    '
}

echo "[bench] fastsed: $FASTSED_BIN"
echo "[bench] sed:     $SED_BIN"
echo "[bench] runs=$RUNS warmup=$WARMUP"
echo ""

benchmark_case "global-substitute"   "$LOG_INPUT"    ""   "Benchmark/Scripts/global-substitute.sed"
benchmark_case "extended-capture"    "$LOG_INPUT"    "-E" "Benchmark/Scripts/extended-capture.sed"
benchmark_case "address-delete"      "$LOG_INPUT"    ""   "Benchmark/Scripts/address-delete.sed"
benchmark_case "delete-comments"     "$CONFIG_INPUT" ""   "Benchmark/Scripts/delete-comments.sed"
benchmark_case "transliterate-upper" "$LOG_INPUT"    ""   "Benchmark/Scripts/transliterate-upper.sed"
benchmark_case "nth-global"          "$REPEAT_INPUT" ""   "Benchmark/Scripts/nth-global.sed"
benchmark_case "compress-spaces"     "$TEXT_INPUT"   ""   "Benchmark/Scripts/compress-spaces.sed"
benchmark_case "strip-trailing"      "$TEXT_INPUT"   ""   "Benchmark/Scripts/strip-trailing.sed"
benchmark_case "blank-delete"        "$TEXT_INPUT"   ""   "Benchmark/Scripts/blank-delete.sed"
benchmark_case "csv-reorder"         "$CSV_INPUT"    "-E" "Benchmark/Scripts/csv-reorder.sed"
benchmark_case "path-rewrite"        "$PATH_INPUT"   ""   "Benchmark/Scripts/path-rewrite.sed"
benchmark_case "step-delete"         "$NUM_INPUT"    ""   "Benchmark/Scripts/step-delete.sed"
benchmark_case "email-redact"        "$MAIL_INPUT"   "-E" "Benchmark/Scripts/email-redact.sed"
benchmark_case "error-prefix"        "$LOG_INPUT"    ""   "Benchmark/Scripts/error-prefix.sed"
benchmark_case "status-rewrite"      "$LOG_INPUT"    "-E" "Benchmark/Scripts/status-rewrite.sed"

python3 Benchmark/render_chart.py "$CSV_OUT" "$SVG_OUT"
convert "$SVG_OUT" "$PNG_OUT"

echo ""
echo "[bench] wrote CSV: $CSV_OUT"
echo "[bench] wrote SVG: $SVG_OUT"
echo "[bench] wrote PNG: $PNG_OUT"
