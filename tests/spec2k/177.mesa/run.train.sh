#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}


DASHDASH=""
if [[ "${PREFIX}" =~ sel_ldr ]] ; then
  DASHDASH="--"
fi

rm -f  mesa.log mesa.ppm mesa.in numbers

ln -s  data/train/input/* .


# TODO(robertm): remove -c option
# c.f.: http://code.google.com/p/nativeclient/issues/detail?id=717
${PREFIX} $1 -c ${DASHDASH} -frames 500 -meshfile mesa.in -ppmfile mesa.ppm >mesa.out 2>mesa.err

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  mesa.log  data/train/output/mesa.log
  ../specdiff.sh -a 6 -l 10 mesa.ppm data/train/output/mesa.ppm
fi


echo "OK"
