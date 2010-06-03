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

rm -f  *.out *endian* persons.*
ln -s  data/ref/input/* .


LIST="1 2 3"
if [[ "${EMU_HACK}" != "no" ]] ; then
  touch  vortex.msg
  touch  vortex1.out vortex2.out vortex3.out
fi


for i in ${LIST} ; do
  ${PREFIX} $1 ${DASHDASH} lendian$i.raw     > $i.stdout.out 2> $i.stderr.out
done

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  for i in ${LIST} ; do
    cmp  vortex$i.out   data/ref/output/vortex$i.out
  done
fi

echo "OK"
