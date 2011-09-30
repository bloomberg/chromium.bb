#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}


rm -f *.out inp.in inp.out

ln -s  data/ref/input/* .

${PREFIX} $1 ${DASHDASH}  <inp.in  >inp.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  ../specdiff.sh -r 0.00001 -l 10 inp.out data/ref/output/inp.out
fi

echo "OK"
