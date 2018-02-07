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
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "./av1_rtcd.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "test/av1_txfm_test.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"

using libaom_test::ACMRandom;
using libaom_test::Fwd_Txfm2d_Func;
using libaom_test::Inv_Txfm2d_Func;
using libaom_test::bd;
using libaom_test::compute_avg_abs_error;
using libaom_test::input_base;

using std::vector;

namespace {

// AV1InvTxfm2dParam argument list:
// tx_type_, tx_size_, max_error_, max_avg_error_
typedef std::tr1::tuple<TX_TYPE, TX_SIZE, int, double> AV1InvTxfm2dParam;

class AV1InvTxfm2d : public ::testing::TestWithParam<AV1InvTxfm2dParam> {
 public:
  virtual void SetUp() {
    tx_type_ = GET_PARAM(0);
    tx_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
    max_avg_error_ = GET_PARAM(3);
  }

  void RunRoundtripCheck() {
    int tx_w = tx_size_wide[tx_size_];
    int tx_h = tx_size_high[tx_size_];
    int txfm2d_size = tx_w * tx_h;
    const Fwd_Txfm2d_Func fwd_txfm_func =
        libaom_test::fwd_txfm_func_ls[tx_size_];
    const Inv_Txfm2d_Func inv_txfm_func =
        libaom_test::inv_txfm_func_ls[tx_size_];
    double avg_abs_error = 0;
    ACMRandom rnd(ACMRandom::DeterministicSeed());

    const int count = 500;

    for (int ci = 0; ci < count; ci++) {
      DECLARE_ALIGNED(16, int16_t, input[64 * 64]) = { 0 };
      ASSERT_LE(txfm2d_size, NELEMENTS(input));

      for (int ni = 0; ni < txfm2d_size; ++ni) {
        if (ci == 0) {
          int extreme_input = input_base - 1;
          input[ni] = extreme_input;  // extreme case
        } else {
          input[ni] = rnd.Rand16() % input_base;
        }
      }

      DECLARE_ALIGNED(16, uint16_t, expected[64 * 64]) = { 0 };
      ASSERT_LE(txfm2d_size, NELEMENTS(expected));
      if (TxfmUsesApproximation()) {
        // Compare reference forward HT + inverse HT vs forward HT + inverse HT.
        double ref_input[64 * 64];
        ASSERT_LE(txfm2d_size, NELEMENTS(ref_input));
        for (int ni = 0; ni < txfm2d_size; ++ni) {
          ref_input[ni] = input[ni];
        }
        double ref_coeffs[64 * 64] = { 0 };
        ASSERT_LE(txfm2d_size, NELEMENTS(ref_coeffs));
        ASSERT_EQ(tx_type_, DCT_DCT);
        libaom_test::reference_hybrid_2d(ref_input, ref_coeffs, tx_type_,
                                         tx_size_);
        DECLARE_ALIGNED(16, int32_t, ref_coeffs_int[64 * 64]) = { 0 };
        ASSERT_LE(txfm2d_size, NELEMENTS(ref_coeffs_int));
        for (int ni = 0; ni < txfm2d_size; ++ni) {
          ref_coeffs_int[ni] = (int32_t)round(ref_coeffs[ni]);
        }
        inv_txfm_func(ref_coeffs_int, expected, tx_w, tx_type_, bd);
      } else {
        // Compare original input vs forward HT + inverse HT.
        for (int ni = 0; ni < txfm2d_size; ++ni) {
          expected[ni] = input[ni];
        }
      }

      DECLARE_ALIGNED(16, int32_t, coeffs[64 * 64]) = { 0 };
      ASSERT_LE(txfm2d_size, NELEMENTS(coeffs));
      fwd_txfm_func(input, coeffs, tx_w, tx_type_, bd);

      DECLARE_ALIGNED(16, uint16_t, actual[64 * 64]) = { 0 };
      ASSERT_LE(txfm2d_size, NELEMENTS(actual));
      inv_txfm_func(coeffs, actual, tx_w, tx_type_, bd);

      double actual_max_error = 0;
      for (int ni = 0; ni < txfm2d_size; ++ni) {
        const double this_error = abs(expected[ni] - actual[ni]);
        actual_max_error = AOMMAX(actual_max_error, this_error);
      }
      EXPECT_GE(max_error_, actual_max_error)
          << " tx_w: " << tx_w << " tx_h " << tx_h << " tx_type: " << tx_type_;
      if (actual_max_error > max_error_) {  // exit early.
        break;
      }
      avg_abs_error += compute_avg_abs_error<uint16_t, uint16_t>(
          expected, actual, txfm2d_size);
    }

    avg_abs_error /= count;
    EXPECT_GE(max_avg_error_, avg_abs_error)
        << " tx_w: " << tx_w << " tx_h " << tx_h << " tx_type: " << tx_type_;
  }

 private:
  bool TxfmUsesApproximation() {
#if CONFIG_TX64X64
    if (tx_size_wide[tx_size_] == 64 || tx_size_high[tx_size_] == 64) {
      return true;
    }
#endif  // CONFIG_TX64X64
    return false;
  }

  int max_error_;
  double max_avg_error_;
  TX_TYPE tx_type_;
  TX_SIZE tx_size_;
};

vector<AV1InvTxfm2dParam> GetInvTxfm2dParamList() {
  vector<AV1InvTxfm2dParam> param_list;
  for (int t = 0; t < TX_TYPES; ++t) {
    const TX_TYPE tx_type = static_cast<TX_TYPE>(t);
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_4X4, 2, 0.002));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_8X8, 2, 0.05));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_16X16, 2, 0.04));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_32X32, 4, 0.4));
#if CONFIG_TX64X64
    if (tx_type == DCT_DCT) {  // Other types not supported by these tx sizes.
      param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_64X64, 3, 0.3));
    }
#endif  // CONFIG_TX64X64

    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_4X8, 2, 0.09));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_8X4, 2, 0.11));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_8X16, 2, 0.03));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_16X8, 2, 0.07));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_16X32, 3, 0.4));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_32X16, 3, 0.5));

    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_4X16, 2, 0.2));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_16X4, 2, 0.2));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_8X32, 2, 0.2));
    param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_32X8, 2, 0.2));

#if CONFIG_TX64X64
    if (tx_type == DCT_DCT) {  // Other types not supported by these tx sizes.
      param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_32X64, 5, 0.38));
      param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_64X32, 5, 0.39));
      param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_16X64, 3, 0.38));
      param_list.push_back(AV1InvTxfm2dParam(tx_type, TX_64X16, 3, 0.38));
    }
#endif  // CONFIG_TX64X64
  }
  return param_list;
}

INSTANTIATE_TEST_CASE_P(C, AV1InvTxfm2d,
                        ::testing::ValuesIn(GetInvTxfm2dParamList()));

TEST_P(AV1InvTxfm2d, RunRoundtripCheck) { RunRoundtripCheck(); }

TEST(AV1InvTxfm2d, CfgTest) {
  for (int bd_idx = 0; bd_idx < BD_NUM; ++bd_idx) {
    int bd = libaom_test::bd_arr[bd_idx];
    int8_t low_range = libaom_test::low_range_arr[bd_idx];
    int8_t high_range = libaom_test::high_range_arr[bd_idx];
    for (int tx_size = 0; tx_size < TX_SIZES_ALL; ++tx_size) {
      for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
#if CONFIG_TX64X64
        if ((tx_size_wide[tx_size] == 64 || tx_size_high[tx_size] == 64) &&
            tx_type != DCT_DCT) {
          continue;
        }
#endif  // CONFIG_TX64X64
        TXFM_2D_FLIP_CFG cfg;
        av1_get_inv_txfm_cfg(static_cast<TX_TYPE>(tx_type),
                             static_cast<TX_SIZE>(tx_size), &cfg);
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        av1_gen_inv_stage_range(stage_range_col, stage_range_row, &cfg,
                                (TX_SIZE)tx_size, bd);
        libaom_test::txfm_stage_range_check(stage_range_col, cfg.stage_num_col,
                                            cfg.cos_bit_col, low_range,
                                            high_range);
        libaom_test::txfm_stage_range_check(stage_range_row, cfg.stage_num_row,
                                            cfg.cos_bit_row, low_range,
                                            high_range);
      }
    }
  }
}
}  // namespace
