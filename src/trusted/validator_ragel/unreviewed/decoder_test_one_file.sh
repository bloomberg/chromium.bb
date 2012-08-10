#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Checks that a given assembyl file is decoded identically to objdump.
#
# Usage:
#   decoder_test_one_file.sh GAS=... OBJDUMP=... DECODER=... ASMFILE=...

# TODO(shcherbina): fix leaking temp files on failed tests

set -e
set -u

# Parse arguments.
eval "$@"

# Sanity check arguments.
if [[ ! -x "${GAS% *}" || ! -x "${OBJDUMP% *}" || ! -x "${DECODER% *}" ]] ; then
  echo >&2 "error: GAS or OBJDUMP or DECODER incorrect"
  exit 2
fi

if [[ ! -f "$ASMFILE" ]] ; then
  echo >&2 "error: ASMFILE is not a regular file: $ASMFILE"
  exit 3
fi

# Produce an object file, disassemble it in 2 ways and compare results.
$GAS "$ASMFILE" -o "$ASMFILE.o"
rm -f "$ASMFILE"
$DECODER "$ASMFILE.o" > "$ASMFILE.decoder"
# Take objdump output starting at line 8 to skip the unimportant header that
# is not emulated in the decoder test.
$OBJDUMP -d "$ASMFILE.o" |
  tail -n+8 - |
  cmp - "$ASMFILE.decoder"
rm -f "$ASMFILE.o" "$ASMFILE.decoder"
