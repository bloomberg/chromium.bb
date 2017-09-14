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
const int kTotalPixels = 32 * kBPS;
const int kNumAv1IntraFuncs = INTRA_MODES + 3;  // 4 DC predictor variants.
const char *kAv1IntraPredNames[kNumAv1IntraFuncs] = {
  "DC_PRED",       "DC_LEFT_PRED",  "DC_TOP_PRED", "DC_128_PRED", "V_PRED",
  "H_PRED",        "D45_PRED",      "D135_PRED",   "D117_PRED",   "D153_PRED",
  "D207_PRED",     "D63_PRED",      "TM_PRED",     "SMOOTH_PRED",
#if CONFIG_SMOOTH_HV
  "SMOOTH_V_PRED", "SMOOTH_H_PRED",
#endif  // CONFIG_SMOOTH_HV
};

template <typename Pixel>
struct IntraPredTestMem {
  void Init(int block_size, int bd) {
    libaom_test::ACMRandom rnd(libaom_test::ACMRandom::DeterministicSeed());
    Pixel *const above = above_mem + 16;
    const int mask = (1 << bd) - 1;
    for (int i = 0; i < kTotalPixels; ++i) ref_src[i] = rnd.Rand16() & mask;
    for (int i = 0; i < kBPS; ++i) left[i] = rnd.Rand16() & mask;
    for (int i = -1; i < kBPS; ++i) above[i] = rnd.Rand16() & mask;

    ASSERT_LE(block_size, kBPS);
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

void CheckMd5Signature(const char name[], const char *const signatures[],
                       const void *data, size_t data_size, int elapsed_time,
                       int idx) {
  libaom_test::MD5 md5;
  md5.Add(reinterpret_cast<const uint8_t *>(data), data_size);
  printf("Mode %s[%12s]: %5d ms     MD5: %s\n", name, kAv1IntraPredNames[idx],
         elapsed_time, md5.Get());
  EXPECT_STREQ(signatures[idx], md5.Get());
}

void TestIntraPred(const char name[], AvxPredFunc const *pred_funcs,
                   const char *const signatures[], int block_size,
                   int num_pixels_per_test) {
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);
  Av1IntraPredTestMem intra_pred_test_mem;
  const uint8_t *const above = intra_pred_test_mem.above_mem + 16;

  intra_pred_test_mem.Init(block_size, 8);

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

void TestIntraPred4(AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "e7ed7353c3383fff942e500e9bfe82fe",
    "2a4a26fcc6ce005eadc08354d196c8a9",
    "269d92eff86f315d9c38fe7640d85b15",
    "ae2960eea9f71ee3dabe08b282ec1773",
    "6c1abcc44e90148998b51acd11144e9c",
    "f7bb3186e1ef8a2b326037ff898cad8e",
    "87e72798518d62e84bcc77dcb17d0f3b",
    "141624072a4a56773f68fadbdd07c4a7",
    "7be49b08687a5f24df3a2c612fca3876",
    "459bb5d9fd5b238348179c9a22108cd6",
    "3d98810f418a9de92acfe2c68909c61c",
    "6310eecda3cc9496987ca10186255558",
    "59fc0e923a08cfac0a493fb38988e2bb",
    "9ff8bb37d9c830e6ab8ecb0c435d3c91",
#if CONFIG_SMOOTH_HV
    "de6937fca02354f2874dbc5dbec5d5b3",
    "723cf948137f7d8c7860d814e55ae67d",
#endif  // CONFIG_SMOOTH_HV
  };
  TestIntraPred("Intra4", pred_funcs, kSignatures, 4,
                4 * 4 * kNumAv1IntraFuncs);
}

void TestIntraPred8(AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "d8bbae5d6547cfc17e4f5f44c8730e88",
    "373bab6d931868d41a601d9d88ce9ac3",
    "6fdd5ff4ff79656c14747598ca9e3706",
    "d9661c2811d6a73674f40ffb2b841847",
    "7c722d10b19ccff0b8c171868e747385",
    "f81dd986eb2b50f750d3a7da716b7e27",
    "e0b1292448f3350bf1c92ca283ca872a",
    "0e3523f9cab2142dd37fd07ec0760bce",
    "79ac4efe907f0a0f1885d43066cfedee",
    "19ecf2432ac305057de3b6578474eec6",
    "7ae38292cbe47b4aa0807c3bd5a543df",
    "d0ecffec1bb01f4b61ab5738164695c4",
    "064404361748dd111a890a1470d7f0ea",
    "dc29b7e1f78cc8e7525d5ea4c0ab9b78",
#if CONFIG_SMOOTH_HV
    "97111eb1bc26bade6272015df829f1ae",
    "d19a8a73cc46b807f2c5e817576cc1e1",
#endif  // CONFIG_SMOOTH_HV
  };
  TestIntraPred("Intra8", pred_funcs, kSignatures, 8,
                8 * 8 * kNumAv1IntraFuncs);
}

void TestIntraPred16(AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "50971c07ce26977d30298538fffec619",
    "527a6b9e0dc5b21b98cf276305432bef",
    "7eff2868f80ebc2c43a4f367281d80f7",
    "67cd60512b54964ef6aff1bd4816d922",
    "48371c87dc95c08a33b2048f89cf6468",
    "b0acf2872ee411d7530af6d2625a7084",
    "31d901ab2289d1e61e704e40240382a7",
    "dae208f3dca583529cff49b73f7c4183",
    "7af66a2f4c8e0b4908e40f047e60c47c",
    "125e3ab6ab9bc961f183ec366a7afa88",
    "ff230677e800977757d14b85a9eba404",
    "eb42dc39140515dd4f3ab1afe6c3e71b",
    "93d6b5352b571805ab16a55e1bbed86a",
    "03764e4c0aebbc180e4e2c68fb06df2b",
#if CONFIG_SMOOTH_HV
    "bb6c74c9076c9f266ab11fb57060d8e6",
    "0c5162bc28489756ddb847b5678e6f07",
#endif  // CONFIG_SMOOTH_HV
  };
  TestIntraPred("Intra16", pred_funcs, kSignatures, 16,
                16 * 16 * kNumAv1IntraFuncs);
}

void TestIntraPred32(AvxPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "a0a618c900e65ae521ccc8af789729f2",
    "985aaa7c72b4a6c2fb431d32100cf13a",
    "10662d09febc3ca13ee4e700120daeb5",
    "b3b01379ba08916ef6b1b35f7d9ad51c",
    "9f4261755795af97e34679c333ec7004",
    "bc2c9da91ad97ef0d1610fb0a9041657",
    "f524b1a7e31c7bb9bfb2487fac3e16d8",
    "4039bb7da0f6860090d3c57b5c85468f",
    "b29fff7b61804e68383e3a609b33da58",
    "e1aa5e49067fd8dba66c2eb8d07b7a89",
    "db217e7891581cf93895ef5974bebb21",
    "beb6cdc52b52c8976b4d2407ec8d2313",
    "ef1653982b69e1f64bee3759f3e1ec45",
    "1a51a675deba2c83282142eb48d3dc3d",
#if CONFIG_SMOOTH_HV
    "866c224746dc260cda861a7b1b383fb3",
    "cea23799fc3526e1b6a6ff02b42b82af",
#endif  // CONFIG_SMOOTH_HV
  };
  TestIntraPred("Intra32", pred_funcs, kSignatures, 32,
                32 * 32 * kNumAv1IntraFuncs);
}

}  // namespace

// Defines a test case for |arch| (e.g., C, SSE2, ...) passing the predictors
// to |test_func|. The test name is 'arch.test_func', e.g., C.TestIntraPred4.
#define INTRA_PRED_TEST(arch, test_func, dc, dc_left, dc_top, dc_128, v, h, \
                        d45e, d135, d117, d153, d207e, d63e, tm, smooth,    \
                        smooth_v, smooth_h)                                 \
  TEST(arch, test_func) {                                                   \
    static const AvxPredFunc aom_intra_pred[] = {                           \
      dc,   dc_left, dc_top, dc_128, v,  h,      d45e,     d135,            \
      d117, d153,    d207e,  d63e,   tm, smooth, smooth_v, smooth_h         \
    };                                                                      \
    test_func(aom_intra_pred);                                              \
  }

// -----------------------------------------------------------------------------
// 4x4

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_4x4_c
#define smooth_h_pred_func aom_smooth_h_predictor_4x4_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

INTRA_PRED_TEST(C, TestIntraPred4, aom_dc_predictor_4x4_c,
                aom_dc_left_predictor_4x4_c, aom_dc_top_predictor_4x4_c,
                aom_dc_128_predictor_4x4_c, aom_v_predictor_4x4_c,
                aom_h_predictor_4x4_c, aom_d45e_predictor_4x4_c,
                aom_d135_predictor_4x4_c, aom_d117_predictor_4x4_c,
                aom_d153_predictor_4x4_c, aom_d207e_predictor_4x4_c,
                aom_d63e_predictor_4x4_c, aom_paeth_predictor_4x4_c,
                aom_smooth_predictor_4x4_c, smooth_v_pred_func,
                smooth_h_pred_func)

#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TestIntraPred4, aom_dc_predictor_4x4_sse2,
                aom_dc_left_predictor_4x4_sse2, aom_dc_top_predictor_4x4_sse2,
                aom_dc_128_predictor_4x4_sse2, aom_v_predictor_4x4_sse2,
                aom_h_predictor_4x4_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred4, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, aom_d153_predictor_4x4_ssse3, NULL,
                aom_d63e_predictor_4x4_ssse3, NULL, NULL, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred4, aom_dc_predictor_4x4_dspr2, NULL, NULL,
                NULL, NULL, aom_h_predictor_4x4_dspr2, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred4, aom_dc_predictor_4x4_neon,
                aom_dc_left_predictor_4x4_neon, aom_dc_top_predictor_4x4_neon,
                aom_dc_128_predictor_4x4_neon, aom_v_predictor_4x4_neon,
                aom_h_predictor_4x4_neon, NULL, aom_d135_predictor_4x4_neon,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred4, aom_dc_predictor_4x4_msa,
                aom_dc_left_predictor_4x4_msa, aom_dc_top_predictor_4x4_msa,
                aom_dc_128_predictor_4x4_msa, aom_v_predictor_4x4_msa,
                aom_h_predictor_4x4_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 8x8

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_8x8_c
#define smooth_h_pred_func aom_smooth_h_predictor_8x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C, TestIntraPred8, aom_dc_predictor_8x8_c,
                aom_dc_left_predictor_8x8_c, aom_dc_top_predictor_8x8_c,
                aom_dc_128_predictor_8x8_c, aom_v_predictor_8x8_c,
                aom_h_predictor_8x8_c, aom_d45e_predictor_8x8_c,
                aom_d135_predictor_8x8_c, aom_d117_predictor_8x8_c,
                aom_d153_predictor_8x8_c, aom_d207e_predictor_8x8_c,
                aom_d63e_predictor_8x8_c, aom_paeth_predictor_8x8_c,
                aom_smooth_predictor_8x8_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TestIntraPred8, aom_dc_predictor_8x8_sse2,
                aom_dc_left_predictor_8x8_sse2, aom_dc_top_predictor_8x8_sse2,
                aom_dc_128_predictor_8x8_sse2, aom_v_predictor_8x8_sse2,
                aom_h_predictor_8x8_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred8, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, aom_d153_predictor_8x8_ssse3, NULL, NULL, NULL,
                NULL, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred8, aom_dc_predictor_8x8_dspr2, NULL, NULL,
                NULL, NULL, aom_h_predictor_8x8_dspr2, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred8, aom_dc_predictor_8x8_neon,
                aom_dc_left_predictor_8x8_neon, aom_dc_top_predictor_8x8_neon,
                aom_dc_128_predictor_8x8_neon, aom_v_predictor_8x8_neon,
                aom_h_predictor_8x8_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred8, aom_dc_predictor_8x8_msa,
                aom_dc_left_predictor_8x8_msa, aom_dc_top_predictor_8x8_msa,
                aom_dc_128_predictor_8x8_msa, aom_v_predictor_8x8_msa,
                aom_h_predictor_8x8_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 16x16

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_16x16_c
#define smooth_h_pred_func aom_smooth_h_predictor_16x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C, TestIntraPred16, aom_dc_predictor_16x16_c,
                aom_dc_left_predictor_16x16_c, aom_dc_top_predictor_16x16_c,
                aom_dc_128_predictor_16x16_c, aom_v_predictor_16x16_c,
                aom_h_predictor_16x16_c, aom_d45e_predictor_16x16_c,
                aom_d135_predictor_16x16_c, aom_d117_predictor_16x16_c,
                aom_d153_predictor_16x16_c, aom_d207e_predictor_16x16_c,
                aom_d63e_predictor_16x16_c, aom_paeth_predictor_16x16_c,
                aom_smooth_predictor_16x16_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TestIntraPred16, aom_dc_predictor_16x16_sse2,
                aom_dc_left_predictor_16x16_sse2,
                aom_dc_top_predictor_16x16_sse2,
                aom_dc_128_predictor_16x16_sse2, aom_v_predictor_16x16_sse2,
                aom_h_predictor_16x16_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred16, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, aom_d153_predictor_16x16_ssse3, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_DSPR2
INTRA_PRED_TEST(DSPR2, TestIntraPred16, aom_dc_predictor_16x16_dspr2, NULL,
                NULL, NULL, NULL, aom_h_predictor_16x16_dspr2, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#endif  // HAVE_DSPR2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred16, aom_dc_predictor_16x16_neon,
                aom_dc_left_predictor_16x16_neon,
                aom_dc_top_predictor_16x16_neon,
                aom_dc_128_predictor_16x16_neon, aom_v_predictor_16x16_neon,
                aom_h_predictor_16x16_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred16, aom_dc_predictor_16x16_msa,
                aom_dc_left_predictor_16x16_msa, aom_dc_top_predictor_16x16_msa,
                aom_dc_128_predictor_16x16_msa, aom_v_predictor_16x16_msa,
                aom_h_predictor_16x16_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// 32x32

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_smooth_v_predictor_32x32_c
#define smooth_h_pred_func aom_smooth_h_predictor_32x32_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV
INTRA_PRED_TEST(C, TestIntraPred32, aom_dc_predictor_32x32_c,
                aom_dc_left_predictor_32x32_c, aom_dc_top_predictor_32x32_c,
                aom_dc_128_predictor_32x32_c, aom_v_predictor_32x32_c,
                aom_h_predictor_32x32_c, aom_d45e_predictor_32x32_c,
                aom_d135_predictor_32x32_c, aom_d117_predictor_32x32_c,
                aom_d153_predictor_32x32_c, aom_d207e_predictor_32x32_c,
                aom_d63e_predictor_32x32_c, aom_paeth_predictor_32x32_c,
                aom_smooth_predictor_32x32_c, smooth_v_pred_func,
                smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TestIntraPred32, aom_dc_predictor_32x32_sse2,
                aom_dc_left_predictor_32x32_sse2,
                aom_dc_top_predictor_32x32_sse2,
                aom_dc_128_predictor_32x32_sse2, aom_v_predictor_32x32_sse2,
                aom_h_predictor_32x32_sse2, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TestIntraPred32, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, aom_d153_predictor_32x32_ssse3, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_SSSE3

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TestIntraPred32, aom_dc_predictor_32x32_neon,
                aom_dc_left_predictor_32x32_neon,
                aom_dc_top_predictor_32x32_neon,
                aom_dc_128_predictor_32x32_neon, aom_v_predictor_32x32_neon,
                aom_h_predictor_32x32_neon, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_NEON

#if HAVE_MSA
INTRA_PRED_TEST(MSA, TestIntraPred32, aom_dc_predictor_32x32_msa,
                aom_dc_left_predictor_32x32_msa, aom_dc_top_predictor_32x32_msa,
                aom_dc_128_predictor_32x32_msa, aom_v_predictor_32x32_msa,
                aom_h_predictor_32x32_msa, NULL, NULL, NULL, NULL, NULL, NULL,
                NULL, NULL, NULL, NULL)
#endif  // HAVE_MSA

// -----------------------------------------------------------------------------
// High Bitdepth
#if CONFIG_HIGHBITDEPTH
namespace {

typedef void (*AvxHighbdPredFunc)(uint16_t *dst, ptrdiff_t y_stride,
                                  const uint16_t *above, const uint16_t *left,
                                  int bd);

typedef IntraPredTestMem<uint16_t> Av1HighbdIntraPredTestMem;

void TestHighbdIntraPred(const char name[], AvxHighbdPredFunc const *pred_funcs,
                         const char *const signatures[], int block_size,
                         int num_pixels_per_test) {
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);
  Av1HighbdIntraPredTestMem intra_pred_test_mem;
  const uint16_t *const above = intra_pred_test_mem.above_mem + 16;
  const int bd = 12;

  intra_pred_test_mem.Init(block_size, bd);

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

void TestHighbdIntraPred4(AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "11f74af6c5737df472f3275cbde062fa",
    "51bea056b6447c93f6eb8f6b7e8f6f71",
    "27e97f946766331795886f4de04c5594",
    "53ab15974b049111fb596c5168ec7e3f",
    "f0b640bb176fbe4584cf3d32a9b0320a",
    "729783ca909e03afd4b47111c80d967b",
    "d631a8544ccc87702db3e98fac494657",
    "293fc903254a33754133314c6cdba81f",
    "f8074d704233e73dfd35b458c6092374",
    "aa6363d08544a1ec4da33d7a0be5640d",
    "0bdc21a3acdebc393bc2c22e71bbeada",
    "a48f7a484ba4ad3916055c7160665b56",
    "6e30009c45474a22032678b1bd579c8f",
    "e57cba016d808aa8a35619df2a65f049",
#if CONFIG_SMOOTH_HV
    "55a6c37f39afcbbf5abca4a985b96459",
    "a623d45b37dafec1f8a75c4c5218913d",
#endif  // CONFIG_SMOOTH_HV
  };
  TestHighbdIntraPred("Hbd Intra4", pred_funcs, kSignatures, 4,
                      4 * 4 * kNumAv1IntraFuncs);
}

void TestHighbdIntraPred8(AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "03da8829fe94663047fd108c5fcaa71d",
    "ecdb37b8120a2d3a4c706b016bd1bfd7",
    "1d4543ed8d2b9368cb96898095fe8a75",
    "f791c9a67b913cbd82d9da8ecede30e2",
    "065c70646f4dbaff913282f55a45a441",
    "51f87123616662ef7c35691497dfd0ba",
    "4f53cf8e5f43894dc0759f43c7081f60",
    "9ffe186a6bc7db95275f1bbddd6f7aba",
    "a3258a2eae2e2bd55cb8f71351b22998",
    "8d909f0a2066e39b3216092c6289ece4",
    "6751f60655aba44aff78aaaf4e967377",
    "d31a449872fab968a8d41de578338780",
    "85c01ba03df68f9ece7bd3fa0f8980e6",
    "ad19b7dac092f56df6d054e1f67f21e7",
#if CONFIG_SMOOTH_HV
    "0edc415b5dd7299f7a34fb9f71d31d78",
    "2bc8ec19e9f4b77a64b8a0a1f6aec7e7",
#endif  // CONFIG_SMOOTH_HV
  };
  TestHighbdIntraPred("Hbd Intra8", pred_funcs, kSignatures, 8,
                      8 * 8 * kNumAv1IntraFuncs);
}

void TestHighbdIntraPred16(AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "e33cb3f56a878e2fddb1b2fc51cdd275",
    "c7bff6f04b6052c8ab335d726dbbd52d",
    "d0b0b47b654a9bcc5c6008110a44589b",
    "78f5da7b10b2b9ab39f114a33b6254e9",
    "c78e31d23831abb40d6271a318fdd6f3",
    "90d1347f4ec9198a0320daecb6ff90b8",
    "e38e12830e2ee5a01a064ec5998d5948",
    "cf28bd387b81ad3e5f1a1c779a4b70a0",
    "24c304330431ddeaf630f6ce94af2eac",
    "91a329798036bf64e8e00a87b131b8b1",
    "e536338d1a8ee192b9e591855db1a222",
    "54ecd47737f71c62d24e3779585113f2",
    "e63ded54ab3d0e8728b6f24d4f01e53f",
    "35ce21fbe0ea114c089fc3489a78155d",
#if CONFIG_SMOOTH_HV
    "f277f6ef8e4d717f1f0dfe2706ac197d",
    "e8014d3f41256976c02e0f1e622ba2b9",
#endif  // CONFIG_SMOOTH_HV
  };
  TestHighbdIntraPred("Hbd Intra16", pred_funcs, kSignatures, 16,
                      16 * 16 * kNumAv1IntraFuncs);
}

void TestHighbdIntraPred32(AvxHighbdPredFunc const *pred_funcs) {
  static const char *const kSignatures[kNumAv1IntraFuncs] = {
    "a3e8056ba7e36628cce4917cd956fedd",
    "cc7d3024fe8748b512407edee045377e",
    "2aab0a0f330a1d3e19b8ecb8f06387a3",
    "a547bc3fb7b06910bf3973122a426661",
    "26f712514da95042f93d6e8dc8e431dc",
    "bb08c6e16177081daa3d936538dbc2e3",
    "4e10f10b082a5b4265080c102d34eb47",
    "42867c8553285e94ee8e4df7abafbda8",
    "6496bdee96100667833f546e1be3d640",
    "2ebfa25bf981377e682e580208504300",
    "1788695b10a6f82ae1a56686dcbcd0a9",
    "c3b9c506604a7132bbb5f4e97bdb03f0",
    "84bf83f94a51b33654ca940c6f8bc057",
    "7168b03fc31bf29596a344d6a35d007c",
#if CONFIG_SMOOTH_HV
    "b073a70d3672f1282236994f5d12e94b",
    "c51607aebad5dcb3c1e3b58ef9e5b84e",
#endif  // CONFIG_SMOOTH_HV
  };
  TestHighbdIntraPred("Hbd Intra32", pred_funcs, kSignatures, 32,
                      32 * 32 * kNumAv1IntraFuncs);
}

}  // namespace

#define HIGHBD_INTRA_PRED_TEST(arch, test_func, dc, dc_left, dc_top, dc_128,  \
                               v, h, d45e, d135, d117, d153, d207e, d63e, tm, \
                               smooth, smooth_v, smooth_h)                    \
  TEST(arch, test_func) {                                                     \
    static const AvxHighbdPredFunc aom_intra_pred[] = {                       \
      dc,   dc_left, dc_top, dc_128, v,  h,      d45e,     d135,              \
      d117, d153,    d207e,  d63e,   tm, smooth, smooth_v, smooth_h           \
    };                                                                        \
    test_func(aom_intra_pred);                                                \
  }

// -----------------------------------------------------------------------------
// 4x4

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_4x4_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_4x4_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C, TestHighbdIntraPred4, aom_highbd_dc_predictor_4x4_c,
    aom_highbd_dc_left_predictor_4x4_c, aom_highbd_dc_top_predictor_4x4_c,
    aom_highbd_dc_128_predictor_4x4_c, aom_highbd_v_predictor_4x4_c,
    aom_highbd_h_predictor_4x4_c, aom_highbd_d45e_predictor_4x4_c,
    aom_highbd_d135_predictor_4x4_c, aom_highbd_d117_predictor_4x4_c,
    aom_highbd_d153_predictor_4x4_c, aom_highbd_d207e_predictor_4x4_c,
    aom_highbd_d63e_predictor_4x4_c, aom_highbd_paeth_predictor_4x4_c,
    aom_highbd_smooth_predictor_4x4_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2, TestHighbdIntraPred4, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL)
#endif

// -----------------------------------------------------------------------------
// 8x8

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_8x8_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_8x8_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C, TestHighbdIntraPred8, aom_highbd_dc_predictor_8x8_c,
    aom_highbd_dc_left_predictor_8x8_c, aom_highbd_dc_top_predictor_8x8_c,
    aom_highbd_dc_128_predictor_8x8_c, aom_highbd_v_predictor_8x8_c,
    aom_highbd_h_predictor_8x8_c, aom_highbd_d45e_predictor_8x8_c,
    aom_highbd_d135_predictor_8x8_c, aom_highbd_d117_predictor_8x8_c,
    aom_highbd_d153_predictor_8x8_c, aom_highbd_d207e_predictor_8x8_c,
    aom_highbd_d63e_predictor_8x8_c, aom_highbd_paeth_predictor_8x8_c,
    aom_highbd_smooth_predictor_8x8_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2, TestHighbdIntraPred8, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL)
#endif

// -----------------------------------------------------------------------------
// 16x16

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_16x16_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_16x16_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C, TestHighbdIntraPred16, aom_highbd_dc_predictor_16x16_c,
    aom_highbd_dc_left_predictor_16x16_c, aom_highbd_dc_top_predictor_16x16_c,
    aom_highbd_dc_128_predictor_16x16_c, aom_highbd_v_predictor_16x16_c,
    aom_highbd_h_predictor_16x16_c, aom_highbd_d45e_predictor_16x16_c,
    aom_highbd_d135_predictor_16x16_c, aom_highbd_d117_predictor_16x16_c,
    aom_highbd_d153_predictor_16x16_c, aom_highbd_d207e_predictor_16x16_c,
    aom_highbd_d63e_predictor_16x16_c, aom_highbd_paeth_predictor_16x16_c,
    aom_highbd_smooth_predictor_16x16_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2, TestHighbdIntraPred16, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL)
#endif

// -----------------------------------------------------------------------------
// 32x32

#if CONFIG_SMOOTH_HV
#define smooth_v_pred_func aom_highbd_smooth_v_predictor_32x32_c
#define smooth_h_pred_func aom_highbd_smooth_h_predictor_32x32_c
#else
#define smooth_v_pred_func NULL
#define smooth_h_pred_func NULL
#endif  // CONFIG_SMOOTH_HV

HIGHBD_INTRA_PRED_TEST(
    C, TestHighbdIntraPred32, aom_highbd_dc_predictor_32x32_c,
    aom_highbd_dc_left_predictor_32x32_c, aom_highbd_dc_top_predictor_32x32_c,
    aom_highbd_dc_128_predictor_32x32_c, aom_highbd_v_predictor_32x32_c,
    aom_highbd_h_predictor_32x32_c, aom_highbd_d45e_predictor_32x32_c,
    aom_highbd_d135_predictor_32x32_c, aom_highbd_d117_predictor_32x32_c,
    aom_highbd_d153_predictor_32x32_c, aom_highbd_d207e_predictor_32x32_c,
    aom_highbd_d63e_predictor_32x32_c, aom_highbd_paeth_predictor_32x32_c,
    aom_highbd_smooth_predictor_32x32_c, smooth_v_pred_func, smooth_h_pred_func)
#undef smooth_v_pred_func
#undef smooth_h_pred_func

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2, TestHighbdIntraPred32, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL)
#endif
#endif  // CONFIG_HIGHBITDEPTH

#include "test/test_libaom.cc"
