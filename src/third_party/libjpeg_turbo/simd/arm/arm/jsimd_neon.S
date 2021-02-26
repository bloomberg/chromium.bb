/*
 * ARMv7 NEON optimizations for libjpeg-turbo
 *
 * Copyright (C) 2009-2011, Nokia Corporation and/or its subsidiary(-ies).
 *                          All Rights Reserved.
 * Author:  Siarhei Siamashka <siarhei.siamashka@nokia.com>
 * Copyright (C) 2014, Siarhei Siamashka.  All Rights Reserved.
 * Copyright (C) 2014, Linaro Limited.  All Rights Reserved.
 * Copyright (C) 2015, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2015-2016, 2018, Matthieu Darbois.  All Rights Reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack, "", %progbits  /* mark stack as non-executable */
#endif

.text
.fpu neon
.arch armv7a
.object_arch armv4
.arm
.syntax unified


#define RESPECT_STRICT_ALIGNMENT  1


/*****************************************************************************/

/* Supplementary macro for setting function attributes */
.macro asm_function fname
#ifdef __APPLE__
    .private_extern _\fname
    .globl _\fname
_\fname:
#else
    .global \fname
#ifdef __ELF__
    .hidden \fname
    .type \fname, %function
#endif
\fname:
#endif
.endm


#define CENTERJSAMPLE  128

/*****************************************************************************/

/*
 * GLOBAL(JOCTET*)
 * jsimd_huff_encode_one_block(working_state *state, JOCTET *buffer,
 *                             JCOEFPTR block, int last_dc_val,
 *                             c_derived_tbl *dctbl, c_derived_tbl *actbl)
 *
 */

.macro emit_byte BUFFER, PUT_BUFFER, PUT_BITS, ZERO, TMP
    sub             \PUT_BITS, \PUT_BITS, #0x8
    lsr             \TMP, \PUT_BUFFER, \PUT_BITS
    uxtb            \TMP, \TMP
    strb            \TMP, [\BUFFER, #1]!
    cmp             \TMP, #0xff
    /*it eq*/
    strbeq          \ZERO, [\BUFFER, #1]!
.endm

.macro put_bits PUT_BUFFER, PUT_BITS, CODE, SIZE
    /*lsl             \PUT_BUFFER, \PUT_BUFFER, \SIZE*/
    add             \PUT_BITS, \SIZE
    /*orr             \PUT_BUFFER, \PUT_BUFFER, \CODE*/
    orr             \PUT_BUFFER, \CODE, \PUT_BUFFER, lsl \SIZE
.endm

.macro checkbuf15 BUFFER, PUT_BUFFER, PUT_BITS, ZERO, TMP
  cmp               \PUT_BITS, #0x10
  blt               15f
    eor               \ZERO, \ZERO, \ZERO
    emit_byte         \BUFFER, \PUT_BUFFER, \PUT_BITS, \ZERO, \TMP
    emit_byte         \BUFFER, \PUT_BUFFER, \PUT_BITS, \ZERO, \TMP
15:
.endm

.balign 16
jsimd_huff_encode_one_block_neon_consts:
  .byte 0x01
  .byte 0x02
  .byte 0x04
  .byte 0x08
  .byte 0x10
  .byte 0x20
  .byte 0x40
  .byte 0x80

asm_function jsimd_huff_encode_one_block_neon
    push            {r4, r5, r6, r7, r8, r9, r10, r11, lr}
    add             r7, sp, #0x1c
    sub             r4, sp, #0x40
    bfc             r4, #0, #5
    mov             sp, r4           /* align sp on 32 bytes */
    vst1.64         {d8, d9, d10, d11}, [r4, :128]!
    vst1.64         {d12, d13, d14, d15}, [r4, :128]
    sub             sp, #0x140       /* reserve 320 bytes */
    str             r0, [sp, #0x18]  /* working state > sp + Ox18 */
    add             r4, sp, #0x20    /* r4 = t1 */
    ldr             lr, [r7, #0x8]   /* lr = dctbl */
    sub             r10, r1, #0x1    /* r10=buffer-- */
    ldrsh           r1, [r2]
    mov             r9, #0x10
    mov             r8, #0x1
    adr             r5, jsimd_huff_encode_one_block_neon_consts
    /* prepare data */
    vld1.8          {d26}, [r5, :64]
    veor            q8, q8, q8
    veor            q9, q9, q9
    vdup.16         q14, r9
    vdup.16         q15, r8
    veor            q10, q10, q10
    veor            q11, q11, q11
    sub             r1, r1, r3
    add             r9, r2, #0x22
    add             r8, r2, #0x18
    add             r3, r2, #0x36
    vmov.16         d0[0], r1
    vld1.16         {d2[0]}, [r9, :16]
    vld1.16         {d4[0]}, [r8, :16]
    vld1.16         {d6[0]}, [r3, :16]
    add             r1, r2, #0x2
    add             r9, r2, #0x30
    add             r8, r2, #0x26
    add             r3, r2, #0x28
    vld1.16         {d0[1]}, [r1, :16]
    vld1.16         {d2[1]}, [r9, :16]
    vld1.16         {d4[1]}, [r8, :16]
    vld1.16         {d6[1]}, [r3, :16]
    add             r1, r2, #0x10
    add             r9, r2, #0x40
    add             r8, r2, #0x34
    add             r3, r2, #0x1a
    vld1.16         {d0[2]}, [r1, :16]
    vld1.16         {d2[2]}, [r9, :16]
    vld1.16         {d4[2]}, [r8, :16]
    vld1.16         {d6[2]}, [r3, :16]
    add             r1, r2, #0x20
    add             r9, r2, #0x32
    add             r8, r2, #0x42
    add             r3, r2, #0xc
    vld1.16         {d0[3]}, [r1, :16]
    vld1.16         {d2[3]}, [r9, :16]
    vld1.16         {d4[3]}, [r8, :16]
    vld1.16         {d6[3]}, [r3, :16]
    add             r1, r2, #0x12
    add             r9, r2, #0x24
    add             r8, r2, #0x50
    add             r3, r2, #0xe
    vld1.16         {d1[0]}, [r1, :16]
    vld1.16         {d3[0]}, [r9, :16]
    vld1.16         {d5[0]}, [r8, :16]
    vld1.16         {d7[0]}, [r3, :16]
    add             r1, r2, #0x4
    add             r9, r2, #0x16
    add             r8, r2, #0x60
    add             r3, r2, #0x1c
    vld1.16         {d1[1]}, [r1, :16]
    vld1.16         {d3[1]}, [r9, :16]
    vld1.16         {d5[1]}, [r8, :16]
    vld1.16         {d7[1]}, [r3, :16]
    add             r1, r2, #0x6
    add             r9, r2, #0x8
    add             r8, r2, #0x52
    add             r3, r2, #0x2a
    vld1.16         {d1[2]}, [r1, :16]
    vld1.16         {d3[2]}, [r9, :16]
    vld1.16         {d5[2]}, [r8, :16]
    vld1.16         {d7[2]}, [r3, :16]
    add             r1, r2, #0x14
    add             r9, r2, #0xa
    add             r8, r2, #0x44
    add             r3, r2, #0x38
    vld1.16         {d1[3]}, [r1, :16]
    vld1.16         {d3[3]}, [r9, :16]
    vld1.16         {d5[3]}, [r8, :16]
    vld1.16         {d7[3]}, [r3, :16]
    vcgt.s16        q8, q8, q0
    vcgt.s16        q9, q9, q1
    vcgt.s16        q10, q10, q2
    vcgt.s16        q11, q11, q3
    vabs.s16        q0, q0
    vabs.s16        q1, q1
    vabs.s16        q2, q2
    vabs.s16        q3, q3
    veor            q8, q8, q0
    veor            q9, q9, q1
    veor            q10, q10, q2
    veor            q11, q11, q3
    add             r9, r4, #0x20
    add             r8, r4, #0x80
    add             r3, r4, #0xa0
    vclz.i16        q0, q0
    vclz.i16        q1, q1
    vclz.i16        q2, q2
    vclz.i16        q3, q3
    vsub.i16        q0, q14, q0
    vsub.i16        q1, q14, q1
    vsub.i16        q2, q14, q2
    vsub.i16        q3, q14, q3
    vst1.16         {d0, d1, d2, d3}, [r4, :256]
    vst1.16         {d4, d5, d6, d7}, [r9, :256]
    vshl.s16        q0, q15, q0
    vshl.s16        q1, q15, q1
    vshl.s16        q2, q15, q2
    vshl.s16        q3, q15, q3
    vsub.i16        q0, q0, q15
    vsub.i16        q1, q1, q15
    vsub.i16        q2, q2, q15
    vsub.i16        q3, q3, q15
    vand            q8, q8, q0
    vand            q9, q9, q1
    vand            q10, q10, q2
    vand            q11, q11, q3
    vst1.16         {d16, d17, d18, d19}, [r8, :256]
    vst1.16         {d20, d21, d22, d23}, [r3, :256]
    add             r1, r2, #0x46
    add             r9, r2, #0x3a
    add             r8, r2, #0x74
    add             r3, r2, #0x6a
    vld1.16         {d8[0]}, [r1, :16]
    vld1.16         {d10[0]}, [r9, :16]
    vld1.16         {d12[0]}, [r8, :16]
    vld1.16         {d14[0]}, [r3, :16]
    veor            q8, q8, q8
    veor            q9, q9, q9
    veor            q10, q10, q10
    veor            q11, q11, q11
    add             r1, r2, #0x54
    add             r9, r2, #0x2c
    add             r8, r2, #0x76
    add             r3, r2, #0x78
    vld1.16         {d8[1]}, [r1, :16]
    vld1.16         {d10[1]}, [r9, :16]
    vld1.16         {d12[1]}, [r8, :16]
    vld1.16         {d14[1]}, [r3, :16]
    add             r1, r2, #0x62
    add             r9, r2, #0x1e
    add             r8, r2, #0x68
    add             r3, r2, #0x7a
    vld1.16         {d8[2]}, [r1, :16]
    vld1.16         {d10[2]}, [r9, :16]
    vld1.16         {d12[2]}, [r8, :16]
    vld1.16         {d14[2]}, [r3, :16]
    add             r1, r2, #0x70
    add             r9, r2, #0x2e
    add             r8, r2, #0x5a
    add             r3, r2, #0x6c
    vld1.16         {d8[3]}, [r1, :16]
    vld1.16         {d10[3]}, [r9, :16]
    vld1.16         {d12[3]}, [r8, :16]
    vld1.16         {d14[3]}, [r3, :16]
    add             r1, r2, #0x72
    add             r9, r2, #0x3c
    add             r8, r2, #0x4c
    add             r3, r2, #0x5e
    vld1.16         {d9[0]}, [r1, :16]
    vld1.16         {d11[0]}, [r9, :16]
    vld1.16         {d13[0]}, [r8, :16]
    vld1.16         {d15[0]}, [r3, :16]
    add             r1, r2, #0x64
    add             r9, r2, #0x4a
    add             r8, r2, #0x3e
    add             r3, r2, #0x6e
    vld1.16         {d9[1]}, [r1, :16]
    vld1.16         {d11[1]}, [r9, :16]
    vld1.16         {d13[1]}, [r8, :16]
    vld1.16         {d15[1]}, [r3, :16]
    add             r1, r2, #0x56
    add             r9, r2, #0x58
    add             r8, r2, #0x4e
    add             r3, r2, #0x7c
    vld1.16         {d9[2]}, [r1, :16]
    vld1.16         {d11[2]}, [r9, :16]
    vld1.16         {d13[2]}, [r8, :16]
    vld1.16         {d15[2]}, [r3, :16]
    add             r1, r2, #0x48
    add             r9, r2, #0x66
    add             r8, r2, #0x5c
    add             r3, r2, #0x7e
    vld1.16         {d9[3]}, [r1, :16]
    vld1.16         {d11[3]}, [r9, :16]
    vld1.16         {d13[3]}, [r8, :16]
    vld1.16         {d15[3]}, [r3, :16]
    vcgt.s16        q8, q8, q4
    vcgt.s16        q9, q9, q5
    vcgt.s16        q10, q10, q6
    vcgt.s16        q11, q11, q7
    vabs.s16        q4, q4
    vabs.s16        q5, q5
    vabs.s16        q6, q6
    vabs.s16        q7, q7
    veor            q8, q8, q4
    veor            q9, q9, q5
    veor            q10, q10, q6
    veor            q11, q11, q7
    add             r1, r4, #0x40
    add             r9, r4, #0x60
    add             r8, r4, #0xc0
    add             r3, r4, #0xe0
    vclz.i16        q4, q4
    vclz.i16        q5, q5
    vclz.i16        q6, q6
    vclz.i16        q7, q7
    vsub.i16        q4, q14, q4
    vsub.i16        q5, q14, q5
    vsub.i16        q6, q14, q6
    vsub.i16        q7, q14, q7
    vst1.16         {d8, d9, d10, d11}, [r1, :256]
    vst1.16         {d12, d13, d14, d15}, [r9, :256]
    vshl.s16        q4, q15, q4
    vshl.s16        q5, q15, q5
    vshl.s16        q6, q15, q6
    vshl.s16        q7, q15, q7
    vsub.i16        q4, q4, q15
    vsub.i16        q5, q5, q15
    vsub.i16        q6, q6, q15
    vsub.i16        q7, q7, q15
    vand            q8, q8, q4
    vand            q9, q9, q5
    vand            q10, q10, q6
    vand            q11, q11, q7
    vst1.16         {d16, d17, d18, d19}, [r8, :256]
    vst1.16         {d20, d21, d22, d23}, [r3, :256]
    ldr             r12, [r7, #0xc]       /* r12 = actbl */
    add             r1, lr, #0x400        /* r1 = dctbl->ehufsi */
    mov             r9, r12               /* r9 = actbl */
    add             r6, r4, #0x80         /* r6 = t2 */
    ldr             r11, [r0, #0x8]       /* r11 = put_buffer */
    ldr             r4, [r0, #0xc]        /* r4  = put_bits */
    ldrh            r2, [r6, #-128]       /* r2  = nbits */
    ldrh            r3, [r6]              /* r3  = temp2 & (((JLONG)1)<<nbits) - 1; */
    ldr             r0, [lr, r2, lsl #2]
    ldrb            r5, [r1, r2]
    put_bits        r11, r4, r0, r5
    checkbuf15      r10, r11, r4, r5, r0
    put_bits        r11, r4, r3, r2
    checkbuf15      r10, r11, r4, r5, r0
    mov             lr, r6                /* lr = t2 */
    add             r5, r9, #0x400        /* r5 = actbl->ehufsi */
    ldrsb           r6, [r5, #0xf0]       /* r6 = actbl->ehufsi[0xf0] */
    veor            q8, q8, q8
    vceq.i16        q0, q0, q8
    vceq.i16        q1, q1, q8
    vceq.i16        q2, q2, q8
    vceq.i16        q3, q3, q8
    vceq.i16        q4, q4, q8
    vceq.i16        q5, q5, q8
    vceq.i16        q6, q6, q8
    vceq.i16        q7, q7, q8
    vmovn.i16       d0, q0
    vmovn.i16       d2, q1
    vmovn.i16       d4, q2
    vmovn.i16       d6, q3
    vmovn.i16       d8, q4
    vmovn.i16       d10, q5
    vmovn.i16       d12, q6
    vmovn.i16       d14, q7
    vand            d0, d0, d26
    vand            d2, d2, d26
    vand            d4, d4, d26
    vand            d6, d6, d26
    vand            d8, d8, d26
    vand            d10, d10, d26
    vand            d12, d12, d26
    vand            d14, d14, d26
    vpadd.i8        d0, d0, d2
    vpadd.i8        d4, d4, d6
    vpadd.i8        d8, d8, d10
    vpadd.i8        d12, d12, d14
    vpadd.i8        d0, d0, d4
    vpadd.i8        d8, d8, d12
    vpadd.i8        d0, d0, d8
    vmov.32         r1, d0[1]
    vmov.32         r8, d0[0]
    mvn             r1, r1
    mvn             r8, r8
    lsrs            r1, r1, #0x1
    rrx             r8, r8            /* shift in last r1 bit while shifting out DC bit */
    rbit            r1, r1            /* r1 = index1 */
    rbit            r8, r8            /* r8 = index0 */
    ldr             r0, [r9, #0x3c0]  /* r0 = actbl->ehufco[0xf0] */
    str             r1, [sp, #0x14]   /* index1 > sp + 0x14 */
    cmp             r8, #0x0
    beq             6f
1:
    clz             r2, r8
    add             lr, lr, r2, lsl #1
    lsl             r8, r8, r2
    ldrh            r1, [lr, #-126]
2:
    cmp             r2, #0x10
    blt             3f
    sub             r2, r2, #0x10
    put_bits        r11, r4, r0, r6
    cmp             r4, #0x10
    blt             2b
    eor             r3, r3, r3
    emit_byte       r10, r11, r4, r3, r12
    emit_byte       r10, r11, r4, r3, r12
    b               2b
3:
    add             r2, r1, r2, lsl #4
    ldrh            r3, [lr, #2]!
    ldr             r12, [r9, r2, lsl #2]
    ldrb            r2, [r5, r2]
    put_bits        r11, r4, r12, r2
    checkbuf15      r10, r11, r4, r2, r12
    put_bits        r11, r4, r3, r1
    checkbuf15      r10, r11, r4, r2, r12
    lsls            r8, r8, #0x1
    bne             1b
6:
    add             r12, sp, #0x20   /* r12 = t1 */
    ldr             r8, [sp, #0x14]  /* r8 = index1 */
    adds            r12, #0xc0       /* r12 = t2 + (DCTSIZE2/2) */
    cmp             r8, #0x0
    beq             6f
    clz             r2, r8
    sub             r12, r12, lr
    lsl             r8, r8, r2
    add             r2, r2, r12, lsr #1
    add             lr, lr, r2, lsl #1
    b               7f
1:
    clz             r2, r8
    add             lr, lr, r2, lsl #1
    lsl             r8, r8, r2
7:
    ldrh            r1, [lr, #-126]
2:
    cmp             r2, #0x10
    blt             3f
    sub             r2, r2, #0x10
    put_bits        r11, r4, r0, r6
    cmp             r4, #0x10
    blt             2b
    eor             r3, r3, r3
    emit_byte       r10, r11, r4, r3, r12
    emit_byte       r10, r11, r4, r3, r12
    b               2b
3:
    add             r2, r1, r2, lsl #4
    ldrh            r3, [lr, #2]!
    ldr             r12, [r9, r2, lsl #2]
    ldrb            r2, [r5, r2]
    put_bits        r11, r4, r12, r2
    checkbuf15      r10, r11, r4, r2, r12
    put_bits        r11, r4, r3, r1
    checkbuf15      r10, r11, r4, r2, r12
    lsls            r8, r8, #0x1
    bne             1b
6:
    add             r0, sp, #0x20
    add             r0, #0xfe
    cmp             lr, r0
    bhs             1f
    ldr             r1, [r9]
    ldrb            r0, [r5]
    put_bits        r11, r4, r1, r0
    checkbuf15      r10, r11, r4, r0, r1
1:
    ldr             r12, [sp, #0x18]
    str             r11, [r12, #0x8]
    str             r4, [r12, #0xc]
    add             r0, r10, #0x1
    add             r4, sp, #0x140
    vld1.64         {d8, d9, d10, d11}, [r4, :128]!
    vld1.64         {d12, d13, d14, d15}, [r4, :128]
    sub             r4, r7, #0x1c
    mov             sp, r4
    pop             {r4, r5, r6, r7, r8, r9, r10, r11, pc}

.purgem emit_byte
.purgem put_bits
.purgem checkbuf15
