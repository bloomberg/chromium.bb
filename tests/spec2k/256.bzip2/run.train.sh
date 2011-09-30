#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}


rm -f  *.out
${PREFIX} $1 ${DASHDASH} data/train/input/input.compressed 8 > input.compressed.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  input.compressed.out  data/train/output/input.compressed.out
fi
echo OK
