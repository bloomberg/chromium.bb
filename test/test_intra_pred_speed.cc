/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

//  Test and time AOM intra-predictor functions

#include <stdio.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/md5_helper.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"

// -----------------------------------------------------------------------------

namespace {

typedef void (*AvxPredFunc)(uint8_t *dst, ptrdiff_t y_stride,
                            const uint8_t *above, const uint8_t *left);

const int kBPS = 32;
const int kTotalPixels = kBPS * kBPS;
// 4 DC variants, H, V, PAETH, SMOOTH, SMOOTH_V, SMOOTH_H
const int kNumAv1IntraFuncs = 10;
const char *kAv1IntraPredNames[kNumAv1IntraFuncs] = {
  "DC_PRED", "DC_LEFT_PRED", "DC_TOP_PRED", "DC_128_PRED",   "V_PRED",
  "H_PRED",  "PAETH_PRED",   "SMOOTH_PRED", "SMOOTH_V_PRED", "SMOOTH_H_PRED",
};

template <typename Pixel>
struct IntraPredTestMem {
  void Init(int block_width, int bd) {
    libaom_test::ACMRandom rnd(libaom_test::ACMRandom::DeterministicSeed());
    Pixel *const above = above_mem + 16;
    const int mask = (1 << bd) - 1;
    for (int i = 0; i < kTotalPixels; ++i) ref_src[i] = rnd.Rand16() & mask;
    for (int i = 0; i < kBPS; ++i) left[i] = rnd.Rand16() & mask;
    for (int i = -1; i < kBPS; ++i) above[i] = rnd.Rand16() & mask;

    ASSERT_LE(block_width, kBPS);
    for (int i = kBPS; i < 2 * kBPS; ++i) {
      left[i] = rnd.Rand16() & mask;
      above[i] = rnd.Rand16() & mask;
    }
  }

  DECLARE_ALIGNED(16, Pixel, src[kTotalPixels]);
  DECLARE_ALIGNED(16, Pixel, ref_src[kTotalPixels]);
  DECLARE_ALIGNED(16, Pixel, left[2 * kBPS]);
  DECLARE_ALIGNED(16, Pixel, above_mem[2 * kBPS + 16]);
};

// -----------------------------------------------------------------------------
// Low Bittdepth

typedef IntraPredTestMem<uint8_t> Av1IntraPredTestMem;

// Note:
// APPLY_UNIT_TESTS
// 1: Do unit tests
// 0: Generate MD5 array as required
#define APPLY_UNIT_TESTS 1

void CheckMd5Signature(const char name[], const char *const signatures[],
                       const void *data, size_t data_size, int elapsed_time,
                       int idx) {
  libaom_test::MD5 md5;
  md5.Add(reinterpret_cast<const uint8_t *>(data), data_size);
#if APPLY_UNIT_TESTS
  printf("Mode %s[%13s]: %5d ms     MD5: %s\n", name, kAv1IntraPredNames[idx],
         elapsed_time, md5.Get());
  EXPECT_STREQ(signatures[idx], md5.Get());
#else
  printf("\"%s\",\n", md5.Get());
#endif
}

void TestIntraPred(const char name[], AvxPredFunc const *pred_funcs,
                   const char *const signatures[], int block_width,
                   int block_height) {
  const int num_pixels_per_test =
      block_width * block_height * kNumAv1IntraFuncs;
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);
  Av1IntraPredTestMem intra_pred_test_mem;
  const uint8_t *const above = intra_pred_test_mem.above_mem + 16;

  intra_pred_test_mem.Init(block_width, 8);

  for (int k = 0; k < kNumAv1IntraFuncs; ++k) {
    if (pred_funcs[k] == NULL) continue;
    memcpy(intra_pred_test_mem.src, intra_pred_test_mem.ref_src,
           sizeof(intra_pred_test_mem.src));
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int num_tests = 0; num_tests < kNumTests; ++num_tests) {
      pred_funcs[k](intra_pred_test_mem.src, kBPS, above,
                    intra_pred_test_mem.left);
    }
    libaom_test::ClearSystemState();
    aom_usec_timer_mark(&timer);
    const int elapsed_time =
        static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
    CheckMd5Signature(name, signatures, intra_pred_test_mem.src,
                      sizeof(intra_pred_test_mem.src), elapsed_time, k);
  }
}

void TestIntraPred4(const char *block_name, AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures4x4[kNumAv1IntraFuncs] = {
    "e7ed7353c3383fff942e500e9bfe82fe", "2a4a26fcc6ce005eadc08354d196c8a9",
    "269d92eff86f315d9c38fe7640d85b15", "ae2960eea9f71ee3dabe08b282ec1773",
    "6c1abcc44e90148998b51acd11144e9c", "f7bb3186e1ef8a2b326037ff898cad8e",
    "59fc0e923a08cfac0a493fb38988e2bb", "9ff8bb37d9c830e6ab8ecb0c435d3c91",
    "de6937fca02354f2874dbc5dbec5d5b3", "723cf948137f7d8c7860d814e55ae67d",
  };
  static const char *const kSignatures4x8[kNumAv1IntraFuncs] = {
    "d9fbebdc85f71ab1e18461b2db4a2adc", "5ccb2a68284bc9714d94b8a06ccadbb2",
    "735d059abc2744f3ff3f9590f7191b37", "d9fbebdc85f71ab1e18461b2db4a2adc",
    "6819497c44cd0ace120add83672996ee", "7e3244f5a2d3edf81c7e962a842b97f9",
    "809350f164cd4d1650850bb0f59c3260", "1b60a394331eeab6927a6f8aaff57040",
    "5307de1bd7329ba6b281d2c1b0b457f9", "24c58a8138339846d95568efb91751db",
  };
  if (!strcmp(block_name, "intra4x4")) {
    TestIntraPred(block_name, pred_funcs, kSignatures4x4, 4, 4);
  }
  if (!strcmp(block_name, "intra4x8")) {
    TestIntraPred(block_name, pred_funcs, kSignatures4x8, 4, 8);
  }
}

void TestIntraPred8(const char *block_name, AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures8x8[kNumAv1IntraFuncs] = {
    "d8bbae5d6547cfc17e4f5f44c8730e88", "373bab6d931868d41a601d9d88ce9ac3",
    "6fdd5ff4ff79656c14747598ca9e3706", "d9661c2811d6a73674f40ffb2b841847",
    "7c722d10b19ccff0b8c171868e747385", "f81dd986eb2b50f750d3a7da716b7e27",
    "064404361748dd111a890a1470d7f0ea", "dc29b7e1f78cc8e7525d5ea4c0ab9b78",
    "97111eb1bc26bade6272015df829f1ae", "d19a8a73cc46b807f2c5e817576cc1e1",
  };
  static const char *const kSignatures8x4[kNumAv1IntraFuncs] = {
    "23f9fc11344426c9bee2e06d57dfd628", "2d71a26d1bae1fb34734de7b42fc5eb7",
    "5af9c1b2fd9d5721fad67b67b3f7c816", "00d71b17be662753813d515f197d145e",
    "bef10ec984427e28f4390f43809d10af", "77773cdfb7ed6bc882ab202a64b0a470",
    "2cc48bd66d6b0121b5221d52ccd732af", "b302155e1c9eeeafe2ba2bf68e807a46",
    "561bc8d0e76d5041ebd5168fc6a115e1", "81d0113fb1d0a9a24ffd6f1987b77948",
  };
  static const char *const kSignatures8x16[kNumAv1IntraFuncs] = {
    "c849de88b24f773dfcdd1d48d1209796", "6cb807c1897b94866a0f3d3c56ed8695",
    "d56db05a8ac7981762f5b877f486c4ef", "b4bc01eb6e59a40922ad17715cafb04b",
    "09d178439534f4062ae687c351f66d64", "644501399cf73080ac606e5cef7ca09b",
    "278076495180e17c065a95ab7278539a", "9dd7f324816f242be408ffeb0c673732",
    "f520c4a20acfa0bea1d253c6f0f040fd", "85f38df809df2c2d7c8b4a157a65cd44",
  };
  if (!strcmp(block_name, "intra8x8")) {
    TestIntraPred(block_name, pred_funcs, kSignatures8x8, 8, 8);
  }
  if (!strcmp(block_name, "intra8x4")) {
    TestIntraPred(block_name, pred_funcs, kSignatures8x4, 8, 4);
  }
  if (!strcmp(block_name, "intra8x16")) {
    TestIntraPred(block_name, pred_funcs, kSignatures8x16, 8, 16);
  }
}

void TestIntraPred16(const char *block_name, AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures16x16[kNumAv1IntraFuncs] = {
    "50971c07ce26977d30298538fffec619", "527a6b9e0dc5b21b98cf276305432bef",
    "7eff2868f80ebc2c43a4f367281d80f7", "67cd60512b54964ef6aff1bd4816d922",
    "48371c87dc95c08a33b2048f89cf6468", "b0acf2872ee411d7530af6d2625a7084",
    "93d6b5352b571805ab16a55e1bbed86a", "03764e4c0aebbc180e4e2c68fb06df2b",
    "bb6c74c9076c9f266ab11fb57060d8e6", "0c5162bc28489756ddb847b5678e6f07",
  };
  static const char *const kSignatures16x8[kNumAv1IntraFuncs] = {
    "b4cbdbdf10ce13300b4063a3daf99e04", "3731e1e6202064a9d0604d7c293ecee4",
    "6c856188c4256a06452f0d5d70cac436", "1f2192b4c8c497589484ea7bf9c944e8",
    "84011bd4b7f565119d06787840e333a0", "0e48949f7a6aa36f0d76b5d01f91124a",
    "60eff8064634b6c73b10681356baeee9", "1559aeb081a9c0c71111d6093c2ff9fd",
    "c15479b739713773e5cabb748451987b", "72e33ec12c9b67aea26d8d005fb82de2",
  };
  static const char *const kSignatures16x32[kNumAv1IntraFuncs] = {
    "abe5233d189cdbf79424721571bbaa7b", "282759f81e3cfb2e2d396fe406b72a8b",
    "e2224926c264f6f174cbc3167a233168", "6814e85c2b33f8c9415d62e80394b47b",
    "99cbbb60459c08a3061d72c4e4f6276a", "1d1567d40b8e816f8c1f71e576fe0f87",
    "36fdd371b624a075814d497c4832ec85", "8ab8da61b727442b6ff692b40d0df018",
    "e35a10ad7fdf2327e821504a90f6a6eb", "1f7211e727dc1de7d6a55d082fbdd821",
  };
  if (!strcmp(block_name, "intra16x16")) {
    TestIntraPred(block_name, pred_funcs, kSignatures16x16, 16, 16);
  }
  if (!strcmp(block_name, "intra16x8")) {
    TestIntraPred(block_name, pred_funcs, kSignatures16x8, 16, 8);
  }
  if (!strcmp(block_name, "intra16x32")) {
    TestIntraPred(block_name, pred_funcs, kSignatures16x32, 16, 32);
  }
}

void TestIntraPred32(const char *block_name, AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures32x32[kNumAv1IntraFuncs] = {
    "a0a618c900e65ae521ccc8af789729f2", "985aaa7c72b4a6c2fb431d32100cf13a",
    "10662d09febc3ca13ee4e700120daeb5", "b3b01379ba08916ef6b1b35f7d9ad51c",
    "9f4261755795af97e34679c333ec7004", "bc2c9da91ad97ef0d1610fb0a9041657",
    "ef1653982b69e1f64bee3759f3e1ec45", "1a51a675deba2c83282142eb48d3dc3d",
    "866c224746dc260cda861a7b1b383fb3", "cea23799fc3526e1b6a6ff02b42b82af",
  };
  static const char *const kSignatures32x16[kNumAv1IntraFuncs] = {
    "d1aeb8d5fdcfd3307922af01a798a4dc", "b0bcb514ebfbee065faea9d34c12ae75",
    "d6a18c63b4e909871c0137ca652fad23", "fd047f2fc1b8ffb95d0eeef3e8796a45",
    "645ab60779ea348fd93c81561c31bab9", "4409633c9db8dff41ade4292a3a56e7f",
    "5e36a11e069b31c2a739f3a9c7b37c24", "e83b9483d702cfae496991c3c7fa92c0",
    "12f6ddf98c7f30a277307f1ea935b030", "354321d6c32bbdb0739e4fa2acbf41e1",
  };
  if (!strcmp(block_name, "intra32x32")) {
    TestIntraPred(block_name, pred_funcs, kSignatures32x32, 32, 32);
  }
  if (!strcmp(block_name, "intra32x16")) {
    TestIntraPred(block_name, pred_funcs, kSignatures32x16, 32, 16);
  }
}

}  // namespace

// Defines a test case for |arch| (e.g., C, SSE2, ...) passing the predictors
// to |test_func|. The test name is 'arch.test_func', e.g., C.TestIntraPred4.
#define INTRA_PRED_TEST(arch, test_func, blk, dc, dc_left, dc_top, dc_128, v, \
                        h, paeth, smooth, smooth_v, smooth_h)                 \
  TEST(arch, DISABLED_##test_func) {                                          \
    static const AvxPredFunc aom_intra_pred[] = {                             \
      dc, dc_left, dc_top, dc_128, v, h, paeth, smooth, smooth_v, smooth_h    \
    };                                                                        \
    test_func(blk, aom_intra_pred);                                           \
  }

// -----------------------------------------------------------------------------
// 4x4

INTRA_PRED_TEST(C_1, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_c,
                aom_dc_left_predictor_4x4_c, aom_dc_top_predictor_4x4_c,
                aom_dc_128_predictor_4x4_c, aom_v_predictor_4x4_c,
                aom_h_predictor_4x4_c, aom_paeth_predictor_4x4_c,
                aom_smooth_predictor_4x4_c, aom_smooth_v_predictor_4x4_c,
                aom_smooth_h_predictor_4x4_c)

INTRA_PRED_TEST(C_2, TestIntraPred4, "intra4x8", aom_dc_predictor_4x8_c,
                aom_dc_left_predictor_4x8_c, aom_dc_top_predictor_4x8_c,
                aom_dc_128_predictor_4x8_c, aom_v_predictor_4x8_c,
                aom_h_predictor_4x8_c, aom_paeth_predictor_4x8_c,
                aom_smooth_predictor_4x8_c, aom_smooth_v_predictor_4x8_c,
                aom_smooth_h_predictor_4x8_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2_1, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_sse2,
                aom_dc_left_predictor_4x4_sse2, aom_dc_top_predictor_4x4_sse2,
                aom_dc_128_predictor_4x4_sse2, aom_v_predictor_4x4_sse2,
                aom_h_predictor_4x4_sse2, NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_2, TestIntraPred4, "intra4x8", aom_dc_predictor_4x8_sse2,
                aom_dc_left_predictor_4x8_sse2, aom_dc_top_predictor_4x8_sse2,
                aom_dc_128_predictor_4x8_sse2, aom_v_predictor_4x8_sse2,
                aom_h_predictor_4x8_sse2, NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3_1, TestIntraPred4, "intra4x4", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_4x4_ssse3,
                aom_smooth_predictor_4x4_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_2, TestIntraPred4, "intra4x8", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_4x8_ssse3,
                aom_smooth_predictor_4x8_ssse3, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_dspr2,
                NULL, NULL, NULL, NULL, aom_h_predictor_4x4_dspr2, NULL, NULL,
                NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_neon,
                aom_dc_left_predictor_4x4_neon, aom_dc_top_predictor_4x4_neon,
                aom_dc_128_predictor_4x4_neon, aom_v_predictor_4x4_neon,
                aom_h_predictor_4x4_neon, NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred4, "intra4x4", aom_dc_predictor_4x4_msa,
                aom_dc_left_predictor_4x4_msa, aom_dc_top_predictor_4x4_msa,
                aom_dc_128_predictor_4x4_msa, aom_v_predictor_4x4_msa,
                aom_h_predictor_4x4_msa, NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 8x8

INTRA_PRED_TEST(C_1, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_c,
                aom_dc_left_predictor_8x8_c, aom_dc_top_predictor_8x8_c,
                aom_dc_128_predictor_8x8_c, aom_v_predictor_8x8_c,
                aom_h_predictor_8x8_c, aom_paeth_predictor_8x8_c,
                aom_smooth_predictor_8x8_c, aom_smooth_v_predictor_8x8_c,
                aom_smooth_h_predictor_8x8_c)

INTRA_PRED_TEST(C_2, TestIntraPred8, "intra8x4", aom_dc_predictor_8x4_c,
                aom_dc_left_predictor_8x4_c, aom_dc_top_predictor_8x4_c,
                aom_dc_128_predictor_8x4_c, aom_v_predictor_8x4_c,
                aom_h_predictor_8x4_c, aom_paeth_predictor_8x4_c,
                aom_smooth_predictor_8x4_c, aom_smooth_v_predictor_8x4_c,
                aom_smooth_h_predictor_8x4_c)

INTRA_PRED_TEST(C_3, TestIntraPred8, "intra8x16", aom_dc_predictor_8x16_c,
                aom_dc_left_predictor_8x16_c, aom_dc_top_predictor_8x16_c,
                aom_dc_128_predictor_8x16_c, aom_v_predictor_8x16_c,
                aom_h_predictor_8x16_c, aom_paeth_predictor_8x16_c,
                aom_smooth_predictor_8x16_c, aom_smooth_v_predictor_8x16_c,
                aom_smooth_h_predictor_8x16_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2_1, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_sse2,
                aom_dc_left_predictor_8x8_sse2, aom_dc_top_predictor_8x8_sse2,
                aom_dc_128_predictor_8x8_sse2, aom_v_predictor_8x8_sse2,
                aom_h_predictor_8x8_sse2, NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_2, TestIntraPred8, "intra8x4", aom_dc_predictor_8x4_sse2,
                aom_dc_left_predictor_8x4_sse2, aom_dc_top_predictor_8x4_sse2,
                aom_dc_128_predictor_8x4_sse2, aom_v_predictor_8x4_sse2,
                aom_h_predictor_8x4_sse2, NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_3, TestIntraPred8, "intra8x16", aom_dc_predictor_8x16_sse2,
                aom_dc_left_predictor_8x16_sse2, aom_dc_top_predictor_8x16_sse2,
                aom_dc_128_predictor_8x16_sse2, aom_v_predictor_8x16_sse2,
                aom_h_predictor_8x16_sse2, NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3_1, TestIntraPred8, "intra8x8", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_8x8_ssse3,
                aom_smooth_predictor_8x8_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_2, TestIntraPred8, "intra8x4", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_8x4_ssse3,
                aom_smooth_predictor_8x4_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_3, TestIntraPred8, "intra8x16", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_8x16_ssse3,
                aom_smooth_predictor_8x16_ssse3, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_dspr2,
                NULL, NULL, NULL, NULL, aom_h_predictor_8x8_dspr2, NULL, NULL,
                NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_neon,
                aom_dc_left_predictor_8x8_neon, aom_dc_top_predictor_8x8_neon,
                aom_dc_128_predictor_8x8_neon, aom_v_predictor_8x8_neon,
                aom_h_predictor_8x8_neon, NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred8, "intra8x8", aom_dc_predictor_8x8_msa,
                aom_dc_left_predictor_8x8_msa, aom_dc_top_predictor_8x8_msa,
                aom_dc_128_predictor_8x8_msa, aom_v_predictor_8x8_msa,
                aom_h_predictor_8x8_msa, NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 16x16

INTRA_PRED_TEST(C_1, TestIntraPred16, "intra16x16", aom_dc_predictor_16x16_c,
                aom_dc_left_predictor_16x16_c, aom_dc_top_predictor_16x16_c,
                aom_dc_128_predictor_16x16_c, aom_v_predictor_16x16_c,
                aom_h_predictor_16x16_c, aom_paeth_predictor_16x16_c,
                aom_smooth_predictor_16x16_c, aom_smooth_v_predictor_16x16_c,
                aom_smooth_h_predictor_16x16_c)

INTRA_PRED_TEST(C_2, TestIntraPred16, "intra16x8", aom_dc_predictor_16x8_c,
                aom_dc_left_predictor_16x8_c, aom_dc_top_predictor_16x8_c,
                aom_dc_128_predictor_16x8_c, aom_v_predictor_16x8_c,
                aom_h_predictor_16x8_c, aom_paeth_predictor_16x8_c,
                aom_smooth_predictor_16x8_c, aom_smooth_v_predictor_16x8_c,
                aom_smooth_h_predictor_16x8_c)

INTRA_PRED_TEST(C_3, TestIntraPred16, "intra16x32", aom_dc_predictor_16x32_c,
                aom_dc_left_predictor_16x32_c, aom_dc_top_predictor_16x32_c,
                aom_dc_128_predictor_16x32_c, aom_v_predictor_16x32_c,
                aom_h_predictor_16x32_c, aom_paeth_predictor_16x32_c,
                aom_smooth_predictor_16x32_c, aom_smooth_v_predictor_16x32_c,
                aom_smooth_h_predictor_16x32_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2_1, TestIntraPred16, "intra16x16",
                aom_dc_predictor_16x16_sse2, aom_dc_left_predictor_16x16_sse2,
                aom_dc_top_predictor_16x16_sse2,
                aom_dc_128_predictor_16x16_sse2, aom_v_predictor_16x16_sse2,
                aom_h_predictor_16x16_sse2, NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_2, TestIntraPred16, "intra16x8",
                aom_dc_predictor_16x8_sse2, aom_dc_left_predictor_16x8_sse2,
                aom_dc_top_predictor_16x8_sse2, aom_dc_128_predictor_16x8_sse2,
                aom_v_predictor_16x8_sse2, aom_h_predictor_16x8_sse2, NULL,
                NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_3, TestIntraPred16, "intra16x32",
                aom_dc_predictor_16x32_sse2, aom_dc_left_predictor_16x32_sse2,
                aom_dc_top_predictor_16x32_sse2,
                aom_dc_128_predictor_16x32_sse2, aom_v_predictor_16x32_sse2,
                aom_h_predictor_16x32_sse2, NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3_1, TestIntraPred16, "intra16x16", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_16x16_ssse3,
                aom_smooth_predictor_16x16_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_2, TestIntraPred16, "intra16x8", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_16x8_ssse3,
                aom_smooth_predictor_16x8_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_3, TestIntraPred16, "intra16x32", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_16x32_ssse3,
                aom_smooth_predictor_16x32_ssse3, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_AVX2
INTRA_PRED_TEST(AVX2_1, TestIntraPred16, "intra16x16", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_16x16_avx2, NULL, NULL, NULL)
INTRA_PRED_TEST(AVX2_2, TestIntraPred16, "intra16x8", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_16x8_avx2, NULL, NULL, NULL)
INTRA_PRED_TEST(AVX2_3, TestIntraPred16, "intra16x32", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_16x32_avx2, NULL, NULL, NULL)
#endif  // HAVE_AVX2

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred16, "intra16x16",
                aom_dc_predictor_16x16_dspr2, NULL, NULL, NULL, NULL,
                aom_h_predictor_16x16_dspr2, NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred16, "intra16x16",
                aom_dc_predictor_16x16_neon, aom_dc_left_predictor_16x16_neon,
                aom_dc_top_predictor_16x16_neon,
                aom_dc_128_predictor_16x16_neon, aom_v_predictor_16x16_neon,
                aom_h_predictor_16x16_neon, NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred16, "intra16x16", aom_dc_predictor_16x16_msa,
                aom_dc_left_predictor_16x16_msa, aom_dc_top_predictor_16x16_msa,
                aom_dc_128_predictor_16x16_msa, aom_v_predictor_16x16_msa,
                aom_h_predictor_16x16_msa, NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 32x32

INTRA_PRED_TEST(C_1, TestIntraPred32, "intra32x32", aom_dc_predictor_32x32_c,
                aom_dc_left_predictor_32x32_c, aom_dc_top_predictor_32x32_c,
                aom_dc_128_predictor_32x32_c, aom_v_predictor_32x32_c,
                aom_h_predictor_32x32_c, aom_paeth_predictor_32x32_c,
                aom_smooth_predictor_32x32_c, aom_smooth_v_predictor_32x32_c,
                aom_smooth_h_predictor_32x32_c)

INTRA_PRED_TEST(C_2, TestIntraPred32, "intra32x16", aom_dc_predictor_32x16_c,
                aom_dc_left_predictor_32x16_c, aom_dc_top_predictor_32x16_c,
                aom_dc_128_predictor_32x16_c, aom_v_predictor_32x16_c,
                aom_h_predictor_32x16_c, aom_paeth_predictor_32x16_c,
                aom_smooth_predictor_32x16_c, aom_smooth_v_predictor_32x16_c,
                aom_smooth_h_predictor_32x16_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2_1, TestIntraPred32, "intra32x32",
                aom_dc_predictor_32x32_sse2, aom_dc_left_predictor_32x32_sse2,
                aom_dc_top_predictor_32x32_sse2,
                aom_dc_128_predictor_32x32_sse2, aom_v_predictor_32x32_sse2,
                aom_h_predictor_32x32_sse2, NULL, NULL, NULL, NULL)
INTRA_PRED_TEST(SSE2_2, TestIntraPred32, "intra32x16",
                aom_dc_predictor_32x16_sse2, aom_dc_left_predictor_32x16_sse2,
                aom_dc_top_predictor_32x16_sse2,
                aom_dc_128_predictor_32x16_sse2, aom_v_predictor_32x16_sse2,
                aom_h_predictor_32x16_sse2, NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3_1, TestIntraPred32, "intra32x32", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_32x32_ssse3,
                aom_smooth_predictor_32x32_ssse3, NULL, NULL)
INTRA_PRED_TEST(SSSE3_2, TestIntraPred32, "intra32x16", NULL, NULL, NULL, NULL,
                NULL, NULL, aom_paeth_predictor_32x16_ssse3,
                aom_smooth_predictor_32x16_ssse3, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_AVX2
INTRA_PRED_TEST(AVX2_1, TestIntraPred32, "intra32x32",
                aom_dc_predictor_32x32_avx2, aom_dc_left_predictor_32x32_avx2,
                aom_dc_top_predictor_32x32_avx2,
                aom_dc_128_predictor_32x32_avx2, aom_v_predictor_32x32_avx2,
                aom_h_predictor_32x32_avx2, aom_paeth_predictor_32x32_avx2,
                NULL, NULL, NULL)
INTRA_PRED_TEST(AVX2_2, TestIntraPred32, "intra32x16",
                aom_dc_predictor_32x16_avx2, aom_dc_left_predictor_32x16_avx2,
                aom_dc_top_predictor_32x16_avx2,
                aom_dc_128_predictor_32x16_avx2, aom_v_predictor_32x16_avx2,
                NULL, aom_paeth_predictor_32x16_avx2, NULL, NULL, NULL)
#endif  // HAVE_AVX2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred32, "intra32x32",
                aom_dc_predictor_32x32_neon, aom_dc_left_predictor_32x32_neon,
                aom_dc_top_predictor_32x32_neon,
                aom_dc_128_predictor_32x32_neon, aom_v_predictor_32x32_neon,
                aom_h_predictor_32x32_neon, NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred32, "intra32x32", aom_dc_predictor_32x32_msa,
                aom_dc_left_predictor_32x32_msa, aom_dc_top_predictor_32x32_msa,
                aom_dc_128_predictor_32x32_msa, aom_v_predictor_32x32_msa,
                aom_h_predictor_32x32_msa, NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// High Bitdepth
namespace {

typedef void (*AvxHighbdPredFunc)(uint16_t *dst, ptrdiff_t y_stride,
                                  const uint16_t *above, const uint16_t *left,
                                  int bd);

typedef IntraPredTestMem<uint16_t> Av1HighbdIntraPredTestMem;

void TestHighbdIntraPred(const char name[], AvxHighbdPredFunc const *pred_funcs,
                         const char *const signatures[], int block_width,
                         int block_height) {
  const int num_pixels_per_test =
      block_width * block_height * kNumAv1IntraFuncs;
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);
  Av1HighbdIntraPredTestMem intra_pred_test_mem;
  const uint16_t *const above = intra_pred_test_mem.above_mem + 16;
  const int bd = 12;

  intra_pred_test_mem.Init(block_width, bd);

  for (int k = 0; k < kNumAv1IntraFuncs; ++k) {
    if (pred_funcs[k] == NULL) continue;
    memcpy(intra_pred_test_mem.src, intra_pred_test_mem.ref_src,
           sizeof(intra_pred_test_mem.src));
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int num_tests = 0; num_tests < kNumTests; ++num_tests) {
      pred_funcs[k](intra_pred_test_mem.src, kBPS, above,
                    intra_pred_test_mem.left, bd);
    }
    libaom_test::ClearSystemState();
    aom_usec_timer_mark(&timer);
    const int elapsed_time =
        static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
    CheckMd5Signature(name, signatures, intra_pred_test_mem.src,
                      sizeof(intra_pred_test_mem.src), elapsed_time, k);
  }
}

void TestHighbdIntraPred4(const char *block_name,
                          AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures4x4[kNumAv1IntraFuncs] = {
    "11f74af6c5737df472f3275cbde062fa", "51bea056b6447c93f6eb8f6b7e8f6f71",
    "27e97f946766331795886f4de04c5594", "53ab15974b049111fb596c5168ec7e3f",
    "f0b640bb176fbe4584cf3d32a9b0320a", "729783ca909e03afd4b47111c80d967b",
    "6e30009c45474a22032678b1bd579c8f", "e57cba016d808aa8a35619df2a65f049",
    "55a6c37f39afcbbf5abca4a985b96459", "a623d45b37dafec1f8a75c4c5218913d",
  };
  static const char *const kSignatures4x8[kNumAv1IntraFuncs] = {
    "22d519b796d59644043466320e4ccd14", "09513a738c49b3f9542d27f34abbe1d5",
    "807ae5e8813443ff01e71be6efacfb69", "cbfa18d0293430b6e9708b0be1fd2394",
    "346c354c34ec7fa780b576db355dab88", "f97dae85c35359632380b09ca98d611e",
    "698ae351d8896d89ed9e4e67b6e53eda", "dcc197034a9c45a3d8238bf085835f4e",
    "7a35e2c42ffdc2efc2d6d1d75a100fc7", "41ab6cebd4516c87a91b2a593e2c2506",
  };

  if (!strcmp(block_name, "Hbd Intra4x4")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures4x4, 4, 4);
  }
  if (!strcmp(block_name, "Hbd Intra4x8")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures4x8, 4, 8);
  }
}

void TestHighbdIntraPred8(const char *block_name,
                          AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures8x8[kNumAv1IntraFuncs] = {
    "03da8829fe94663047fd108c5fcaa71d", "ecdb37b8120a2d3a4c706b016bd1bfd7",
    "1d4543ed8d2b9368cb96898095fe8a75", "f791c9a67b913cbd82d9da8ecede30e2",
    "065c70646f4dbaff913282f55a45a441", "51f87123616662ef7c35691497dfd0ba",
    "85c01ba03df68f9ece7bd3fa0f8980e6", "ad19b7dac092f56df6d054e1f67f21e7",
    "0edc415b5dd7299f7a34fb9f71d31d78", "2bc8ec19e9f4b77a64b8a0a1f6aec7e7",
  };
  static const char *const kSignatures8x4[kNumAv1IntraFuncs] = {
    "d58cd4c4bf3b7bbaa5db5e1a5622ec78", "6e572c35aa782d00cafcb99e9ea047ea",
    "e8c22a3702b416dc9ab974505afbed09", "aaa4e4762a795aad7ad74de0c662c4e4",
    "a19f9101967383c3dcbd516dc317a291", "9ab8cb91f1a595b9ebe3fe8de58031aa",
    "2cf9021d5f1169268699807ee118b65f", "ee9605fcbd6fb871f1c5cd81a6989327",
    "b4871af8316089e3e23522175df7e93f", "d33301e1c2cb173be46792a22d19881a",
  };
  static const char *const kSignatures8x16[kNumAv1IntraFuncs] = {
    "4562de1d0336610880fdd5685498a9ec", "16310fa7076394f16fc85c4b149d89c9",
    "0e94af88e1dc573b6f0f499cddd1f530", "dfd245ee20d091c67809160340365aa9",
    "d3562504327f70c096c5be23fd8a3747", "601b853558502acbb5135eadd2da117a",
    "3c624345a723a1b2b1bea05a6a08bc99", "2a9c781de609e0184cc7ab442050f4e5",
    "0ddc5035c22252747126b61fc238c74d", "e43f5d83bab759af69c7b6773fc8f9b2",
  };
  if (!strcmp(block_name, "Hbd Intra8x8")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures8x8, 8, 8);
  }
  if (!strcmp(block_name, "Hbd Intra8x4")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures8x4, 8, 4);
  }
  if (!strcmp(block_name, "Hbd Intra8x16")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures8x16, 8, 16);
  }
}

void TestHighbdIntraPred16(const char *block_name,
                           AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures16x16[kNumAv1IntraFuncs] = {
    "e33cb3f56a878e2fddb1b2fc51cdd275", "c7bff6f04b6052c8ab335d726dbbd52d",
    "d0b0b47b654a9bcc5c6008110a44589b", "78f5da7b10b2b9ab39f114a33b6254e9",
    "c78e31d23831abb40d6271a318fdd6f3", "90d1347f4ec9198a0320daecb6ff90b8",
    "e63ded54ab3d0e8728b6f24d4f01e53f", "35ce21fbe0ea114c089fc3489a78155d",
    "f277f6ef8e4d717f1f0dfe2706ac197d", "e8014d3f41256976c02e0f1e622ba2b9",
  };
  static const char *const kSignatures16x8[kNumAv1IntraFuncs] = {
    "a57d6b5a9bfd30c29591d8717ace9c51", "f5907ba97ee6c53e339e953fc8d845ee",
    "ea3aa727913ce45af06f89dd1808db5f", "408af4f23e48d14b48ee35ae094fcd18",
    "85c41cbcb5d744f7961e8950026fbffe", "8a4e588a837638887ba671f8d4910485",
    "b792d8826b67a21757ea7097cff9e05b", "f94ce7101bb87fd3bb9312112527dbf4",
    "688c6660a6dc6fa61fa1aa38e708c209", "0cdf641b4f81d69509c92ae0b93ef5ff",
  };
  static const char *const kSignatures16x32[kNumAv1IntraFuncs] = {
    "aee4b3b0e3cc02d48e2c40d77f807927", "8baef2b2e789f79c8df9d90ad10f34a4",
    "038c38ee3c4f090bb8d736eab136aafc", "1a3de2aaeaffd68a9fd6c7f6557b83f3",
    "385c6e0ea29421dd81011a2934641e26", "6cf96c285d1a2d4787f955dad715b08c",
    "2d7f75dcd73b9528c8396279ff09ff3a", "5a63cd1841e4ed470e4ca5ef845f2281",
    "610d899ca945fbead33287d4335a8b32", "6bafaad81fce37be46730187e78d8b11",
  };
  if (!strcmp(block_name, "Hbd Intra16x16")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures16x16, 16, 16);
  }
  if (!strcmp(block_name, "Hbd Intra16x8")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures16x8, 16, 8);
  }
  if (!strcmp(block_name, "Hbd Intra16x32")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures16x32, 16, 32);
  }
}

void TestHighbdIntraPred32(const char *block_name,
                           AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures32x32[kNumAv1IntraFuncs] = {
    "a3e8056ba7e36628cce4917cd956fedd", "cc7d3024fe8748b512407edee045377e",
    "2aab0a0f330a1d3e19b8ecb8f06387a3", "a547bc3fb7b06910bf3973122a426661",
    "26f712514da95042f93d6e8dc8e431dc", "bb08c6e16177081daa3d936538dbc2e3",
    "84bf83f94a51b33654ca940c6f8bc057", "7168b03fc31bf29596a344d6a35d007c",
    "b073a70d3672f1282236994f5d12e94b", "c51607aebad5dcb3c1e3b58ef9e5b84e",
  };
  static const char *const kSignatures32x16[kNumAv1IntraFuncs] = {
    "290b23c9f5a1de7905bfa71a942da29b", "701e7b82593c66da5052fc4b6afd79ce",
    "4da828c5455cd246735a663fbb204989", "e3fbeaf234efece8dbd752b77226200c",
    "4d1d8c969f05155a7e7e84cf7aad021b", "c22e4877c2c946d5bdc0d542e29e70cf",
    "8ac1ce815e7780500f842b0beb0bb980", "9fee2e2502b507f25bfad30a55b0b610",
    "4ced9c212ec6f9956e27f68a91b59fef", "4a7a0b93f138bb0863e4e465b01ec0b1",
  };
  if (!strcmp(block_name, "Hbd Intra32x32")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures32x32, 32, 32);
  }
  if (!strcmp(block_name, "Hbd Intra32x16")) {
    TestHighbdIntraPred(block_name, pred_funcs, kSignatures32x16, 32, 16);
  }
}

}  // namespace

#define HIGHBD_INTRA_PRED_TEST(arch, test_func, block_size, dc, dc_left,      \
                               dc_top, dc_128, v, h, paeth, smooth, smooth_v, \
                               smooth_h)                                      \
  TEST(arch, DISABLED_##test_func) {                                          \
    static const AvxHighbdPredFunc aom_intra_pred[] = {                       \
      dc, dc_left, dc_top, dc_128, v, h, paeth, smooth, smooth_v, smooth_h    \
    };                                                                        \
    test_func(block_size, aom_intra_pred);                                    \
  }

// -----------------------------------------------------------------------------
// 4x4

HIGHBD_INTRA_PRED_TEST(
    C_1, TestHighbdIntraPred4, "Hbd Intra4x4", aom_highbd_dc_predictor_4x4_c,
    aom_highbd_dc_left_predictor_4x4_c, aom_highbd_dc_top_predictor_4x4_c,
    aom_highbd_dc_128_predictor_4x4_c, aom_highbd_v_predictor_4x4_c,
    aom_highbd_h_predictor_4x4_c, aom_highbd_paeth_predictor_4x4_c,
    aom_highbd_smooth_predictor_4x4_c, aom_highbd_smooth_v_predictor_4x4_c,
    aom_highbd_smooth_h_predictor_4x4_c)

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2_1, TestHighbdIntraPred4, "Hbd Intra4x4",
                       aom_highbd_dc_predictor_4x4_sse2,
                       aom_highbd_dc_left_predictor_4x4_sse2,
                       aom_highbd_dc_top_predictor_4x4_sse2,
                       aom_highbd_dc_128_predictor_4x4_sse2,
                       aom_highbd_v_predictor_4x4_sse2,
                       aom_highbd_h_predictor_4x4_sse2, NULL, NULL, NULL, NULL)

HIGHBD_INTRA_PRED_TEST(SSE2_2, TestHighbdIntraPred4, "Hbd Intra4x8",
                       aom_highbd_dc_predictor_4x8_sse2,
                       aom_highbd_dc_left_predictor_4x8_sse2,
                       aom_highbd_dc_top_predictor_4x8_sse2,
                       aom_highbd_dc_128_predictor_4x8_sse2,
                       aom_highbd_v_predictor_4x8_sse2,
                       aom_highbd_h_predictor_4x8_sse2, NULL, NULL, NULL, NULL)
#endif

HIGHBD_INTRA_PRED_TEST(
    C_2, TestHighbdIntraPred4, "Hbd Intra4x8", aom_highbd_dc_predictor_4x8_c,
    aom_highbd_dc_left_predictor_4x8_c, aom_highbd_dc_top_predictor_4x8_c,
    aom_highbd_dc_128_predictor_4x8_c, aom_highbd_v_predictor_4x8_c,
    aom_highbd_h_predictor_4x8_c, aom_highbd_paeth_predictor_4x8_c,
    aom_highbd_smooth_predictor_4x8_c, aom_highbd_smooth_v_predictor_4x8_c,
    aom_highbd_smooth_h_predictor_4x8_c)

// -----------------------------------------------------------------------------
// 8x8

HIGHBD_INTRA_PRED_TEST(
    C_1, TestHighbdIntraPred8, "Hbd Intra8x8", aom_highbd_dc_predictor_8x8_c,
    aom_highbd_dc_left_predictor_8x8_c, aom_highbd_dc_top_predictor_8x8_c,
    aom_highbd_dc_128_predictor_8x8_c, aom_highbd_v_predictor_8x8_c,
    aom_highbd_h_predictor_8x8_c, aom_highbd_paeth_predictor_8x8_c,
    aom_highbd_smooth_predictor_8x8_c, aom_highbd_smooth_v_predictor_8x8_c,
    aom_highbd_smooth_h_predictor_8x8_c)

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2_1, TestHighbdIntraPred8, "Hbd Intra8x8",
                       aom_highbd_dc_predictor_8x8_sse2,
                       aom_highbd_dc_left_predictor_8x8_sse2,
                       aom_highbd_dc_top_predictor_8x8_sse2,
                       aom_highbd_dc_128_predictor_8x8_sse2,
                       aom_highbd_v_predictor_8x8_sse2,
                       aom_highbd_h_predictor_8x8_sse2, NULL, NULL, NULL, NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_2, TestHighbdIntraPred8, "Hbd Intra8x4",
                       aom_highbd_dc_predictor_8x4_sse2,
                       aom_highbd_dc_left_predictor_8x4_sse2,
                       aom_highbd_dc_top_predictor_8x4_sse2,
                       aom_highbd_dc_128_predictor_8x4_sse2,
                       aom_highbd_v_predictor_8x4_sse2,
                       aom_highbd_h_predictor_8x4_sse2, NULL, NULL, NULL, NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_3, TestHighbdIntraPred8, "Hbd Intra8x16",
                       aom_highbd_dc_predictor_8x16_sse2,
                       aom_highbd_dc_left_predictor_8x16_sse2,
                       aom_highbd_dc_top_predictor_8x16_sse2,
                       aom_highbd_dc_128_predictor_8x16_sse2,
                       aom_highbd_v_predictor_8x16_sse2,
                       aom_highbd_h_predictor_8x16_sse2, NULL, NULL, NULL, NULL)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3, TestHighbdIntraPred8, "Hbd Intra8x8", NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif

HIGHBD_INTRA_PRED_TEST(
    C_2, TestHighbdIntraPred8, "Hbd Intra8x4", aom_highbd_dc_predictor_8x4_c,
    aom_highbd_dc_left_predictor_8x4_c, aom_highbd_dc_top_predictor_8x4_c,
    aom_highbd_dc_128_predictor_8x4_c, aom_highbd_v_predictor_8x4_c,
    aom_highbd_h_predictor_8x4_c, aom_highbd_paeth_predictor_8x4_c,
    aom_highbd_smooth_predictor_8x4_c, aom_highbd_smooth_v_predictor_8x4_c,
    aom_highbd_smooth_h_predictor_8x4_c)

HIGHBD_INTRA_PRED_TEST(
    C_3, TestHighbdIntraPred8, "Hbd Intra8x16", aom_highbd_dc_predictor_8x16_c,
    aom_highbd_dc_left_predictor_8x16_c, aom_highbd_dc_top_predictor_8x16_c,
    aom_highbd_dc_128_predictor_8x16_c, aom_highbd_v_predictor_8x16_c,
    aom_highbd_h_predictor_8x16_c, aom_highbd_paeth_predictor_8x16_c,
    aom_highbd_smooth_predictor_8x16_c, aom_highbd_smooth_v_predictor_8x16_c,
    aom_highbd_smooth_h_predictor_8x16_c)

// -----------------------------------------------------------------------------
// 16x16

HIGHBD_INTRA_PRED_TEST(
    C_1, TestHighbdIntraPred16, "Hbd Intra16x16",
    aom_highbd_dc_predictor_16x16_c, aom_highbd_dc_left_predictor_16x16_c,
    aom_highbd_dc_top_predictor_16x16_c, aom_highbd_dc_128_predictor_16x16_c,
    aom_highbd_v_predictor_16x16_c, aom_highbd_h_predictor_16x16_c,
    aom_highbd_paeth_predictor_16x16_c, aom_highbd_smooth_predictor_16x16_c,
    aom_highbd_smooth_v_predictor_16x16_c,
    aom_highbd_smooth_h_predictor_16x16_c)

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2_1, TestHighbdIntraPred16, "Hbd Intra16x16",
                       aom_highbd_dc_predictor_16x16_sse2,
                       aom_highbd_dc_left_predictor_16x16_sse2,
                       aom_highbd_dc_top_predictor_16x16_sse2,
                       aom_highbd_dc_128_predictor_16x16_sse2,
                       aom_highbd_v_predictor_16x16_sse2,
                       aom_highbd_h_predictor_16x16_sse2, NULL, NULL, NULL,
                       NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_2, TestHighbdIntraPred16, "Hbd Intra16x8",
                       aom_highbd_dc_predictor_16x8_sse2,
                       aom_highbd_dc_left_predictor_16x8_sse2,
                       aom_highbd_dc_top_predictor_16x8_sse2,
                       aom_highbd_dc_128_predictor_16x8_sse2,
                       aom_highbd_v_predictor_16x8_sse2,
                       aom_highbd_h_predictor_16x8_sse2, NULL, NULL, NULL, NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_3, TestHighbdIntraPred16, "Hbd Intra16x32",
                       aom_highbd_dc_predictor_16x32_sse2,
                       aom_highbd_dc_left_predictor_16x32_sse2,
                       aom_highbd_dc_top_predictor_16x32_sse2,
                       aom_highbd_dc_128_predictor_16x32_sse2,
                       aom_highbd_v_predictor_16x32_sse2,
                       aom_highbd_h_predictor_16x32_sse2, NULL, NULL, NULL,
                       NULL)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3_1, TestHighbdIntraPred16, "Hbd Intra16x16", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif

#if HAVE_AVX2
HIGHBD_INTRA_PRED_TEST(AVX2_1, TestHighbdIntraPred16, "Hbd Intra16x16", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)

HIGHBD_INTRA_PRED_TEST(AVX2_2, TestHighbdIntraPred16, "Hbd Intra16x8", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)

HIGHBD_INTRA_PRED_TEST(AVX2_3, TestHighbdIntraPred16, "Hbd Intra16x32", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif

HIGHBD_INTRA_PRED_TEST(
    C_2, TestHighbdIntraPred16, "Hbd Intra16x8", aom_highbd_dc_predictor_16x8_c,
    aom_highbd_dc_left_predictor_16x8_c, aom_highbd_dc_top_predictor_16x8_c,
    aom_highbd_dc_128_predictor_16x8_c, aom_highbd_v_predictor_16x8_c,
    aom_highbd_h_predictor_16x8_c, aom_highbd_paeth_predictor_16x8_c,
    aom_highbd_smooth_predictor_16x8_c, aom_highbd_smooth_v_predictor_16x8_c,
    aom_highbd_smooth_h_predictor_16x8_c)

HIGHBD_INTRA_PRED_TEST(
    C_3, TestHighbdIntraPred16, "Hbd Intra16x32",
    aom_highbd_dc_predictor_16x32_c, aom_highbd_dc_left_predictor_16x32_c,
    aom_highbd_dc_top_predictor_16x32_c, aom_highbd_dc_128_predictor_16x32_c,
    aom_highbd_v_predictor_16x32_c, aom_highbd_h_predictor_16x32_c,
    aom_highbd_paeth_predictor_16x32_c, aom_highbd_smooth_predictor_16x32_c,
    aom_highbd_smooth_v_predictor_16x32_c,
    aom_highbd_smooth_h_predictor_16x32_c)

// -----------------------------------------------------------------------------
// 32x32

HIGHBD_INTRA_PRED_TEST(
    C_1, TestHighbdIntraPred32, "Hbd Intra32x32",
    aom_highbd_dc_predictor_32x32_c, aom_highbd_dc_left_predictor_32x32_c,
    aom_highbd_dc_top_predictor_32x32_c, aom_highbd_dc_128_predictor_32x32_c,
    aom_highbd_v_predictor_32x32_c, aom_highbd_h_predictor_32x32_c,
    aom_highbd_paeth_predictor_32x32_c, aom_highbd_smooth_predictor_32x32_c,
    aom_highbd_smooth_v_predictor_32x32_c,
    aom_highbd_smooth_h_predictor_32x32_c)

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2_1, TestHighbdIntraPred32, "Hbd Intra32x32",
                       aom_highbd_dc_predictor_32x32_sse2,
                       aom_highbd_dc_left_predictor_32x32_sse2,
                       aom_highbd_dc_top_predictor_32x32_sse2,
                       aom_highbd_dc_128_predictor_32x32_sse2,
                       aom_highbd_v_predictor_32x32_sse2,
                       aom_highbd_h_predictor_32x32_sse2, NULL, NULL, NULL,
                       NULL)
HIGHBD_INTRA_PRED_TEST(SSE2_2, TestHighbdIntraPred32, "Hbd Intra32x16",
                       aom_highbd_dc_predictor_32x16_sse2,
                       aom_highbd_dc_left_predictor_32x16_sse2,
                       aom_highbd_dc_top_predictor_32x16_sse2,
                       aom_highbd_dc_128_predictor_32x16_sse2,
                       aom_highbd_v_predictor_32x16_sse2,
                       aom_highbd_h_predictor_32x16_sse2, NULL, NULL, NULL,
                       NULL)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3_1, TestHighbdIntraPred32, "Hbd Intra32x32", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif

#if HAVE_AVX2
HIGHBD_INTRA_PRED_TEST(AVX2_1, TestHighbdIntraPred32, "Hbd Intra32x32", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)

HIGHBD_INTRA_PRED_TEST(AVX2_2, TestHighbdIntraPred32, "Hbd Intra32x16", NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif

HIGHBD_INTRA_PRED_TEST(
    C_2, TestHighbdIntraPred32, "Hbd Intra32x16",
    aom_highbd_dc_predictor_32x16_c, aom_highbd_dc_left_predictor_32x16_c,
    aom_highbd_dc_top_predictor_32x16_c, aom_highbd_dc_128_predictor_32x16_c,
    aom_highbd_v_predictor_32x16_c, aom_highbd_h_predictor_32x16_c,
    aom_highbd_paeth_predictor_32x16_c, aom_highbd_smooth_predictor_32x16_c,
    aom_highbd_smooth_v_predictor_32x16_c,
    aom_highbd_smooth_h_predictor_32x16_c)

#include "test/test_libaom.cc"
