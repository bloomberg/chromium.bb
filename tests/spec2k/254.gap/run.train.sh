#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}

if [[ $1 =~ .*pnacl.* ]]; the
  echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
  echo "@@@BUILD_STEP skip 254.gap due to BUG2036 in PNaCl@@@"
  echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
  echo OK
  exit 0
fi

DASHDASH=
if [[ "${PREFIX}" =~ sel_ldr ]] ; then
 DASHDASH="--"
fi

rm -f  *.out

${PREFIX} $1 ${DASHDASH} -l data/all/input/ -q -m 128M \
    < data/train/input/train.in  > stdout.out 2> stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  stdout.out  data/train/output/train.out
fi

echo "OK"
