/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Segment test part two - we keep this in a second file which is linked
 * just after  segment_test_main.c to work around a linker ideosyncrazy.
 * The first file has a text section of size 16384.
 * This files pads this to the desired size and also adds .rodata/.data
 */

/*
 * This file expects the following defines provided via -D
 * on the compile command line:
 * TEXT_SEGMENT_SIZE between 32k and 64k (divisible by 4)
 * RODATA_SEGMENT_SIZE
 * RWDATA_SEGMENT_SIZE
 * NOP_WORD
 * EXPECTED_BREAK
 */

#define TEXT_REST_SIZE (TEXT_SEGMENT_SIZE - 16384)


#define ALIGN(sec, n)  __attribute__ ((__used__, section(sec), aligned(n)))

int TextRest[TEXT_REST_SIZE / 4]  ALIGN(".text", 1)=
{ [0 ... TEXT_REST_SIZE / 4 - 1] = NOP_WORD };

#if RODATA_SEGMENT_SIZE != 0
char RoData[RODATA_SEGMENT_SIZE] ALIGN(".rodata", 1) =
{ [0 ... (RODATA_SEGMENT_SIZE) - 1] = 'r' };
#endif

#if RWDATA_SEGMENT_SIZE != 0
char RwData[RWDATA_SEGMENT_SIZE] ALIGN(".data", 1) =
{ [0 ... (RWDATA_SEGMENT_SIZE) - 1] = 'w' };
#endif
