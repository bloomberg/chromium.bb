#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${PREFIX:-yes}
EMU_HACK=${EMU_HACK:-yes}

DASHDASH=""
if [[ "${PREFIX}" =~ sel_ldr ]] ; then
  DASHDASH="--"
fi

rm -f *.out

if [[ "${EMU_HACK}" != "no" ]] ; then
  touch mcf.out
fi

${PREFIX} $1  ${DASHDASH} data/ref/input/inp.in >stdout.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp stdout.out  data/ref/output/inp.out
  cmp mcf.out data/ref/output/mcf.out
fi

echo "OK"
