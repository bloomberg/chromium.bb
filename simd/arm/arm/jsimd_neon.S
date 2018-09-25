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
 * jsimd_extrgb_ycc_convert_neon
 * jsimd_extbgr_ycc_convert_neon
 * jsimd_extrgbx_ycc_convert_neon
 * jsimd_extbgrx_ycc_convert_neon
 * jsimd_extxbgr_ycc_convert_neon
 * jsimd_extxrgb_ycc_convert_neon
 *
 * Colorspace conversion RGB -> YCbCr
 */

.macro do_store size
  .if \size == 8
    vst1.8          {d20}, [Y]!
    vst1.8          {d21}, [U]!
    vst1.8          {d22}, [V]!
  .elseif \size == 4
    vst1.8          {d20[0]}, [Y]!
    vst1.8          {d20[1]}, [Y]!
    vst1.8          {d20[2]}, [Y]!
    vst1.8          {d20[3]}, [Y]!
    vst1.8          {d21[0]}, [U]!
    vst1.8          {d21[1]}, [U]!
    vst1.8          {d21[2]}, [U]!
    vst1.8          {d21[3]}, [U]!
    vst1.8          {d22[0]}, [V]!
    vst1.8          {d22[1]}, [V]!
    vst1.8          {d22[2]}, [V]!
    vst1.8          {d22[3]}, [V]!
  .elseif \size == 2
    vst1.8          {d20[4]}, [Y]!
    vst1.8          {d20[5]}, [Y]!
    vst1.8          {d21[4]}, [U]!
    vst1.8          {d21[5]}, [U]!
    vst1.8          {d22[4]}, [V]!
    vst1.8          {d22[5]}, [V]!
  .elseif \size == 1
    vst1.8          {d20[6]}, [Y]!
    vst1.8          {d21[6]}, [U]!
    vst1.8          {d22[6]}, [V]!
  .else
    .error unsupported macroblock size
  .endif
.endm

.macro do_load bpp, size
  .if \bpp == 24
    .if \size == 8
      vld3.8        {d10, d11, d12}, [RGB]!
      pld           [RGB, #128]
    .elseif \size == 4
      vld3.8        {d10[0], d11[0], d12[0]}, [RGB]!
      vld3.8        {d10[1], d11[1], d12[1]}, [RGB]!
      vld3.8        {d10[2], d11[2], d12[2]}, [RGB]!
      vld3.8        {d10[3], d11[3], d12[3]}, [RGB]!
    .elseif \size == 2
      vld3.8        {d10[4], d11[4], d12[4]}, [RGB]!
      vld3.8        {d10[5], d11[5], d12[5]}, [RGB]!
    .elseif \size == 1
      vld3.8        {d10[6], d11[6], d12[6]}, [RGB]!
    .else
      .error unsupported macroblock size
    .endif
  .elseif \bpp == 32
    .if \size == 8
      vld4.8        {d10, d11, d12, d13}, [RGB]!
      pld           [RGB, #128]
    .elseif \size == 4
      vld4.8        {d10[0], d11[0], d12[0], d13[0]}, [RGB]!
      vld4.8        {d10[1], d11[1], d12[1], d13[1]}, [RGB]!
      vld4.8        {d10[2], d11[2], d12[2], d13[2]}, [RGB]!
      vld4.8        {d10[3], d11[3], d12[3], d13[3]}, [RGB]!
    .elseif \size == 2
      vld4.8        {d10[4], d11[4], d12[4], d13[4]}, [RGB]!
      vld4.8        {d10[5], d11[5], d12[5], d13[5]}, [RGB]!
    .elseif \size == 1
      vld4.8        {d10[6], d11[6], d12[6], d13[6]}, [RGB]!
    .else
      .error unsupported macroblock size
    .endif
  .else
    .error unsupported bpp
  .endif
.endm

.macro generate_jsimd_rgb_ycc_convert_neon colorid, bpp, r_offs, g_offs, b_offs

/*
 * 2-stage pipelined RGB->YCbCr conversion
 */

.macro do_rgb_to_yuv_stage1
    vmovl.u8        q2, d1\r_offs  /* r = { d4, d5 } */
    vmovl.u8        q3, d1\g_offs  /* g = { d6, d7 } */
    vmovl.u8        q4, d1\b_offs  /* b = { d8, d9 } */
    vmull.u16       q7, d4, d0[0]
    vmlal.u16       q7, d6, d0[1]
    vmlal.u16       q7, d8, d0[2]
    vmull.u16       q8, d5, d0[0]
    vmlal.u16       q8, d7, d0[1]
    vmlal.u16       q8, d9, d0[2]
    vrev64.32       q9, q1
    vrev64.32       q13, q1
    vmlsl.u16       q9, d4, d0[3]
    vmlsl.u16       q9, d6, d1[0]
    vmlal.u16       q9, d8, d1[1]
    vmlsl.u16       q13, d5, d0[3]
    vmlsl.u16       q13, d7, d1[0]
    vmlal.u16       q13, d9, d1[1]
    vrev64.32       q14, q1
    vrev64.32       q15, q1
    vmlal.u16       q14, d4, d1[1]
    vmlsl.u16       q14, d6, d1[2]
    vmlsl.u16       q14, d8, d1[3]
    vmlal.u16       q15, d5, d1[1]
    vmlsl.u16       q15, d7, d1[2]
    vmlsl.u16       q15, d9, d1[3]
.endm

.macro do_rgb_to_yuv_stage2
    vrshrn.u32      d20, q7, #16
    vrshrn.u32      d21, q8, #16
    vshrn.u32       d22, q9, #16
    vshrn.u32       d23, q13, #16
    vshrn.u32       d24, q14, #16
    vshrn.u32       d25, q15, #16
    vmovn.u16       d20, q10       /* d20 = y */
    vmovn.u16       d21, q11       /* d21 = u */
    vmovn.u16       d22, q12       /* d22 = v */
.endm

.macro do_rgb_to_yuv
    do_rgb_to_yuv_stage1
    do_rgb_to_yuv_stage2
.endm

.macro do_rgb_to_yuv_stage2_store_load_stage1
      vrshrn.u32      d20, q7, #16
      vrshrn.u32      d21, q8, #16
      vshrn.u32       d22, q9, #16
    vrev64.32       q9, q1
      vshrn.u32       d23, q13, #16
    vrev64.32       q13, q1
      vshrn.u32       d24, q14, #16
      vshrn.u32       d25, q15, #16
    do_load         \bpp, 8
      vmovn.u16       d20, q10     /* d20 = y */
    vmovl.u8        q2, d1\r_offs  /* r = { d4, d5 } */
      vmovn.u16       d21, q11     /* d21 = u */
    vmovl.u8        q3, d1\g_offs  /* g = { d6, d7 } */
      vmovn.u16       d22, q12     /* d22 = v */
    vmovl.u8        q4, d1\b_offs  /* b = { d8, d9 } */
    vmull.u16       q7, d4, d0[0]
    vmlal.u16       q7, d6, d0[1]
    vmlal.u16       q7, d8, d0[2]
      vst1.8          {d20}, [Y]!
    vmull.u16       q8, d5, d0[0]
    vmlal.u16       q8, d7, d0[1]
    vmlal.u16       q8, d9, d0[2]
    vmlsl.u16       q9, d4, d0[3]
    vmlsl.u16       q9, d6, d1[0]
    vmlal.u16       q9, d8, d1[1]
      vst1.8          {d21}, [U]!
    vmlsl.u16       q13, d5, d0[3]
    vmlsl.u16       q13, d7, d1[0]
    vmlal.u16       q13, d9, d1[1]
    vrev64.32       q14, q1
    vrev64.32       q15, q1
    vmlal.u16       q14, d4, d1[1]
    vmlsl.u16       q14, d6, d1[2]
    vmlsl.u16       q14, d8, d1[3]
      vst1.8          {d22}, [V]!
    vmlal.u16       q15, d5, d1[1]
    vmlsl.u16       q15, d7, d1[2]
    vmlsl.u16       q15, d9, d1[3]
.endm

.balign 16
jsimd_\colorid\()_ycc_neon_consts:
  .short 19595, 38470, 7471,  11059
  .short 21709, 32768, 27439, 5329
  .short 32767, 128,   32767, 128
  .short 32767, 128,   32767, 128

asm_function jsimd_\colorid\()_ycc_convert_neon
    OUTPUT_WIDTH    .req r0
    INPUT_BUF       .req r1
    OUTPUT_BUF      .req r2
    OUTPUT_ROW      .req r3
    NUM_ROWS        .req r4

    OUTPUT_BUF0     .req r5
    OUTPUT_BUF1     .req r6
    OUTPUT_BUF2     .req OUTPUT_BUF

    RGB             .req r7
    Y               .req r8
    U               .req r9
    V               .req r10
    N               .req ip

    /* Load constants to d0, d1, d2, d3 */
    adr             ip, jsimd_\colorid\()_ycc_neon_consts
    vld1.16         {d0, d1, d2, d3}, [ip, :128]

    /* Save ARM registers and handle input arguments */
    push            {r4, r5, r6, r7, r8, r9, r10, lr}
    ldr             NUM_ROWS, [sp, #(4 * 8)]
    ldr             OUTPUT_BUF0, [OUTPUT_BUF]
    ldr             OUTPUT_BUF1, [OUTPUT_BUF, #4]
    ldr             OUTPUT_BUF2, [OUTPUT_BUF, #8]
    .unreq          OUTPUT_BUF

    /* Save NEON registers */
    vpush           {d8-d15}

    /* Outer loop over scanlines */
    cmp             NUM_ROWS, #1
    blt             9f
0:
    ldr             Y, [OUTPUT_BUF0, OUTPUT_ROW, lsl #2]
    ldr             U, [OUTPUT_BUF1, OUTPUT_ROW, lsl #2]
    mov             N, OUTPUT_WIDTH
    ldr             V, [OUTPUT_BUF2, OUTPUT_ROW, lsl #2]
    add             OUTPUT_ROW, OUTPUT_ROW, #1
    ldr             RGB, [INPUT_BUF], #4

    /* Inner loop over pixels */
    subs            N, N, #8
    blt             3f
    do_load         \bpp, 8
    do_rgb_to_yuv_stage1
    subs            N, N, #8
    blt             2f
1:
    do_rgb_to_yuv_stage2_store_load_stage1
    subs            N, N, #8
    bge             1b
2:
    do_rgb_to_yuv_stage2
    do_store        8
    tst             N, #7
    beq             8f
3:
    tst             N, #4
    beq             3f
    do_load         \bpp, 4
3:
    tst             N, #2
    beq             4f
    do_load         \bpp, 2
4:
    tst             N, #1
    beq             5f
    do_load         \bpp, 1
5:
    do_rgb_to_yuv
    tst             N, #4
    beq             6f
    do_store        4
6:
    tst             N, #2
    beq             7f
    do_store        2
7:
    tst             N, #1
    beq             8f
    do_store        1
8:
    subs            NUM_ROWS, NUM_ROWS, #1
    bgt             0b
9:
    /* Restore all registers and return */
    vpop            {d8-d15}
    pop             {r4, r5, r6, r7, r8, r9, r10, pc}

    .unreq          OUTPUT_WIDTH
    .unreq          OUTPUT_ROW
    .unreq          INPUT_BUF
    .unreq          NUM_ROWS
    .unreq          OUTPUT_BUF0
    .unreq          OUTPUT_BUF1
    .unreq          OUTPUT_BUF2
    .unreq          RGB
    .unreq          Y
    .unreq          U
    .unreq          V
    .unreq          N

.purgem do_rgb_to_yuv
.purgem do_rgb_to_yuv_stage1
.purgem do_rgb_to_yuv_stage2
.purgem do_rgb_to_yuv_stage2_store_load_stage1

.endm

/*--------------------------------- id ----- bpp R  G  B */
generate_jsimd_rgb_ycc_convert_neon extrgb,  24, 0, 1, 2
generate_jsimd_rgb_ycc_convert_neon extbgr,  24, 2, 1, 0
generate_jsimd_rgb_ycc_convert_neon extrgbx, 32, 0, 1, 2
generate_jsimd_rgb_ycc_convert_neon extbgrx, 32, 2, 1, 0
generate_jsimd_rgb_ycc_convert_neon extxbgr, 32, 3, 2, 1
generate_jsimd_rgb_ycc_convert_neon extxrgb, 32, 1, 2, 3

.purgem do_load
.purgem do_store


/*****************************************************************************/

/*
 * Load data into workspace, applying unsigned->signed conversion
 *
 * TODO: can be combined with 'jsimd_fdct_ifast_neon' to get
 *       rid of VST1.16 instructions
 */

asm_function jsimd_convsamp_neon
    SAMPLE_DATA     .req r0
    START_COL       .req r1
    WORKSPACE       .req r2
    TMP1            .req r3
    TMP2            .req r4
    TMP3            .req r5
    TMP4            .req ip

    push            {r4, r5}
    vmov.u8         d0, #128

    ldmia           SAMPLE_DATA!, {TMP1, TMP2, TMP3, TMP4}
    add             TMP1, TMP1, START_COL
    add             TMP2, TMP2, START_COL
    add             TMP3, TMP3, START_COL
    add             TMP4, TMP4, START_COL
    vld1.8          {d16}, [TMP1]
    vsubl.u8        q8, d16, d0
    vld1.8          {d18}, [TMP2]
    vsubl.u8        q9, d18, d0
    vld1.8          {d20}, [TMP3]
    vsubl.u8        q10, d20, d0
    vld1.8          {d22}, [TMP4]
    ldmia           SAMPLE_DATA!, {TMP1, TMP2, TMP3, TMP4}
    vsubl.u8        q11, d22, d0
    vst1.16         {d16, d17, d18, d19}, [WORKSPACE, :128]!
    add             TMP1, TMP1, START_COL
    add             TMP2, TMP2, START_COL
    vst1.16         {d20, d21, d22, d23}, [WORKSPACE, :128]!
    add             TMP3, TMP3, START_COL
    add             TMP4, TMP4, START_COL
    vld1.8          {d24}, [TMP1]
    vsubl.u8        q12, d24, d0
    vld1.8          {d26}, [TMP2]
    vsubl.u8        q13, d26, d0
    vld1.8          {d28}, [TMP3]
    vsubl.u8        q14, d28, d0
    vld1.8          {d30}, [TMP4]
    vsubl.u8        q15, d30, d0
    vst1.16         {d24, d25, d26, d27}, [WORKSPACE, :128]!
    vst1.16         {d28, d29, d30, d31}, [WORKSPACE, :128]!
    pop             {r4, r5}
    bx              lr

    .unreq          SAMPLE_DATA
    .unreq          START_COL
    .unreq          WORKSPACE
    .unreq          TMP1
    .unreq          TMP2
    .unreq          TMP3
    .unreq          TMP4


/*****************************************************************************/

/*
 * jsimd_fdct_ifast_neon
 *
 * This function contains a fast, not so accurate integer implementation of
 * the forward DCT (Discrete Cosine Transform). It uses the same calculations
 * and produces exactly the same output as IJG's original 'jpeg_fdct_ifast'
 * function from jfdctfst.c
 *
 * TODO: can be combined with 'jsimd_convsamp_neon' to get
 *       rid of a bunch of VLD1.16 instructions
 */

#define XFIX_0_382683433  d0[0]
#define XFIX_0_541196100  d0[1]
#define XFIX_0_707106781  d0[2]
#define XFIX_1_306562965  d0[3]

.balign 16
jsimd_fdct_ifast_neon_consts:
  .short (98 * 128)               /* XFIX_0_382683433 */
  .short (139 * 128)              /* XFIX_0_541196100 */
  .short (181 * 128)              /* XFIX_0_707106781 */
  .short (334 * 128 - 256 * 128)  /* XFIX_1_306562965 */

asm_function jsimd_fdct_ifast_neon

    DATA            .req r0
    TMP             .req ip

    vpush           {d8-d15}

    /* Load constants */
    adr             TMP, jsimd_fdct_ifast_neon_consts
    vld1.16         {d0}, [TMP, :64]

    /* Load all DATA into NEON registers with the following allocation:
     *       0 1 2 3 | 4 5 6 7
     *      ---------+--------
     *   0 | d16     | d17    | q8
     *   1 | d18     | d19    | q9
     *   2 | d20     | d21    | q10
     *   3 | d22     | d23    | q11
     *   4 | d24     | d25    | q12
     *   5 | d26     | d27    | q13
     *   6 | d28     | d29    | q14
     *   7 | d30     | d31    | q15
     */

    vld1.16         {d16, d17, d18, d19}, [DATA, :128]!
    vld1.16         {d20, d21, d22, d23}, [DATA, :128]!
    vld1.16         {d24, d25, d26, d27}, [DATA, :128]!
    vld1.16         {d28, d29, d30, d31}, [DATA, :128]
    sub             DATA, DATA, #(128 - 32)

    mov             TMP, #2
1:
    /* Transpose */
    vtrn.16         q12, q13
    vtrn.16         q10, q11
    vtrn.16         q8, q9
    vtrn.16         q14, q15
    vtrn.32         q9, q11
    vtrn.32         q13, q15
    vtrn.32         q8, q10
    vtrn.32         q12, q14
    vswp            d30, d23
    vswp            d24, d17
    vswp            d26, d19
      /* 1-D FDCT */
      vadd.s16        q2, q11, q12
    vswp            d28, d21
      vsub.s16        q12, q11, q12
      vsub.s16        q6, q10, q13
      vadd.s16        q10, q10, q13
      vsub.s16        q7, q9, q14
      vadd.s16        q9, q9, q14
      vsub.s16        q1, q8, q15
      vadd.s16        q8, q8, q15
      vsub.s16        q4, q9, q10
      vsub.s16        q5, q8, q2
      vadd.s16        q3, q9, q10
      vadd.s16        q4, q4, q5
      vadd.s16        q2, q8, q2
      vqdmulh.s16     q4, q4, XFIX_0_707106781
      vadd.s16        q11, q12, q6
      vadd.s16        q8, q2, q3
      vsub.s16        q12, q2, q3
      vadd.s16        q3, q6, q7
      vadd.s16        q7, q7, q1
      vqdmulh.s16     q3, q3, XFIX_0_707106781
      vsub.s16        q6, q11, q7
      vadd.s16        q10, q5, q4
      vqdmulh.s16     q6, q6, XFIX_0_382683433
      vsub.s16        q14, q5, q4
      vqdmulh.s16     q11, q11, XFIX_0_541196100
      vqdmulh.s16     q5, q7, XFIX_1_306562965
      vadd.s16        q4, q1, q3
      vsub.s16        q3, q1, q3
      vadd.s16        q7, q7, q6
      vadd.s16        q11, q11, q6
      vadd.s16        q7, q7, q5
      vadd.s16        q13, q3, q11
      vsub.s16        q11, q3, q11
      vadd.s16        q9, q4, q7
      vsub.s16        q15, q4, q7
    subs            TMP, TMP, #1
    bne             1b

    /* store results */
    vst1.16         {d16, d17, d18, d19}, [DATA, :128]!
    vst1.16         {d20, d21, d22, d23}, [DATA, :128]!
    vst1.16         {d24, d25, d26, d27}, [DATA, :128]!
    vst1.16         {d28, d29, d30, d31}, [DATA, :128]

    vpop            {d8-d15}
    bx              lr

    .unreq          DATA
    .unreq          TMP


/*****************************************************************************/

/*
 * GLOBAL(void)
 * jsimd_quantize_neon(JCOEFPTR coef_block, DCTELEM *divisors,
 *                     DCTELEM *workspace);
 *
 * Note: the code uses 2 stage pipelining in order to improve instructions
 *       scheduling and eliminate stalls (this provides ~15% better
 *       performance for this function on both ARM Cortex-A8 and
 *       ARM Cortex-A9 when compared to the non-pipelined variant).
 *       The instructions which belong to the second stage use different
 *       indentation for better readiability.
 */
asm_function jsimd_quantize_neon

    COEF_BLOCK      .req r0
    DIVISORS        .req r1
    WORKSPACE       .req r2

    RECIPROCAL      .req DIVISORS
    CORRECTION      .req r3
    SHIFT           .req ip
    LOOP_COUNT      .req r4

    vld1.16         {d0, d1, d2, d3}, [WORKSPACE, :128]!
    vabs.s16        q12, q0
    add             CORRECTION, DIVISORS, #(64 * 2)
    add             SHIFT, DIVISORS, #(64 * 6)
    vld1.16         {d20, d21, d22, d23}, [CORRECTION, :128]!
    vabs.s16        q13, q1
    vld1.16         {d16, d17, d18, d19}, [RECIPROCAL, :128]!
    vadd.u16        q12, q12, q10  /* add correction */
    vadd.u16        q13, q13, q11
    vmull.u16       q10, d24, d16  /* multiply by reciprocal */
    vmull.u16       q11, d25, d17
    vmull.u16       q8, d26, d18
    vmull.u16       q9, d27, d19
    vld1.16         {d24, d25, d26, d27}, [SHIFT, :128]!
    vshrn.u32       d20, q10, #16
    vshrn.u32       d21, q11, #16
    vshrn.u32       d22, q8, #16
    vshrn.u32       d23, q9, #16
    vneg.s16        q12, q12
    vneg.s16        q13, q13
    vshr.s16        q2, q0, #15    /* extract sign */
    vshr.s16        q3, q1, #15
    vshl.u16        q14, q10, q12  /* shift */
    vshl.u16        q15, q11, q13

    push            {r4, r5}
    mov             LOOP_COUNT, #3
1:
    vld1.16         {d0, d1, d2, d3}, [WORKSPACE, :128]!
      veor.u16        q14, q14, q2  /* restore sign */
    vabs.s16        q12, q0
    vld1.16         {d20, d21, d22, d23}, [CORRECTION, :128]!
    vabs.s16        q13, q1
      veor.u16        q15, q15, q3
    vld1.16         {d16, d17, d18, d19}, [RECIPROCAL, :128]!
    vadd.u16        q12, q12, q10  /* add correction */
    vadd.u16        q13, q13, q11
    vmull.u16       q10, d24, d16  /* multiply by reciprocal */
    vmull.u16       q11, d25, d17
    vmull.u16       q8, d26, d18
    vmull.u16       q9, d27, d19
      vsub.u16        q14, q14, q2
    vld1.16         {d24, d25, d26, d27}, [SHIFT, :128]!
      vsub.u16        q15, q15, q3
    vshrn.u32       d20, q10, #16
    vshrn.u32       d21, q11, #16
      vst1.16         {d28, d29, d30, d31}, [COEF_BLOCK, :128]!
    vshrn.u32       d22, q8, #16
    vshrn.u32       d23, q9, #16
    vneg.s16        q12, q12
    vneg.s16        q13, q13
    vshr.s16        q2, q0, #15    /* extract sign */
    vshr.s16        q3, q1, #15
    vshl.u16        q14, q10, q12  /* shift */
    vshl.u16        q15, q11, q13
    subs            LOOP_COUNT, LOOP_COUNT, #1
    bne             1b
    pop             {r4, r5}

      veor.u16        q14, q14, q2  /* restore sign */
      veor.u16        q15, q15, q3
      vsub.u16        q14, q14, q2
      vsub.u16        q15, q15, q3
      vst1.16         {d28, d29, d30, d31}, [COEF_BLOCK, :128]!

    bx              lr  /* return */

    .unreq          COEF_BLOCK
    .unreq          DIVISORS
    .unreq          WORKSPACE
    .unreq          RECIPROCAL
    .unreq          CORRECTION
    .unreq          SHIFT
    .unreq          LOOP_COUNT


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
