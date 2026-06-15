#!/bin/sh

set -eu

[ -f ./CMakeLists.txt ] || {
	printf 'FAIL: run this script from the project root\n' >&2
	exit 1
}

cmake --build build
ctest --test-dir build --output-on-failure

SCRIBE_BIN="./build/scribe" ./tests/test_integration.sh
