#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}


rm -f  *.out *endian* persons.*
ln -s  data/train/input/* .

if [[ "${EMU_HACK}" != "no" ]] ; then
  touch  vortex.msg
  touch  vortex.out
fi

${PREFIX} $1 ${DASHDASH} lendian.raw     > stdout.out 2> stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  vortex.out   data/train/output/vortex.out
fi

echo "OK"
