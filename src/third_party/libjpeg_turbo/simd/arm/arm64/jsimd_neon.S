/*
 * ARMv8 NEON optimizations for libjpeg-turbo
 *
 * Copyright (C) 2009-2011, Nokia Corporation and/or its subsidiary(-ies).
 *                          All Rights Reserved.
 * Author:  Siarhei Siamashka <siarhei.siamashka@nokia.com>
 * Copyright (C) 2013-2014, Linaro Limited.  All Rights Reserved.
 * Author:  Ragesh Radhakrishnan <ragesh.r@linaro.org>
 * Copyright (C) 2014-2016, D. R. Commander.  All Rights Reserved.
 * Copyright (C) 2015-2016, 2018, Matthieu Darbois.  All Rights Reserved.
 * Copyright (C) 2016, Siarhei Siamashka.  All Rights Reserved.
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

#if defined(__APPLE__)
.section __DATA, __const
#elif defined(_WIN32)
.section .rdata
#else
.section .rodata, "a", %progbits
#endif

/* Constants for jsimd_huff_encode_one_block_neon() */

.balign 16
Ljsimd_huff_encode_one_block_neon_consts:
    .byte 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, \
          0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
    .byte    0,   1,   2,   3,  16,  17,  32,  33, \
            18,  19,   4,   5,   6,   7,  20,  21  /* L0 => L3 : 4 lines OK */
    .byte   34,  35,  48,  49, 255, 255,  50,  51, \
            36,  37,  22,  23,   8,   9,  10,  11  /* L0 => L3 : 4 lines OK */
    .byte    8,   9,  22,  23,  36,  37,  50,  51, \
           255, 255, 255, 255, 255, 255,  52,  53  /* L1 => L4 : 4 lines OK */
    .byte   54,  55,  40,  41,  26,  27,  12,  13, \
            14,  15,  28,  29,  42,  43,  56,  57  /* L0 => L3 : 4 lines OK */
    .byte    6,   7,  20,  21,  34,  35,  48,  49, \
            50,  51,  36,  37,  22,  23,   8,   9  /* L4 => L7 : 4 lines OK */
    .byte   42,  43,  28,  29,  14,  15,  30,  31, \
            44,  45,  58,  59, 255, 255, 255, 255  /* L1 => L4 : 4 lines OK */
    .byte  255, 255, 255, 255,  56,  57,  42,  43, \
            28,  29,  14,  15,  30,  31,  44,  45  /* L3 => L6 : 4 lines OK */
    .byte   26,  27,  40,  41,  42,  43,  28,  29, \
            14,  15,  30,  31,  44,  45,  46,  47  /* L5 => L7 : 3 lines OK */
    .byte  255, 255, 255, 255,   0,   1, 255, 255, \
           255, 255, 255, 255, 255, 255, 255, 255  /* L4 : 1 lines OK */
    .byte  255, 255, 255, 255, 255, 255, 255, 255, \
             0,   1,  16,  17,   2,   3, 255, 255  /* L5 => L6 : 2 lines OK */
    .byte  255, 255, 255, 255, 255, 255, 255, 255, \
           255, 255, 255, 255,   8,   9,  22,  23  /* L5 => L6 : 2 lines OK */
    .byte    4,   5,   6,   7, 255, 255, 255, 255, \
           255, 255, 255, 255, 255, 255, 255, 255  /* L7 : 1 line OK */

.text


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

/* Get symbol location */
.macro get_symbol_loc reg, symbol
#ifdef __APPLE__
    adrp            \reg, \symbol@PAGE
    add             \reg, \reg, \symbol@PAGEOFF
#else
    adrp            \reg, \symbol
    add             \reg, \reg, :lo12:\symbol
#endif
.endm


#define CENTERJSAMPLE  128

/*****************************************************************************/

/*
 * GLOBAL(JOCTET *)
 * jsimd_huff_encode_one_block(working_state *state, JOCTET *buffer,
 *                             JCOEFPTR block, int last_dc_val,
 *                             c_derived_tbl *dctbl, c_derived_tbl *actbl)
 *
 */

    BUFFER          .req x1
    PUT_BUFFER      .req x6
    PUT_BITS        .req x7
    PUT_BITSw       .req w7

.macro emit_byte
    sub             PUT_BITS, PUT_BITS, #0x8
    lsr             x19, PUT_BUFFER, PUT_BITS
    uxtb            w19, w19
    strb            w19, [BUFFER, #1]!
    cmp             w19, #0xff
    b.ne            14f
    strb            wzr, [BUFFER, #1]!
14:
.endm
.macro put_bits CODE, SIZE
    lsl             PUT_BUFFER, PUT_BUFFER, \SIZE
    add             PUT_BITS, PUT_BITS, \SIZE
    orr             PUT_BUFFER, PUT_BUFFER, \CODE
.endm
.macro checkbuf31
    cmp             PUT_BITS, #0x20
    b.lt            31f
    emit_byte
    emit_byte
    emit_byte
    emit_byte
31:
.endm
.macro checkbuf47
    cmp             PUT_BITS, #0x30
    b.lt            47f
    emit_byte
    emit_byte
    emit_byte
    emit_byte
    emit_byte
    emit_byte
47:
.endm

.macro generate_jsimd_huff_encode_one_block fast_tbl

.balign 16
.if \fast_tbl == 1
asm_function jsimd_huff_encode_one_block_neon
.else
asm_function jsimd_huff_encode_one_block_neon_slowtbl
.endif
    sub             sp, sp, 272
    sub             BUFFER, BUFFER, #0x1    /* BUFFER=buffer-- */
    /* Save ARM registers */
    stp             x19, x20, [sp]
    get_symbol_loc  x15, Ljsimd_huff_encode_one_block_neon_consts
    ldr             PUT_BUFFER, [x0, #0x10]
    ldr             PUT_BITSw, [x0, #0x18]
    ldrsh           w12, [x2]               /* load DC coeff in w12 */
    /* prepare data */
.if \fast_tbl == 1
    ld1             {v23.16b}, [x15], #16
    ld1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x15], #64
    ld1             {v4.16b, v5.16b, v6.16b, v7.16b}, [x15], #64
    ld1             {v16.16b, v17.16b, v18.16b, v19.16b}, [x15], #64
    ld1             {v24.16b, v25.16b, v26.16b, v27.16b}, [x2], #64
    ld1             {v28.16b, v29.16b, v30.16b, v31.16b}, [x2], #64
    sub             w12, w12, w3      /* last_dc_val, not used afterwards */
    /* ZigZag 8x8 */
    tbl             v0.16b, {v24.16b, v25.16b, v26.16b, v27.16b}, v0.16b
    tbl             v1.16b, {v24.16b, v25.16b, v26.16b, v27.16b}, v1.16b
    tbl             v2.16b, {v25.16b, v26.16b, v27.16b, v28.16b}, v2.16b
    tbl             v3.16b, {v24.16b, v25.16b, v26.16b, v27.16b}, v3.16b
    tbl             v4.16b, {v28.16b, v29.16b, v30.16b, v31.16b}, v4.16b
    tbl             v5.16b, {v25.16b, v26.16b, v27.16b, v28.16b}, v5.16b
    tbl             v6.16b, {v27.16b, v28.16b, v29.16b, v30.16b}, v6.16b
    tbl             v7.16b, {v29.16b, v30.16b, v31.16b}, v7.16b
    ins             v0.h[0], w12
    tbx             v1.16b, {v28.16b}, v16.16b
    tbx             v2.16b, {v29.16b, v30.16b}, v17.16b
    tbx             v5.16b, {v29.16b, v30.16b}, v18.16b
    tbx             v6.16b, {v31.16b}, v19.16b
.else
      add             x13, x2, #0x22
      sub             w12, w12, w3    /* last_dc_val, not used afterwards */
    ld1             {v23.16b}, [x15]
      add             x14, x2, #0x18
      add             x3, x2, #0x36
    ins             v0.h[0], w12
      add             x9, x2, #0x2
    ld1             {v1.h}[0], [x13]
      add             x15, x2, #0x30
    ld1             {v2.h}[0], [x14]
      add             x19, x2, #0x26
    ld1             {v3.h}[0], [x3]
      add             x20, x2, #0x28
    ld1             {v0.h}[1], [x9]
      add             x12, x2, #0x10
    ld1             {v1.h}[1], [x15]
      add             x13, x2, #0x40
    ld1             {v2.h}[1], [x19]
      add             x14, x2, #0x34
    ld1             {v3.h}[1], [x20]
      add             x3, x2, #0x1a
    ld1             {v0.h}[2], [x12]
      add             x9, x2, #0x20
    ld1             {v1.h}[2], [x13]
      add             x15, x2, #0x32
    ld1             {v2.h}[2], [x14]
      add             x19, x2, #0x42
    ld1             {v3.h}[2], [x3]
      add             x20, x2, #0xc
    ld1             {v0.h}[3], [x9]
      add             x12, x2, #0x12
    ld1             {v1.h}[3], [x15]
      add             x13, x2, #0x24
    ld1             {v2.h}[3], [x19]
      add             x14, x2, #0x50
    ld1             {v3.h}[3], [x20]
      add             x3, x2, #0xe
    ld1             {v0.h}[4], [x12]
      add             x9, x2, #0x4
    ld1             {v1.h}[4], [x13]
      add             x15, x2, #0x16
    ld1             {v2.h}[4], [x14]
      add             x19, x2, #0x60
    ld1             {v3.h}[4], [x3]
      add             x20, x2, #0x1c
    ld1             {v0.h}[5], [x9]
      add             x12, x2, #0x6
    ld1             {v1.h}[5], [x15]
      add             x13, x2, #0x8
    ld1             {v2.h}[5], [x19]
      add             x14, x2, #0x52
    ld1             {v3.h}[5], [x20]
      add             x3, x2, #0x2a
    ld1             {v0.h}[6], [x12]
      add             x9, x2, #0x14
    ld1             {v1.h}[6], [x13]
      add             x15, x2, #0xa
    ld1             {v2.h}[6], [x14]
      add             x19, x2, #0x44
    ld1             {v3.h}[6], [x3]
      add             x20, x2, #0x38
    ld1             {v0.h}[7], [x9]
      add             x12, x2, #0x46
    ld1             {v1.h}[7], [x15]
      add             x13, x2, #0x3a
    ld1             {v2.h}[7], [x19]
      add             x14, x2, #0x74
    ld1             {v3.h}[7], [x20]
      add             x3, x2, #0x6a
    ld1             {v4.h}[0], [x12]
      add             x9, x2, #0x54
    ld1             {v5.h}[0], [x13]
      add             x15, x2, #0x2c
    ld1             {v6.h}[0], [x14]
      add             x19, x2, #0x76
    ld1             {v7.h}[0], [x3]
      add             x20, x2, #0x78
    ld1             {v4.h}[1], [x9]
      add             x12, x2, #0x62
    ld1             {v5.h}[1], [x15]
      add             x13, x2, #0x1e
    ld1             {v6.h}[1], [x19]
      add             x14, x2, #0x68
    ld1             {v7.h}[1], [x20]
      add             x3, x2, #0x7a
    ld1             {v4.h}[2], [x12]
      add             x9, x2, #0x70
    ld1             {v5.h}[2], [x13]
      add             x15, x2, #0x2e
    ld1             {v6.h}[2], [x14]
      add             x19, x2, #0x5a
    ld1             {v7.h}[2], [x3]
      add             x20, x2, #0x6c
    ld1             {v4.h}[3], [x9]
      add             x12, x2, #0x72
    ld1             {v5.h}[3], [x15]
      add             x13, x2, #0x3c
    ld1             {v6.h}[3], [x19]
      add             x14, x2, #0x4c
    ld1             {v7.h}[3], [x20]
      add             x3, x2, #0x5e
    ld1             {v4.h}[4], [x12]
      add             x9, x2, #0x64
    ld1             {v5.h}[4], [x13]
      add             x15, x2, #0x4a
    ld1             {v6.h}[4], [x14]
      add             x19, x2, #0x3e
    ld1             {v7.h}[4], [x3]
      add             x20, x2, #0x6e
    ld1             {v4.h}[5], [x9]
      add             x12, x2, #0x56
    ld1             {v5.h}[5], [x15]
      add             x13, x2, #0x58
    ld1             {v6.h}[5], [x19]
      add             x14, x2, #0x4e
    ld1             {v7.h}[5], [x20]
      add             x3, x2, #0x7c
    ld1             {v4.h}[6], [x12]
      add             x9, x2, #0x48
    ld1             {v5.h}[6], [x13]
      add             x15, x2, #0x66
    ld1             {v6.h}[6], [x14]
      add             x19, x2, #0x5c
    ld1             {v7.h}[6], [x3]
      add             x20, x2, #0x7e
    ld1             {v4.h}[7], [x9]
    ld1             {v5.h}[7], [x15]
    ld1             {v6.h}[7], [x19]
    ld1             {v7.h}[7], [x20]
.endif
    cmlt            v24.8h, v0.8h, #0
    cmlt            v25.8h, v1.8h, #0
    cmlt            v26.8h, v2.8h, #0
    cmlt            v27.8h, v3.8h, #0
    cmlt            v28.8h, v4.8h, #0
    cmlt            v29.8h, v5.8h, #0
    cmlt            v30.8h, v6.8h, #0
    cmlt            v31.8h, v7.8h, #0
    abs             v0.8h, v0.8h
    abs             v1.8h, v1.8h
    abs             v2.8h, v2.8h
    abs             v3.8h, v3.8h
    abs             v4.8h, v4.8h
    abs             v5.8h, v5.8h
    abs             v6.8h, v6.8h
    abs             v7.8h, v7.8h
    eor             v24.16b, v24.16b, v0.16b
    eor             v25.16b, v25.16b, v1.16b
    eor             v26.16b, v26.16b, v2.16b
    eor             v27.16b, v27.16b, v3.16b
    eor             v28.16b, v28.16b, v4.16b
    eor             v29.16b, v29.16b, v5.16b
    eor             v30.16b, v30.16b, v6.16b
    eor             v31.16b, v31.16b, v7.16b
    cmeq            v16.8h, v0.8h, #0
    cmeq            v17.8h, v1.8h, #0
    cmeq            v18.8h, v2.8h, #0
    cmeq            v19.8h, v3.8h, #0
    cmeq            v20.8h, v4.8h, #0
    cmeq            v21.8h, v5.8h, #0
    cmeq            v22.8h, v6.8h, #0
    xtn             v16.8b, v16.8h
    xtn             v18.8b, v18.8h
    xtn             v20.8b, v20.8h
    xtn             v22.8b, v22.8h
      umov            w14, v0.h[0]
    xtn2            v16.16b, v17.8h
      umov            w13, v24.h[0]
    xtn2            v18.16b, v19.8h
      clz             w14, w14
    xtn2            v20.16b, v21.8h
      lsl             w13, w13, w14
    cmeq            v17.8h, v7.8h, #0
      sub             w12, w14, #32
    xtn2            v22.16b, v17.8h
      lsr             w13, w13, w14
    and             v16.16b, v16.16b, v23.16b
      neg             w12, w12
    and             v18.16b, v18.16b, v23.16b
      add             x3, x4, #0x400           /* r1 = dctbl->ehufsi */
    and             v20.16b, v20.16b, v23.16b
      add             x15, sp, #0x90           /* x15 = t2 */
    and             v22.16b, v22.16b, v23.16b
      ldr             w10, [x4, x12, lsl #2]
    addp            v16.16b, v16.16b, v18.16b
      ldrb            w11, [x3, x12]
    addp            v20.16b, v20.16b, v22.16b
      checkbuf47
    addp            v16.16b, v16.16b, v20.16b
      put_bits        x10, x11
    addp            v16.16b, v16.16b, v18.16b
      checkbuf47
    umov            x9, v16.D[0]
      put_bits        x13, x12
    cnt             v17.8b, v16.8b
      mvn             x9, x9
    addv            B18, v17.8b
      add             x4, x5, #0x400   /* x4 = actbl->ehufsi */
    umov            w12, v18.b[0]
      lsr             x9, x9, #0x1     /* clear AC coeff */
    ldr             w13, [x5, #0x3c0]  /* x13 = actbl->ehufco[0xf0] */
    rbit            x9, x9             /* x9 = index0 */
    ldrb            w14, [x4, #0xf0]   /* x14 = actbl->ehufsi[0xf0] */
    cmp             w12, #(64-8)
    add             x11, sp, #16
    b.lt            4f
    cbz             x9, 6f
    st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x11], #64
    st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x11], #64
    st1             {v24.8h, v25.8h, v26.8h, v27.8h}, [x11], #64
    st1             {v28.8h, v29.8h, v30.8h, v31.8h}, [x11], #64
1:
    clz             x2, x9
    add             x15, x15, x2, lsl #1
    lsl             x9, x9, x2
    ldrh            w20, [x15, #-126]
2:
    cmp             x2, #0x10
    b.lt            3f
    sub             x2, x2, #0x10
    checkbuf47
    put_bits        x13, x14
    b               2b
3:
    clz             w20, w20
    ldrh            w3, [x15, #2]!
    sub             w11, w20, #32
    lsl             w3, w3, w20
    neg             w11, w11
    lsr             w3, w3, w20
    add             x2, x11, x2, lsl #4
    lsl             x9, x9, #0x1
    ldr             w12, [x5, x2, lsl #2]
    ldrb            w10, [x4, x2]
    checkbuf31
    put_bits        x12, x10
    put_bits        x3, x11
    cbnz            x9, 1b
    b               6f
4:
    movi            v21.8h, #0x0010
    clz             v0.8h, v0.8h
    clz             v1.8h, v1.8h
    clz             v2.8h, v2.8h
    clz             v3.8h, v3.8h
    clz             v4.8h, v4.8h
    clz             v5.8h, v5.8h
    clz             v6.8h, v6.8h
    clz             v7.8h, v7.8h
    ushl            v24.8h, v24.8h, v0.8h
    ushl            v25.8h, v25.8h, v1.8h
    ushl            v26.8h, v26.8h, v2.8h
    ushl            v27.8h, v27.8h, v3.8h
    ushl            v28.8h, v28.8h, v4.8h
    ushl            v29.8h, v29.8h, v5.8h
    ushl            v30.8h, v30.8h, v6.8h
    ushl            v31.8h, v31.8h, v7.8h
    neg             v0.8h, v0.8h
    neg             v1.8h, v1.8h
    neg             v2.8h, v2.8h
    neg             v3.8h, v3.8h
    neg             v4.8h, v4.8h
    neg             v5.8h, v5.8h
    neg             v6.8h, v6.8h
    neg             v7.8h, v7.8h
    ushl            v24.8h, v24.8h, v0.8h
    ushl            v25.8h, v25.8h, v1.8h
    ushl            v26.8h, v26.8h, v2.8h
    ushl            v27.8h, v27.8h, v3.8h
    ushl            v28.8h, v28.8h, v4.8h
    ushl            v29.8h, v29.8h, v5.8h
    ushl            v30.8h, v30.8h, v6.8h
    ushl            v31.8h, v31.8h, v7.8h
    add             v0.8h, v21.8h, v0.8h
    add             v1.8h, v21.8h, v1.8h
    add             v2.8h, v21.8h, v2.8h
    add             v3.8h, v21.8h, v3.8h
    add             v4.8h, v21.8h, v4.8h
    add             v5.8h, v21.8h, v5.8h
    add             v6.8h, v21.8h, v6.8h
    add             v7.8h, v21.8h, v7.8h
    st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x11], #64
    st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x11], #64
    st1             {v24.8h, v25.8h, v26.8h, v27.8h}, [x11], #64
    st1             {v28.8h, v29.8h, v30.8h, v31.8h}, [x11], #64
1:
    clz             x2, x9
    add             x15, x15, x2, lsl #1
    lsl             x9, x9, x2
    ldrh            w11, [x15, #-126]
2:
    cmp             x2, #0x10
    b.lt            3f
    sub             x2, x2, #0x10
    checkbuf47
    put_bits        x13, x14
    b               2b
3:
    ldrh            w3, [x15, #2]!
    add             x2, x11, x2, lsl #4
    lsl             x9, x9, #0x1
    ldr             w12, [x5, x2, lsl #2]
    ldrb            w10, [x4, x2]
    checkbuf31
    put_bits        x12, x10
    put_bits        x3, x11
    cbnz            x9, 1b
6:
    add             x13, sp, #0x10e
    cmp             x15, x13
    b.hs            1f
    ldr             w12, [x5]
    ldrb            w14, [x4]
    checkbuf47
    put_bits        x12, x14
1:
    str             PUT_BUFFER, [x0, #0x10]
    str             PUT_BITSw, [x0, #0x18]
    ldp             x19, x20, [sp], 16
    add             x0, BUFFER, #0x1
    add             sp, sp, 256
    br              x30

.endm

generate_jsimd_huff_encode_one_block 1
generate_jsimd_huff_encode_one_block 0

    .unreq          BUFFER
    .unreq          PUT_BUFFER
    .unreq          PUT_BITS
    .unreq          PUT_BITSw

.purgem emit_byte
.purgem put_bits
.purgem checkbuf31
.purgem checkbuf47
