#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}

rm -f  *.out

${PREFIX} $1 ${DASHDASH} -l data/all/input/ -q -m 192M \
    < data/ref/input/ref.in  > stdout.out 2> stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  stdout.out  data/ref/output/ref.out
fi

echo "OK"
