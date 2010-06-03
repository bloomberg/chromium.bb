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

rm -f  *.out

LIST="input.source input.log input.graphic input.random input.program"

for i in  ${LIST} ; do
  ${PREFIX} $1 ${DASHDASH} data/ref/input/$i 60 > $i.out  2>stderr.out
done

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  for i in  ${LIST} ; do
    cmp $i.out  data/ref/output/$i.out
  done
fi
echo "OK"
