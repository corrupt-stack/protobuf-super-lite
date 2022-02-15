#!/bin/bash

# Make sure we're in the project root directory.
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

# Format all C/C++ code except third_party code.
CLANG_FORMAT_EXECUTABLE=$(which clang-format)
if [ -x "$CLANG_FORMAT_EXECUTABLE" ]; then
    find ./ -iname '*.c' -o -iname '*.cc' -o -iname '*.h' |
        grep -vP 'third_party/|out/' |
        xargs $CLANG_FORMAT_EXECUTABLE -i --style=file --verbose
else
    echo
    echo "********************************************************************"
    echo "Don't forget to run clang-format if you changed any code:"
    echo "  clang-format -i --style=file ...changed files..."
    echo "NOTE: This script would have run it for you if it were in your PATH."
    echo "********************************************************************"
    echo
fi

GN_EXECUTABLE=$(which gn)
if [ -x "$GN_EXECUTABLE" ]; then
    find ./ -iname 'BUILD.gn' -o -iname '*.gni' |
        grep -vP '^third_party/|out/' |
        xargs $GN_EXECUTABLE format ./third_party/BUILD.gn
else
    echo
    echo "********************************************************************"
    echo "Don't forget to run gn format if you changed any BUILD.gn files:"
    echo "  gn format ...changed files..."
    echo "NOTE: This script would have run it for you if it were in your PATH."
    echo "********************************************************************"
    echo
fi
