#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}


rm -f *.out all.new.ammp ammp.in new.tether ammp.out

ln -s  data/train/input/* .

${PREFIX} $1 ${DASHDASH} <ammp.in  >ammp.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  ../specdiff.sh -r 0.003 -a 0.0001 -l 10 ammp.out data/train/output/ammp.out
fi

echo "OK"
