#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}

rm -f *.out
rm -f ref.* train.* test.*
cp  data/ref/input/* .


LIST="ref.out ref.twf ref.pl1 ref.pl2 ref.pin"

if [[ "${EMU_HACK}" != "no" ]] ; then
  touch  ${LIST}
  touch  ref.tmp
  touch  ref.sav
  touch  ref.sv2
fi

${PREFIX} $1 ${DASHDASH} ref >stdout.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  ../specdiff.sh -o stdout.out data/ref/output/ref.stdout
  for i in ${LIST}; do
    ../specdiff.sh -o $i data/ref/output/$i
  done
fi
echo "OK"

