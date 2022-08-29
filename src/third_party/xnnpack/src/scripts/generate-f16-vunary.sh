#!/bin/sh
# Copyright 2022 Google LLC
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

################################### ARM NEON ##################################
tools/xngen src/f16-vunary/neonfp16arith.c.in -D OP=ABS -D BATCH_TILE=8  -o src/f16-vunary/gen/vabs-neonfp16arith-x8.c &
tools/xngen src/f16-vunary/neonfp16arith.c.in -D OP=ABS -D BATCH_TILE=16 -o src/f16-vunary/gen/vabs-neonfp16arith-x16.c &
tools/xngen src/f16-vunary/neonfp16arith.c.in -D OP=NEG -D BATCH_TILE=8  -o src/f16-vunary/gen/vneg-neonfp16arith-x8.c &
tools/xngen src/f16-vunary/neonfp16arith.c.in -D OP=NEG -D BATCH_TILE=16 -o src/f16-vunary/gen/vneg-neonfp16arith-x16.c &
tools/xngen src/f16-vunary/neonfp16arith.c.in -D OP=SQR -D BATCH_TILE=8  -o src/f16-vunary/gen/vsqr-neonfp16arith-x8.c &
tools/xngen src/f16-vunary/neonfp16arith.c.in -D OP=SQR -D BATCH_TILE=16 -o src/f16-vunary/gen/vsqr-neonfp16arith-x16.c &

################################# x86 128-bit #################################
tools/xngen src/f16-vunary/sse2.c.in -D OP=ABS -D BATCH_TILE=8  -o src/f16-vunary/gen/vabs-sse2-x8.c &
tools/xngen src/f16-vunary/sse2.c.in -D OP=ABS -D BATCH_TILE=16 -o src/f16-vunary/gen/vabs-sse2-x16.c &
tools/xngen src/f16-vunary/sse2.c.in -D OP=NEG -D BATCH_TILE=8  -o src/f16-vunary/gen/vneg-sse2-x8.c &
tools/xngen src/f16-vunary/sse2.c.in -D OP=NEG -D BATCH_TILE=16 -o src/f16-vunary/gen/vneg-sse2-x16.c &
tools/xngen src/f16-vunary/f16c.c.in -D OP=SQR -D BATCH_TILE=8  -o src/f16-vunary/gen/vsqr-f16c-x8.c &
tools/xngen src/f16-vunary/f16c.c.in -D OP=SQR -D BATCH_TILE=16 -o src/f16-vunary/gen/vsqr-f16c-x16.c &

################################## Unit tests #################################
tools/generate-vunary-test.py --spec test/f16-vabs.yaml --output test/f16-vabs.cc &
tools/generate-vunary-test.py --spec test/f16-vneg.yaml --output test/f16-vneg.cc &
tools/generate-vunary-test.py --spec test/f16-vsqr.yaml --output test/f16-vsqr.cc &

wait
