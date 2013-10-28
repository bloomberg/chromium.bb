/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

        .text

/*
 * This test attempts to get both the assembler and the linker to generate
 * all the different kinds of nop instructions they will generate when
 * doing alignment padding in the code segment.  Running the output through
 * the validator ensures that it accepts all these nop forms.
 */

.macro try length
        /*
         * Create the circumstance where exactly |length| bytes must be
         * filled to perform '.p2align 5', and then let the assembler fill
         * it with its choice of nops.
         */
        .p2align 5
test_as_nop_\length\():
        .fill 32 - \length, 1, 0xf4
        .p2align 5
        hlt

         /*
          * Now create the same circumstance in the linker when it stitches
          * this section to the next one (which has sh_addralign=32).
          */
        .pushsection .text.nop.\length, "ax", @progbits
        .p2align 5
test_ld_nop_\length\():
        .fill 32 - \length, 1, 0xf4
        .popsection
.endm

try 1
try 2
try 3
try 4
try 5
try 6
try 7
try 8
try 9
try 10
try 11
try 12
try 13
try 14
try 15
try 16
try 17
try 18
try 19
try 20
try 21
try 22
try 23
try 24
try 25
try 26
try 27
try 29
try 30
try 31

        /*
         * This section follows .text.nop.31 and creates (by virtue of its
         * alignment) the need for the linker to generate the last nop
         * sequence.
         */
        .pushsection .text.nop.end, "ax", @progbits
        .p2align 5
        hlt
        .popsection
