#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMPDIR_BASE=${TMPDIR:-/tmp}
BIN="$TMPDIR_BASE/hms-esp-apc-hid-pdc-test-$$"
PARSER_BIN="$TMPDIR_BASE/hms-esp-apc-report-test-$$"
APC_BIN="$TMPDIR_BASE/hms-esp-apc-srt2200xli-test-$$"
trap 'rm -f "$BIN" "$PARSER_BIN" "$APC_BIN"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" -o "$BIN" "$ROOT/test/hid_pdc_test.c" "$ROOT/main/hid_pdc.c" "$ROOT/main/ups_map.c"
"$BIN"
cc -std=gnu11 -Wall -Wextra -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-format -include "$ROOT/test/host_stubs/compat.h" -I"$ROOT/test/host_stubs" -I"$ROOT/main" -o "$PARSER_BIN" "$ROOT/test/apc_report_decode_test.c" "$ROOT/main/apc_hid_parser.c"
"$PARSER_BIN"

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" -o "$APC_BIN" "$ROOT/test/apc_srt2200xli_descriptor_test.c" "$ROOT/main/hid_pdc.c" "$ROOT/main/ups_map.c"
"$APC_BIN"
