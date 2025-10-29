#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

cd "$ROOT"
make >/dev/null

pass=0
fail=0

run_case() {
  name="$1"; shift
  expected="$1"; shift
  cmd=( "$@" )

  out="$(mktemp)"; out_norm="$(mktemp)"
  trap 'rm -f "$out" "$out_norm"' EXIT

  # Run the command (exit code can be non-zero for misspellings; that's fine)
  "${cmd[@]}" >"$out" 2>/dev/null || true

  # Normalize: strip absolute ROOT prefix and sort lines for order-insensitive compare
  sed "s#${ROOT}/##g" "$out" | sort > "$out_norm"

  # Also compare against a sorted copy of expected (in case you reorder lines there)
  if diff -u <(sort "$expected") "$out_norm"; then
    echo "Pass: $name"
    pass=$((pass+1))
  else
    echo "Fail: $name"
    echo "Command: ${cmd[*]}"
    echo "Got:"
    cat "$out_norm"
    echo "Expected:"
    sort "$expected"
    fail=$((fail+1))
  fi

  rm -f "$out" "$out_norm"
}


DICTS="$ROOT/tests/dicts"
INPUTS="$ROOT/tests/inputs"
EXP="$ROOT/tests/expected"

run_case "good (single file, expect empty)" "$EXP/good.out" \
  "$ROOT/spell" "$DICTS/small.txt" "$INPUTS/good.txt"

run_case "bad (single file, no filename in output)" "$EXP/bad_single.out" \
  "$ROOT/spell" "$DICTS/small.txt" "$INPUTS/bad.txt"

run_case "caps (single file)" "$EXP/caps_single.out" \
  "$ROOT/spell" "$DICTS/small.txt" "$INPUTS/caps.txt"

run_case "digits/symbols ignored" "$EXP/digits_symbols.out" \
  "$ROOT/spell" "$DICTS/small.txt" "$INPUTS/digits_symbols.txt"

run_case "directory recursion default suffix .txt" "$EXP/tree_default_suffix.out" \
  "$ROOT/spell" "$DICTS/small.txt" "$ROOT/tests/tree"

echo
echo "Passed: $pass  Failed: $fail"
if [ "$fail" -eq 0 ]; then
  exit 0
else
  exit 1
fi
