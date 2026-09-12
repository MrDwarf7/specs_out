#!/usr/bin/env bash
# Hyperfine matrix: cold/hot x preview/write/help/serial/test suites.
# Invokes Makefile bench-* recipes only (no raw gcc). 100 runs per cell.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

REPORT_DIR="${REPORT_DIR:-$ROOT/docs/performance_reports}"
RAW_DIR="$REPORT_DIR/raw"
SCRATCH_DIR="$REPORT_DIR/scratch"
RUNS="${RUNS:-100}"
WARMUP_HOT="${WARMUP_HOT:-20}"
MAKE="${MAKE:-make}"

mkdir -p "$RAW_DIR" "$SCRATCH_DIR"

if ! command -v hyperfine >/dev/null 2>&1; then
  printf 'hyperfine not found in PATH\n' >&2
  exit 127
fi

COLD_PREPARE='sync'
if [ -w /proc/sys/vm/drop_caches ]; then
  COLD_PREPARE='sync; printf 3 > /proc/sys/vm/drop_caches'
fi

MAKE_CMD=("$MAKE" -s --no-print-directory)

run_cell() {
  local name="$1"
  local kind="$2"
  local recipe="$3"
  local extra_env="${4:-}"

  local json="$RAW_DIR/${name}.json"
  local md="$RAW_DIR/${name}.md"
  local log="$RAW_DIR/${name}.log"
  local csv="$RAW_DIR/${name}.csv"
  local dest="$SCRATCH_DIR/${name}.specs"

  local cmd="${MAKE} -s --no-print-directory ${recipe}"
  case "$recipe" in
    bench-debug-dry | bench-debug-write | bench-rel-dry | bench-rel-write)
      cmd="${cmd} OUT=${dest}"
      ;;
  esac
  if [ -n "$extra_env" ]; then
    cmd="env ${extra_env} ${cmd}"
  fi

  local -a hf=(
    hyperfine
    --shell=bash
    --runs "$RUNS"
    --command-name "$name"
    --export-json "$json"
    --export-markdown "$md"
    --export-csv "$csv"
  )

  if [ "$kind" = hot ]; then
    hf+=(--warmup "$WARMUP_HOT")
  else
    hf+=(--warmup 0 --prepare "$COLD_PREPARE")
  fi

  printf '\n==> %s (%s, %s runs)\n' "$name" "$kind" "$RUNS"
  "${hf[@]}" "$cmd" >"$log" 2>&1 || {
    printf 'FAILED %s\n' "$name" >&2
    tail -20 "$log" >&2
    return 1
  }
}

# Build once through Makefile so every cell measures a make-built artifact.
"${MAKE_CMD[@]}" b
"${MAKE_CMD[@]}" br
"${MAKE_CMD[@]}" t

failed=0

# Debug binary
run_cell debug_help_cold cold bench-debug-help || failed=1
run_cell debug_help_hot hot bench-debug-help || failed=1
run_cell debug_preview_cold cold bench-debug-dry || failed=1
run_cell debug_preview_hot hot bench-debug-dry || failed=1
run_cell debug_preview_serial_cold cold bench-debug-dry SPECS_OUT_JOBS=1 || failed=1
run_cell debug_preview_serial_hot hot bench-debug-dry SPECS_OUT_JOBS=1 || failed=1
run_cell debug_write_cold cold bench-debug-write || failed=1
run_cell debug_write_hot hot bench-debug-write || failed=1

# Release binary
run_cell release_help_cold cold bench-rel-help || failed=1
run_cell release_help_hot hot bench-rel-help || failed=1
run_cell release_preview_cold cold bench-rel-dry || failed=1
run_cell release_preview_hot hot bench-rel-dry || failed=1
run_cell release_preview_serial_cold cold bench-rel-dry SPECS_OUT_JOBS=1 || failed=1
run_cell release_preview_serial_hot hot bench-rel-dry SPECS_OUT_JOBS=1 || failed=1
run_cell release_write_cold cold bench-rel-write || failed=1
run_cell release_write_hot hot bench-rel-write || failed=1

# Test runner (debug objects)
run_cell tests_all_cold cold bench-test || failed=1
run_cell tests_all_hot hot bench-test || failed=1
run_cell tests_parse_cold cold bench-test-parse || failed=1
run_cell tests_parse_hot hot bench-test-parse || failed=1
run_cell tests_format_cold cold bench-test-format || failed=1
run_cell tests_format_hot hot bench-test-format || failed=1
run_cell tests_cli_cold cold bench-test-cli || failed=1
run_cell tests_cli_hot hot bench-test-cli || failed=1
run_cell tests_capture_cold cold bench-test-capture || failed=1
run_cell tests_capture_hot hot bench-test-capture || failed=1

# Combined table from every JSON.
{
  printf '| cell | mean (s) | stddev (s) | min (s) | max (s) |\n'
  printf '| --- | ---: | ---: | ---: | ---: |\n'
  for json in "$RAW_DIR"/*.json; do
    [ -f "$json" ] || continue
    python3 - "$json" <<'PY'
import json, sys
path = sys.argv[1]
with open(path, encoding="utf-8") as fh:
    data = json.load(fh)
for row in data.get("results", []):
    name = row.get("command") or path
    print(
        f"| {name} | {row['mean']:.6f} | {row['stddev']:.6f} | {row['min']:.6f} | {row['max']:.6f} |"
    )
PY
  done
} >"$REPORT_DIR/summary_table.md"

if [ "$failed" -ne 0 ]; then
  printf 'one or more hyperfine cells failed; see %s\n' "$RAW_DIR" >&2
  exit 1
fi

printf 'wrote reports under %s\n' "$REPORT_DIR"
