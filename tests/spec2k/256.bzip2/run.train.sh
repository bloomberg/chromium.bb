#!/bin/bash

set -o nounset
set -o errexit

VERIFY=${PREFIX:-yes}
PREFIX=${PREFIX:-}

DASHDASH=""
if [[ "${PREFIX}" =~ "sel_ldr" ]] ; then
  DASHDASH="--"
fi

rm -f  *.out
${PREFIX} $1 ${DASHDASH} data/train/input/input.compressed 8 > input.compressed.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  input.compressed.out  data/train/output/input.compressed.out
fi
echo OK
