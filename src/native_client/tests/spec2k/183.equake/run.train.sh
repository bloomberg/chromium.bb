#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}


python ../prepare_input.py --config $(basename $(pwd)) train

${PREFIX} $1 ${DASHDASH}  <inp.in  >inp.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  ../specdiff.sh -r 0.00001 -l 10 inp.out data/train/output/inp.out
fi

echo "OK"
