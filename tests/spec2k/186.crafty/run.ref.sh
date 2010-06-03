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

if [[ "${EMU_HACK}" != "no" ]] ; then
  touch game.001
fi

rm -f *.out

${PREFIX} $1 <data/ref/input/crafty.in >stdout.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp stdout.out data/ref/output/crafty.out
fi


echo "OK"
