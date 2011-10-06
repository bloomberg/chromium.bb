#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}




if [[ "${DASHDASH}" =~ "--" ]] ; then
  if [[ $1 =~ pnacl.*x8632 ]] ; then
    # TODO(robertm): remove -c option
    # c.f.: http://code.google.com/p/nativeclient/issues/detail?id=893
    DASHDASH="-c --"
  fi
fi

rm -f  *.out *.s

if [[ "${EMU_HACK}" != "no" ]] ; then
  touch cp-decl.s
fi

${PREFIX} $1 ${DASHDASH} data/train/input/cp-decl.i -o cp-decl.s \
  > stdout.out 2> stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  cp-decl.s  data/train/output/cp-decl.s
fi
echo "OK"
