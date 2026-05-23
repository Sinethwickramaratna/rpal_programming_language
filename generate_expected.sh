#!/bin/bash

BINARY="./rpal/rpal20"
TESTS_DIR="./tests"

if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found"
    exit 1
fi

for input in "$TESTS_DIR"/*_input "$TESTS_DIR"/*_input.txt; do
    [ -e "$input" ] || continue

    name="${input/_input/_expected}"
    if [ ! -f "$name" ]; then
        base="${input%.*}"
        name="${base/_input/_expected}"
    fi

    echo "Generating: $name from $input"
    "$BINARY" "$input" > "$name"
    # Normalize to LF and strip trailing whitespace
    sed -i 's/\r$//' "$name"
    sed -i 's/[ \t]*$//' "$name"
done

echo "Done."