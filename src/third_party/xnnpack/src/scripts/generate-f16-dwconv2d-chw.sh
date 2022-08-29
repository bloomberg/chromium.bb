#!/bin/sh
# Copyright 2020 Google LLC
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

################################## ARM NEON ###################################

tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-1x4.c &
tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=2 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-2x4.c &
tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=3 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-3x4.c &
tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=4 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-4x4.c &
tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=5 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-5x4.c &
tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=6 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-6x4.c &

tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-1x4-acc2.c &
tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=3 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-1x4-acc3.c &
tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=4 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-1x4-acc4.c &
tools/xngen src/f16-dwconv2d-chw/3x3p1-neonfp16arith.c.in   -D ROW_TILE=2 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/3x3p1-minmax-neonfp16arith-2x4-acc2.c &

tools/xngen src/f16-dwconv2d-chw/3x3s2p1-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3s2p1-minmax-neonfp16arith-1x4.c &
tools/xngen src/f16-dwconv2d-chw/3x3s2p1-neonfp16arith.c.in -D ROW_TILE=2 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3s2p1-minmax-neonfp16arith-2x4.c &
tools/xngen src/f16-dwconv2d-chw/3x3s2p1-neonfp16arith.c.in -D ROW_TILE=3 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3s2p1-minmax-neonfp16arith-3x4.c &
tools/xngen src/f16-dwconv2d-chw/3x3s2p1-neonfp16arith.c.in -D ROW_TILE=4 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/3x3s2p1-minmax-neonfp16arith-4x4.c &

tools/xngen src/f16-dwconv2d-chw/3x3s2p1-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/3x3s2p1-minmax-neonfp16arith-1x4-acc2.c &
tools/xngen src/f16-dwconv2d-chw/3x3s2p1-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=3 -o src/f16-dwconv2d-chw/gen/3x3s2p1-minmax-neonfp16arith-1x4-acc3.c &
tools/xngen src/f16-dwconv2d-chw/3x3s2p1-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=4 -o src/f16-dwconv2d-chw/gen/3x3s2p1-minmax-neonfp16arith-1x4-acc4.c &
tools/xngen src/f16-dwconv2d-chw/3x3s2p1-neonfp16arith.c.in -D ROW_TILE=2 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/3x3s2p1-minmax-neonfp16arith-2x4-acc2.c &

tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-1x4.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=2 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-2x4.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=3 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-3x4.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=4 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-4x4.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=5 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-5x4.c &

tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-1x4-acc2.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=3 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-1x4-acc3.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=4 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-1x4-acc4.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=1 -D ACCUMULATORS=5 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-1x4-acc5.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=2 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-2x4-acc2.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=2 -D ACCUMULATORS=3 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-2x4-acc3.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=3 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-3x4-acc2.c &
tools/xngen src/f16-dwconv2d-chw/5x5p2-neonfp16arith.c.in   -D ROW_TILE=4 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/5x5p2-minmax-neonfp16arith-4x4-acc2.c &

tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-1x4.c &
tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=2 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-2x4.c &
tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=3 -D ACCUMULATORS=1 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-3x4.c &

tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-1x4-acc2.c &
tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=3 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-1x4-acc3.c &
tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=4 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-1x4-acc4.c &
tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=1 -D ACCUMULATORS=5 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-1x4-acc5.c &
tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=2 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-2x4-acc2.c &
tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=2 -D ACCUMULATORS=3 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-2x4-acc3.c &
tools/xngen src/f16-dwconv2d-chw/5x5s2p2-neonfp16arith.c.in -D ROW_TILE=3 -D ACCUMULATORS=2 -o src/f16-dwconv2d-chw/gen/5x5s2p2-minmax-neonfp16arith-3x4-acc2.c &

################################## Unit tests #################################
tools/generate-dwconv2d-chw-test.py --spec test/f16-dwconv2d-chw.yaml --output test/f16-dwconv2d-chw.cc &

wait
