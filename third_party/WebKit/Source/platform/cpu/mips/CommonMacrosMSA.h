// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CommonMacrosMSA_h
#define CommonMacrosMSA_h

#include <msa.h>
#include <stdint.h>

#if defined(__clang__)
#define CLANG_BUILD
#endif

#ifdef CLANG_BUILD
#define SRLI_H(a, b)  __msa_srli_h((v8i16)a, b)
#define SLLI_H(a, b)  __msa_slli_h((v8i16)a, b)
#define CEQI_H(a, b)  __msa_ceqi_h((v8i16)a, b)
#else
#define SRLI_H(a, b)  ((v8u16)a >> b)
#define SLLI_H(a, b)  ((v8i16)a << b)
#define CEQI_H(a, b)  (a == b)
#endif

#define LD_V(RTYPE, psrc) *((RTYPE*)(psrc))
#define LD_UB(...) LD_V(v16u8, __VA_ARGS__)
#define LD_UH(...) LD_V(v8u16, __VA_ARGS__)
#define LD_SP(...) LD_V(v4f32, __VA_ARGS__)
#define LD_DP(...) LD_V(v2f64, __VA_ARGS__)

#define ST_V(RTYPE, in, pdst) *((RTYPE*)(pdst)) = in
#define ST_UB(...) ST_V(v16u8, __VA_ARGS__)
#define ST_UH(...) ST_V(v8u16, __VA_ARGS__)
#define ST_SP(...) ST_V(v4f32, __VA_ARGS__)
#define ST_DP(...) ST_V(v2f64, __VA_ARGS__)

#ifdef CLANG_BUILD
#define COPY_DOUBLE_TO_VECTOR(a) ({                \
    v2f64 out;                                     \
    out = (v2f64) __msa_fill_d(*(int64_t *)(&a));  \
    out;                                           \
})
#else
#define COPY_DOUBLE_TO_VECTOR(a) ({                \
    v2f64 out;                                     \
    out = __msa_cast_to_vector_double(a);          \
    out = (v2f64) __msa_splati_d((v2i64) out, 0);  \
    out;                                           \
})
#endif

#define MSA_STORE_FUNC(TYPE, INSTR, FUNCNAME)                \
    static inline void FUNCNAME(TYPE val, void* const pdst)  \
    {                                                        \
        uint8_t* const pdstm = (uint8_t*)pdst;               \
        TYPE valm = val;                                     \
        asm volatile(                                        \
            " " #INSTR "  %[valm],  %[pdstm]  \n\t"          \
            : [pdstm] "=m" (*pdstm)                          \
            : [valm] "r" (valm));                            \
    }

#define MSA_STORE(val, pdst, FUNCNAME)  FUNCNAME(val, pdst)

#ifdef CLANG_BUILD
MSA_STORE_FUNC(uint32_t, sw, msa_sw);
#define SW(val, pdst)  MSA_STORE(val, pdst, msa_sw)
#if (__mips == 64)
MSA_STORE_FUNC(uint64_t, sd, msa_sd);
#define SD(val, pdst)  MSA_STORE(val, pdst, msa_sd)
#else
#define SD(val, pdst)                                                     \
{                                                                         \
    uint8_t* const pdstsd = (uint8_t*)(pdst);                             \
    const uint32_t val0m = (uint32_t)(val & 0x00000000FFFFFFFF);          \
    const uint32_t val1m = (uint32_t)((val >> 32) & 0x00000000FFFFFFFF);  \
    SW(val0m, pdstsd);                                                    \
    SW(val1m, pdstsd + 4);                                                \
}
#endif
#else
#if (__mips_isa_rev >= 6)
MSA_STORE_FUNC(uint32_t, sw, msa_sw);
#define SW(val, pdst)  MSA_STORE(val, pdst, msa_sw)
MSA_STORE_FUNC(uint64_t, sd, msa_sd);
#define SD(val, pdst)  MSA_STORE(val, pdst, msa_sd)
#else // !(__mips_isa_rev >= 6)
MSA_STORE_FUNC(uint32_t, usw, msa_usw);
#define SW(val, pdst)  MSA_STORE(val, pdst, msa_usw)
#define SD(val, pdst)                                                     \
{                                                                         \
    uint8_t* const pdstsd = (uint8_t*)(pdst);                             \
    const uint32_t val0m = (uint32_t)(val & 0x00000000FFFFFFFF);          \
    const uint32_t val1m = (uint32_t)((val >> 32) & 0x00000000FFFFFFFF);  \
    SW(val0m, pdstsd);                                                    \
    SW(val1m, pdstsd + 4);                                                \
}
#endif // (__mips_isa_rev >= 6)
#endif

/* Description : Load vectors with elements with stride
 * Arguments   : Inputs  - psrc, stride
 *               Outputs - out0, out1
 *               Return Type - as per RTYPE
 * Details     : Load elements in 'out0' from (psrc)
 *               Load elements in 'out1' from (psrc + stride)
 */
#define LD_V2(RTYPE, psrc, stride, out0, out1)  \
{                                               \
    out0 = LD_V(RTYPE, psrc);                   \
    psrc += stride;                             \
    out1 = LD_V(RTYPE, psrc);                   \
    psrc += stride;                             \
}
#define LD_UB2(...) LD_V2(v16u8, __VA_ARGS__)
#define LD_UH2(...) LD_V2(v8u16, __VA_ARGS__)
#define LD_SP2(...) LD_V2(v4f32, __VA_ARGS__)

#define LD_V3(RTYPE, psrc, stride, out0, out1, out2)  \
{                                                     \
    LD_V2(RTYPE, psrc, stride, out0, out1);           \
    out2 = LD_V(RTYPE, psrc);                         \
    psrc += stride;                                   \
}
#define LD_UB3(...) LD_V3(v16u8, __VA_ARGS__)
#define LD_UH3(...) LD_V3(v8u16, __VA_ARGS__)

#define LD_V4(RTYPE, psrc, stride, out0, out1, out2, out3)  \
{                                                           \
    LD_V2(RTYPE, psrc, stride, out0, out1);                 \
    LD_V2(RTYPE, psrc, stride, out2, out3);                 \
}
#define LD_UB4(...) LD_V4(v16u8, __VA_ARGS__)
#define LD_UH4(...) LD_V4(v8u16, __VA_ARGS__)
#define LD_SP4(...) LD_V4(v4f32, __VA_ARGS__)

/* Description : Store vectors of elements with stride
 * Arguments   : Inputs - in0, in1, pdst, stride
 * Details     : Store elements from 'in0' to (pdst)
 *               Store elements from 'in1' to (pdst + stride)
 */
#define ST_V2(RTYPE, in0, in1, pdst, stride)  \
{                                             \
    ST_V(RTYPE, in0, pdst);                   \
    pdst += stride;                           \
    ST_V(RTYPE, in1, pdst);                   \
    pdst += stride;                           \
}
#define ST_UB2(...) ST_V2(v16u8, __VA_ARGS__)
#define ST_UH2(...) ST_V2(v8u16, __VA_ARGS__)
#define ST_SP2(...) ST_V2(v4f32, __VA_ARGS__)

#define ST_V3(RTYPE, in0, in1, in2, pdst, stride)  \
{                                                  \
    ST_V2(RTYPE, in0, in1, pdst, stride);          \
    ST_V(RTYPE, in2, pdst);                        \
    pdst += stride;                                \
}
#define ST_UB3(...) ST_V3(v16u8, __VA_ARGS__)
#define ST_UH3(...) ST_V3(v8u16, __VA_ARGS__)

#define ST_V4(RTYPE, in0, in1, in2, in3, pdst, stride)  \
{                                                       \
    ST_V2(RTYPE, in0, in1, pdst, stride);               \
    ST_V2(RTYPE, in2, in3, pdst, stride);               \
}
#define ST_UB4(...) ST_V4(v16u8, __VA_ARGS__)
#define ST_UH4(...) ST_V4(v8u16, __VA_ARGS__)
#define ST_SP4(...) ST_V4(v4f32, __VA_ARGS__)
#define ST_V6(RTYPE, in0, in1, in2, in3, in4, in5, pdst, stride)  \
{                                                                 \
    ST_V3(RTYPE, in0, in1, in2, pdst, stride);                    \
    ST_V3(RTYPE, in3, in4, in5, pdst, stride);                    \
}
#define ST_UB6(...) ST_V6(v16u8, __VA_ARGS__)
#define ST_SP6(...) ST_V6(v4f32, __VA_ARGS__)

#define ST_V8(RTYPE, in0, in1, in2, in3, in4, in5, in6, in7, pdst, stride)  \
{                                                                           \
    ST_V4(RTYPE, in0, in1, in2, in3, pdst, stride);                         \
    ST_V4(RTYPE, in4, in5, in6, in7, pdst, stride);                         \
}
#define ST_UB8(...) ST_V8(v16u8, __VA_ARGS__)
#define ST_SP8(...) ST_V8(v4f32, __VA_ARGS__)

/* Description : Interleave even halfword elements from vectors
   Arguments   : Inputs  - in0, in1, in2, in3
                 Outputs - out0, out1
                 Return Type - as per RTYPE
   Details     : Even halfword elements of 'in0' and 'in1' are interleaved
                 and written to 'out0'
*/
#define ILVEV_H2(RTYPE, in0, in1, in2, in3, out0, out1)   \
{                                                         \
    out0 = (RTYPE)__msa_ilvev_h((v8i16)in1, (v8i16)in0);  \
    out1 = (RTYPE)__msa_ilvev_h((v8i16)in3, (v8i16)in2);  \
}
#define ILVEV_H2_UB(...) ILVEV_H2(v16u8, __VA_ARGS__)

/* Description : Interleave both left and right half of input vectors
   Arguments   : Inputs  - in0, in1
                 Outputs - out0, out1
                 Return Type - as per RTYPE
   Details     : Right half of byte elements from 'in0' and 'in1' are
                 interleaved and written to 'out0'
*/
#define ILVRL_B2(RTYPE, in0, in1, out0, out1)            \
{                                                        \
    out0 = (RTYPE)__msa_ilvr_b((v16i8)in0, (v16i8)in1);  \
    out1 = (RTYPE)__msa_ilvl_b((v16i8)in0, (v16i8)in1);  \
}
#define ILVRL_B2_UB(...) ILVRL_B2(v16u8, __VA_ARGS__)

#endif // CommonMacrosMSA_h
