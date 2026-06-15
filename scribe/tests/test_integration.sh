#!/bin/sh

set -eu

[ -f ./CMakeLists.txt ] || {
	printf 'FAIL: run this script from the project root\n' >&2
	exit 1
}

SCRIBE_BIN="${SCRIBE_BIN:-./build/scribe}"
TEST_TMP="./tests/tmp"
INIT_ROOT="$TEST_TMP/init-root"

fail() {
	printf 'FAIL: %s\n' "$1" >&2
	exit 1
}

pass() {
	printf 'PASS: %s\n' "$1"
}

assert_fails() {
	if "$@" >"$TEST_TMP/stdout.txt" 2>"$TEST_TMP/stderr.txt"; then
		fail "expected command to fail: $*"
	fi
}

assert_dir() {
	[ -d "$1" ] || fail "expected directory: $1"
}

assert_symlink() {
	[ -L "$1" ] || fail "expected symlink: $1"
}

assert_link_target() {
	actual_target="$(readlink "$1")"
	[ "$actual_target" = "$2" ] || fail "expected $1 -> $2, got $actual_target"
}

assert_init_layout() {
	assert_dir "$INIT_ROOT"
	assert_dir "$INIT_ROOT/store"
	assert_dir "$INIT_ROOT/var"
	assert_dir "$INIT_ROOT/var/profiles"
	assert_dir "$INIT_ROOT/var/profiles/system"
	assert_dir "$INIT_ROOT/var/profiles/system/generations"
	assert_dir "$INIT_ROOT/var/profiles/system/generations/0"
	assert_dir "$INIT_ROOT/var/gcroots"
	assert_dir "$INIT_ROOT/var/sources"
	assert_dir "$INIT_ROOT/var/builds"
	assert_dir "$INIT_ROOT/var/logs"
	assert_dir "$INIT_ROOT/var/tmp"

	assert_symlink "$INIT_ROOT/var/profiles/system/current"
	assert_link_target "$INIT_ROOT/var/profiles/system/current" "generations/0"
}

test_cli_failures() {
	assert_fails "$SCRIBE_BIN"
	assert_fails "$SCRIBE_BIN" unknown-command
	assert_fails "$SCRIBE_BIN" --root
	assert_fails "$SCRIBE_BIN" --bad-option init
	assert_fails "$SCRIBE_BIN" init extra-arg

	pass "integration: cli rejects invalid invocations"
}

test_init_layout() {
	rm -rf "$TEST_TMP"
	mkdir -p "$TEST_TMP"

	"$SCRIBE_BIN" --root "$INIT_ROOT" init
	assert_init_layout

	"$SCRIBE_BIN" --root "$INIT_ROOT" init
	assert_init_layout

	assert_fails "$SCRIBE_BIN" --root "$TEST_TMP/missing-parent/init-root" init

	pass "integration: init creates root layout and is idempotent"
}

rm -rf "$TEST_TMP"
mkdir -p "$TEST_TMP"

test_cli_failures
test_init_layout
