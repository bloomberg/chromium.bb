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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/av1_txfm_test.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/blockd.h"
#include "av1/common/scan.h"
#include "aom/aom_integer.h"
#include "aom_dsp/inv_txfm.h"

using libaom_test::ACMRandom;

namespace {

typedef void (*IdctFunc)(const tran_low_t *in, tran_low_t *out);

typedef std::tr1::tuple<IdctFunc, int, int> IdctParam;
class AV1InvTxfm : public ::testing::TestWithParam<IdctParam> {
 public:
  virtual void SetUp() {
    inv_txfm_ = GET_PARAM(0);
    txfm_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
  }

  void RunInvAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    for (int ti = 0; ti < count_test_block; ++ti) {
      tran_low_t input[64];
      double ref_input[64];
      for (int ni = 0; ni < txfm_size_; ++ni) {
        input[ni] = rnd.Rand8() - rnd.Rand8();
        ref_input[ni] = static_cast<double>(input[ni]);
      }

      tran_low_t output[64];
      inv_txfm_(input, output);

      double ref_output[64];
      libaom_test::reference_idct_1d(ref_input, ref_output, txfm_size_);

      for (int ni = 0; ni < txfm_size_; ++ni) {
        EXPECT_LE(
            abs(output[ni] - static_cast<tran_low_t>(round(ref_output[ni]))),
            max_error_);
      }
    }
  }

 private:
  double max_error_;
  int txfm_size_;
  IdctFunc inv_txfm_;
};

TEST_P(AV1InvTxfm, RunInvAccuracyCheck) { RunInvAccuracyCheck(); }

INSTANTIATE_TEST_CASE_P(C, AV1InvTxfm,
                        ::testing::Values(IdctParam(&aom_idct4_c, 4, 1),
                                          IdctParam(&aom_idct8_c, 8, 2),
                                          IdctParam(&aom_idct16_c, 16, 4),
                                          IdctParam(&aom_idct32_c, 32, 6)));

}  // namespace
