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

rm -f  *.out words 2.1.dict
ln -s  data/all/input/words .
ln -s  data/all/input/2.1.dict .

${PREFIX} $1 ${DASHDASH} 2.1.dict -batch < data/ref/input/ref.in  > stdout.out 2> stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  stdout.out  data/ref/output/ref.out
fi
echo "OK"
