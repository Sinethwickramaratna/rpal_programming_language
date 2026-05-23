#!/bin/bash

PASS=0
FAIL=0
BINARY="./rpal/rpal20"
TESTS_DIR="./tests"

if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found"
    exit 1
fi

for input in "$TESTS_DIR"/*_input.txt "$TESTS_DIR"/*_input; do
    [ -e "$input" ] || continue

    name="${input/_input/_expected}"
    if [ ! -f "$name" ]; then
        base="${input%.*}"
        name="${base/_input/_expected}"
    fi

    if [ ! -f "$name" ]; then
        echo "SKIP: $input (no expected file found)"
        continue
    fi

    # Capture program output and compare using temporary files. Normalize
    # expected contents to LF-only and strip trailing whitespace so tests are
    # robust to CRLF differences and insignificant whitespace variations.
    actual=$($BINARY "$input" 2>&1)
    actfile=$(mktemp)
    expfile=$(mktemp)
    printf "%s" "$actual" > "$actfile"

    # Normalize expected file: remove CR (\r) and strip trailing spaces per line
    tr -d '\r' < "$name" | sed 's/[ \t]*$//' > "$expfile"

    diffOut=$(diff -u "$expfile" "$actfile" 2>&1) || true
    if [ -z "$diffOut" ]; then
        echo "PASS: $input"
        ((PASS++))
    else
        echo "FAIL: $input"
        echo "--- diff (expected -> actual) ---"
        echo "$diffOut"
        echo "--- end diff ---"
        ((FAIL++))
    fi
    rm -f "$actfile" "$expfile"
done

echo ""
echo "Results: $PASS passed, $FAIL failed"