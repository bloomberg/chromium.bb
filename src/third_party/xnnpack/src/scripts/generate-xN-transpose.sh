#!/bin/sh
# Copyright 2021 Google LLC
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

#################################### Scalar ###################################
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=2 TYPE=int8_t SIZE=8 -o src/x8-transposec/gen/1x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=4 TYPE=int8_t SIZE=8 -o src/x8-transposec/gen/1x4-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=1 TYPE=int8_t SIZE=8 -o src/x8-transposec/gen/2x1-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=2 TYPE=int8_t SIZE=8 -o src/x8-transposec/gen/2x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=4 TYPE=int8_t SIZE=8 -o src/x8-transposec/gen/2x4-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=1 TYPE=int8_t SIZE=8 -o src/x8-transposec/gen/4x1-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=2 TYPE=int8_t SIZE=8 -o src/x8-transposec/gen/4x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=4 TYPE=int8_t SIZE=8 -o src/x8-transposec/gen/4x4-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=2 TYPE=int16_t SIZE=16 -o src/x16-transposec/gen/1x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=4 TYPE=int16_t SIZE=16 -o src/x16-transposec/gen/1x4-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=1 TYPE=int16_t SIZE=16 -o src/x16-transposec/gen/2x1-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=2 TYPE=int16_t SIZE=16 -o src/x16-transposec/gen/2x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=4 TYPE=int16_t SIZE=16 -o src/x16-transposec/gen/2x4-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=1 TYPE=int16_t SIZE=16 -o src/x16-transposec/gen/4x1-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=2 TYPE=int16_t SIZE=16 -o src/x16-transposec/gen/4x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=4 TYPE=int16_t SIZE=16 -o src/x16-transposec/gen/4x4-scalar-int.c &
tools/xngen src/x24-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=2 -o src/x24-transposec/gen/1x2-scalar.c &
tools/xngen src/x24-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=4 -o src/x24-transposec/gen/1x4-scalar.c &
tools/xngen src/x24-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=1 -o src/x24-transposec/gen/2x1-scalar.c &
tools/xngen src/x24-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=2 -o src/x24-transposec/gen/2x2-scalar.c &
tools/xngen src/x24-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=4 -o src/x24-transposec/gen/2x4-scalar.c &
tools/xngen src/x24-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=1 -o src/x24-transposec/gen/4x1-scalar.c &
tools/xngen src/x24-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=2 -o src/x24-transposec/gen/4x2-scalar.c &
tools/xngen src/x24-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=4 -o src/x24-transposec/gen/4x4-scalar.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=2 TYPE=int SIZE=32 -o src/x32-transposec/gen/1x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=4 TYPE=int SIZE=32 -o src/x32-transposec/gen/1x4-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=1 TYPE=int SIZE=32 -o src/x32-transposec/gen/2x1-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=2 TYPE=int SIZE=32 -o src/x32-transposec/gen/2x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=4 TYPE=int SIZE=32 -o src/x32-transposec/gen/2x4-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=1 TYPE=int SIZE=32 -o src/x32-transposec/gen/4x1-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=2 TYPE=int SIZE=32 -o src/x32-transposec/gen/4x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=4 TYPE=int SIZE=32 -o src/x32-transposec/gen/4x4-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=2 TYPE=float SIZE=32 -o src/x32-transposec/gen/1x2-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=4 TYPE=float SIZE=32 -o src/x32-transposec/gen/1x4-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=1 TYPE=float SIZE=32 -o src/x32-transposec/gen/2x1-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=2 TYPE=float SIZE=32 -o src/x32-transposec/gen/2x2-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=4 TYPE=float SIZE=32 -o src/x32-transposec/gen/2x4-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=1 TYPE=float SIZE=32 -o src/x32-transposec/gen/4x1-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=2 TYPE=float SIZE=32 -o src/x32-transposec/gen/4x2-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=4 TYPE=float SIZE=32 -o src/x32-transposec/gen/4x4-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=2 TYPE=int64_t SIZE=64 -o src/x64-transposec/gen/1x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=1 TYPE=int64_t SIZE=64 -o src/x64-transposec/gen/2x1-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=2 TYPE=int64_t SIZE=64 -o src/x64-transposec/gen/2x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=1 TYPE=int64_t SIZE=64 -o src/x64-transposec/gen/4x1-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=2 TYPE=int64_t SIZE=64 -o src/x64-transposec/gen/4x2-scalar-int.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=1 TILE_WIDTH=2 TYPE=double SIZE=64 -o src/x64-transposec/gen/1x2-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=1 TYPE=double SIZE=64 -o src/x64-transposec/gen/2x1-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=2 TILE_WIDTH=2 TYPE=double SIZE=64 -o src/x64-transposec/gen/2x2-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=1 TYPE=double SIZE=64 -o src/x64-transposec/gen/4x1-scalar-float.c &
tools/xngen src/x32-transposec/scalar.c.in -D TILE_HEIGHT=4 TILE_WIDTH=2 TYPE=double SIZE=64 -o src/x64-transposec/gen/4x2-scalar-float.c &

#################################### SSE2 ###################################
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=8 -o src/x8-transposec/gen/16x16-reuse-mov-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=8 -o src/x8-transposec/gen/16x16-reuse-switch-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=16 -o src/x16-transposec/gen/8x8-reuse-mov-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=16 -o src/x16-transposec/gen/8x8-reuse-switch-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=16 -o src/x16-transposec/gen/8x8-reuse-multi-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=16 -o src/x16-transposec/gen/8x8-multi-switch-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=16 -o src/x16-transposec/gen/8x8-multi-mov-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=32 -o src/x32-transposec/gen/4x4-reuse-mov-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=32 -o src/x32-transposec/gen/4x4-reuse-switch-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=32 -o src/x32-transposec/gen/4x4-reuse-multi-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=32 -o src/x32-transposec/gen/4x4-multi-switch-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=MULTI OUT_PTRS=MULTI SIZE=32 -o src/x32-transposec/gen/4x4-multi-multi-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=32 -o src/x32-transposec/gen/4x4-multi-mov-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=64 -o src/x64-transposec/gen/2x2-reuse-mov-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=64 -o src/x64-transposec/gen/2x2-reuse-switch-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=64 -o src/x64-transposec/gen/2x2-reuse-multi-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=64 -o src/x64-transposec/gen/2x2-multi-switch-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=MULTI OUT_PTRS=MULTI SIZE=64 -o src/x64-transposec/gen/2x2-multi-multi-sse2.c &
tools/xngen src/x32-transposec/sse2.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=64 -o src/x64-transposec/gen/2x2-multi-mov-sse2.c &

#################################### ARM NEON ###############################
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=DEC SIZE=8 VECTOR_SIZE=64 -o src/x8-transposec/gen/8x8-multi-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=8 VECTOR_SIZE=64 -o src/x8-transposec/gen/8x8-multi-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=8 VECTOR_SIZE=64 -o src/x8-transposec/gen/8x8-multi-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=DEC SIZE=8 VECTOR_SIZE=64 -o src/x8-transposec/gen/8x8-reuse-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=8 VECTOR_SIZE=64 -o src/x8-transposec/gen/8x8-reuse-multi-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=8 VECTOR_SIZE=64 -o src/x8-transposec/gen/8x8-reuse-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=8 VECTOR_SIZE=64 -o src/x8-transposec/gen/8x8-reuse-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=DEC SIZE=8 VECTOR_SIZE=128 -o src/x8-transposec/gen/16x16-reuse-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=8 VECTOR_SIZE=128 -o src/x8-transposec/gen/16x16-reuse-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=8 VECTOR_SIZE=128 -o src/x8-transposec/gen/16x16-reuse-switch-zip-neon.c &

tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=DEC SIZE=16 VECTOR_SIZE=64 -o src/x16-transposec/gen/4x4-multi-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=16 VECTOR_SIZE=64 -o src/x16-transposec/gen/4x4-multi-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=MULTI SIZE=16 VECTOR_SIZE=64 -o src/x16-transposec/gen/4x4-multi-multi-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=16 VECTOR_SIZE=64 -o src/x16-transposec/gen/4x4-multi-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=DEC SIZE=16 VECTOR_SIZE=64 -o src/x16-transposec/gen/4x4-reuse-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=16 VECTOR_SIZE=64 -o src/x16-transposec/gen/4x4-reuse-multi-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=16 VECTOR_SIZE=64 -o src/x16-transposec/gen/4x4-reuse-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=16 VECTOR_SIZE=64 -o src/x16-transposec/gen/4x4-reuse-switch-zip-neon.c &


tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=DEC SIZE=16 VECTOR_SIZE=128 -o src/x16-transposec/gen/8x8-reuse-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=16 VECTOR_SIZE=128 -o src/x16-transposec/gen/8x8-reuse-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=16 VECTOR_SIZE=128 -o src/x16-transposec/gen/8x8-reuse-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=16 VECTOR_SIZE=128 -o src/x16-transposec/gen/8x8-reuse-multi-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=16 VECTOR_SIZE=128 -o src/x16-transposec/gen/8x8-multi-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=DEC SIZE=16 VECTOR_SIZE=128 -o src/x16-transposec/gen/8x8-multi-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=16 VECTOR_SIZE=128 -o src/x16-transposec/gen/8x8-multi-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=DEC SIZE=32 VECTOR_SIZE=64 -o src/x32-transposec/gen/2x2-reuse-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=32 VECTOR_SIZE=64 -o src/x32-transposec/gen/2x2-reuse-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=32 VECTOR_SIZE=64 -o src/x32-transposec/gen/2x2-reuse-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=32 VECTOR_SIZE=64 -o src/x32-transposec/gen/2x2-reuse-multi-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=32 VECTOR_SIZE=64 -o src/x32-transposec/gen/2x2-multi-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=MULTI SIZE=32 VECTOR_SIZE=64 -o src/x32-transposec/gen/2x2-multi-multi-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=DEC SIZE=32 VECTOR_SIZE=64 -o src/x32-transposec/gen/2x2-multi-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=32 VECTOR_SIZE=64 -o src/x32-transposec/gen/2x2-multi-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=DEC SIZE=32 VECTOR_SIZE=128 -o src/x32-transposec/gen/4x4-reuse-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=32 VECTOR_SIZE=128 -o src/x32-transposec/gen/4x4-reuse-mov-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=32 VECTOR_SIZE=128 -o src/x32-transposec/gen/4x4-reuse-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=32 VECTOR_SIZE=128 -o src/x32-transposec/gen/4x4-reuse-multi-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=32 VECTOR_SIZE=128 -o src/x32-transposec/gen/4x4-multi-switch-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=MULTI SIZE=32 VECTOR_SIZE=128 -o src/x32-transposec/gen/4x4-multi-multi-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=DEC SIZE=32 VECTOR_SIZE=128 -o src/x32-transposec/gen/4x4-multi-dec-zip-neon.c &
tools/xngen src/x32-transposec/neon-zip.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=32 VECTOR_SIZE=128 -o src/x32-transposec/gen/4x4-multi-mov-zip-neon.c &

#################################### WAsm SIMD ###############################
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=8 -o src/x8-transposec/gen/16x16-reuse-mov-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=8 -o src/x8-transposec/gen/16x16-reuse-switch-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=16 -o src/x16-transposec/gen/8x8-reuse-mov-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=16 -o src/x16-transposec/gen/8x8-reuse-switch-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=16 -o src/x16-transposec/gen/8x8-reuse-multi-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=16 -o src/x16-transposec/gen/8x8-multi-switch-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=16 -o src/x16-transposec/gen/8x8-multi-mov-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=REUSE OUT_PTRS=MOV SIZE=32 -o src/x32-transposec/gen/4x4-reuse-mov-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=REUSE OUT_PTRS=SWITCH SIZE=32 -o src/x32-transposec/gen/4x4-reuse-switch-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=REUSE OUT_PTRS=MULTI SIZE=32 -o src/x32-transposec/gen/4x4-reuse-multi-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=MULTI OUT_PTRS=SWITCH SIZE=32 -o src/x32-transposec/gen/4x4-multi-switch-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=MULTI OUT_PTRS=MULTI SIZE=32 -o src/x32-transposec/gen/4x4-multi-multi-wasmsimd.c &
tools/xngen src/x32-transposec/wasmsimd.c.in -D IN_PTRS=MULTI OUT_PTRS=MOV SIZE=32 -o src/x32-transposec/gen/4x4-multi-mov-wasmsimd.c &

################################## Unit tests #################################
tools/generate-transpose-test.py --spec test/x8-transpose.yaml --output=test/x8-transpose.cc &
tools/generate-transpose-test.py --spec test/x16-transpose.yaml --output=test/x16-transpose.cc &
tools/generate-transpose-test.py --spec test/x24-transpose.yaml --output=test/x24-transpose.cc &
tools/generate-transpose-test.py --spec test/x32-transpose.yaml --output=test/x32-transpose.cc &
tools/generate-transpose-test.py --spec test/x64-transpose.yaml --output=test/x64-transpose.cc &
tools/generate-transpose-test.py --spec test/xx-transpose.yaml --output=test/xx-transpose.cc &

wait
