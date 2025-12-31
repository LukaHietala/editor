#!/bin/bash

if ! command -v clang-format &> /dev/null; then
    echo "Install clang-format to use this"
    exit 1
fi

find src -name "*.c" -o -name "*.h" | xargs clang-format -i