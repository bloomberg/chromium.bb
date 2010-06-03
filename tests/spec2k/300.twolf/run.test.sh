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

rm -f *.out
cp  data/test/input/* .


LIST="test.out test.twf test.pl1 test.pl2 test.sav test.pin test.sv2"

if [[ "${EMU_HACK}" != "no" ]] ; then
  touch  ${LIST}
  touch  test.tmp
  touch  test.cel
fi

${PREFIX} $1 ${DASHDASH} test >stdout.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  #cmp stdout.out  data/test/output/test.stdout
  echo "SKIP STDOUT"
  for i in ${LIST}; do 
    cmp $i data/test/output/$i
  done
fi
echo "OK"

