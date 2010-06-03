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

${PREFIX} $1 <data/test/input/crafty.in >stdout.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp stdout.out data/test/output/crafty.out
fi


echo "OK"
