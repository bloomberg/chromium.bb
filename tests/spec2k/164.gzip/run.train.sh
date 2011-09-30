#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}

rm -f  *.out

${PREFIX} $1 ${DASHDASH} data/train/input/input.combined 32 >stdout.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp stdout.out  data/train/output/input.combined.out
fi
echo "OK"
