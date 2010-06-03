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

LIST="166.i 200.i expr.i integrate.i  scilab.i"

rm -f  *.out


for i in ${LIST} ; do
  out=${i%.*}.s
  if [[ "${EMU_HACK}" != "no" ]] ; then
    touch ${out}
  fi


  ${PREFIX} $1 ${DASHDASH} data/ref/input/$i -o ${out} \
    > $i.stdout.out 2> $i.stderr.out
done

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  for i in ${LIST} ; do
    out=${i%.*}.s
    cmp  ${out}  data/ref/output/${out}
  done
fi
echo "OK"
