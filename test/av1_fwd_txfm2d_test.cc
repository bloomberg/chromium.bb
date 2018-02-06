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

#include "test/acm_random.h"
#include "test/util.h"
#include "test/av1_txfm_test.h"
#include "av1/common/av1_txfm.h"
#include "./av1_rtcd.h"

using libaom_test::ACMRandom;
using libaom_test::Fwd_Txfm2d_Func;
using libaom_test::TYPE_TXFM;
using libaom_test::bd;
using libaom_test::compute_avg_abs_error;
using libaom_test::input_base;

using std::vector;

namespace {
// tx_type_, tx_size_, max_error_, max_avg_error_
typedef std::tr1::tuple<TX_TYPE, TX_SIZE, double, double> AV1FwdTxfm2dParam;

class AV1FwdTxfm2d : public ::testing::TestWithParam<AV1FwdTxfm2dParam> {
 public:
  virtual void SetUp() {
    tx_type_ = GET_PARAM(0);
    tx_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
    max_avg_error_ = GET_PARAM(3);
    count_ = 500;
    TXFM_2D_FLIP_CFG fwd_txfm_flip_cfg;
    av1_get_fwd_txfm_cfg(tx_type_, tx_size_, &fwd_txfm_flip_cfg);
    amplify_factor_ = libaom_test::get_amplification_factor(tx_type_, tx_size_);
    tx_width_ = tx_size_wide[fwd_txfm_flip_cfg.tx_size];
    tx_height_ = tx_size_high[fwd_txfm_flip_cfg.tx_size];
    ud_flip_ = fwd_txfm_flip_cfg.ud_flip;
    lr_flip_ = fwd_txfm_flip_cfg.lr_flip;

    fwd_txfm_ = libaom_test::fwd_txfm_func_ls[tx_size_];
    txfm2d_size_ = tx_width_ * tx_height_;
    input_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(input_[0]) * txfm2d_size_));
    output_ = reinterpret_cast<int32_t *>(
        aom_memalign(16, sizeof(output_[0]) * txfm2d_size_));
    ref_input_ = reinterpret_cast<double *>(
        aom_memalign(16, sizeof(ref_input_[0]) * txfm2d_size_));
    ref_output_ = reinterpret_cast<double *>(
        aom_memalign(16, sizeof(ref_output_[0]) * txfm2d_size_));
  }

  void RunFwdAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    double avg_abs_error = 0;
    for (int ci = 0; ci < count_; ci++) {
      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        input_[ni] = rnd.Rand16() % input_base;
        ref_input_[ni] = static_cast<double>(input_[ni]);
        output_[ni] = 0;
        ref_output_[ni] = 0;
      }

      fwd_txfm_(input_, output_, tx_width_, tx_type_, bd);

      if (lr_flip_ && ud_flip_) {
        libaom_test::fliplrud(ref_input_, tx_width_, tx_height_, tx_width_);
      } else if (lr_flip_) {
        libaom_test::fliplr(ref_input_, tx_width_, tx_height_, tx_width_);
      } else if (ud_flip_) {
        libaom_test::flipud(ref_input_, tx_width_, tx_height_, tx_width_);
      }

      libaom_test::reference_hybrid_2d(ref_input_, ref_output_, tx_type_,
                                       tx_size_);

      double actual_max_error = 0;
      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        ref_output_[ni] = round(ref_output_[ni]);
        const double this_error =
            fabs(output_[ni] - ref_output_[ni]) / amplify_factor_;
        actual_max_error = AOMMAX(actual_max_error, this_error);
      }
      EXPECT_GE(max_error_, actual_max_error)
          << "tx_size = " << tx_size_ << ", tx_type = " << tx_type_;
      if (actual_max_error > max_error_) {  // exit early.
        break;
      }

      avg_abs_error += compute_avg_abs_error<int32_t, double>(
          output_, ref_output_, txfm2d_size_);
    }

    avg_abs_error /= amplify_factor_;
    avg_abs_error /= count_;
    EXPECT_GE(max_avg_error_, avg_abs_error)
        << "tx_size = " << tx_size_ << ", tx_type = " << tx_type_;
  }

  virtual void TearDown() {
    aom_free(input_);
    aom_free(output_);
    aom_free(ref_input_);
    aom_free(ref_output_);
  }

 private:
  double max_error_;
  double max_avg_error_;
  int count_;
  double amplify_factor_;
  TX_TYPE tx_type_;
  TX_SIZE tx_size_;
  int tx_width_;
  int tx_height_;
  int txfm2d_size_;
  Fwd_Txfm2d_Func fwd_txfm_;
  int16_t *input_;
  int32_t *output_;
  double *ref_input_;
  double *ref_output_;
  int ud_flip_;  // flip upside down
  int lr_flip_;  // flip left to right
};

vector<AV1FwdTxfm2dParam> GetTxfm2dParamList() {
  vector<AV1FwdTxfm2dParam> param_list;
  for (int t = 0; t < TX_TYPES; ++t) {
    const TX_TYPE tx_type = static_cast<TX_TYPE>(t);
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_4X4, 3, 0.5));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_8X8, 5, 0.5));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_16X16, 11, 1.2));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_32X32, 70, 6.1));
#if CONFIG_TX64X64
    if (tx_type == DCT_DCT) {  // Other types not supported by these tx sizes.
      param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_64X64, 64, 3.4));
    }
#endif  // CONFIG_TX64X64

    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_4X8, 3.2, 0.52));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_8X4, 3.6, 0.64));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_8X16, 8, 0.8));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_16X8, 8, 1.1));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_16X32, 29, 3.9));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_32X16, 37, 5.9));

    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_4X16, 5, 0.6));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_16X4, 6, 0.9));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_8X32, 21, 1.2));
    param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_32X8, 13, 1.7));

#if CONFIG_TX64X64
    if (tx_type == DCT_DCT) {  // Other types not supported by these tx sizes.
      param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_32X64, 136, 2.9));
      param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_64X32, 136, 4.9));
      param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_16X64, 30, 2.0));
      param_list.push_back(AV1FwdTxfm2dParam(tx_type, TX_64X16, 36, 4.7));
    }
#endif  // CONFIG_TX64X64
  }
  return param_list;
}

INSTANTIATE_TEST_CASE_P(C, AV1FwdTxfm2d,
                        ::testing::ValuesIn(GetTxfm2dParamList()));

TEST_P(AV1FwdTxfm2d, RunFwdAccuracyCheck) { RunFwdAccuracyCheck(); }

TEST(AV1FwdTxfm2d, CfgTest) {
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
        av1_get_fwd_txfm_cfg(static_cast<TX_TYPE>(tx_type),
                             static_cast<TX_SIZE>(tx_size), &cfg);
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        av1_gen_fwd_stage_range(stage_range_col, stage_range_row, &cfg, bd);
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

#if HAVE_SSE2 && defined(__SSE2__)
#include "av1/common/x86/av1_txfm_sse2.h"
Fwd_Txfm2d_Func fwd_func_sse2_list[TX_SIZES_ALL][2] = {
  { NULL, NULL },  // TX_4X4
  { av1_fwd_txfm2d_8x8_c,
    av1_fwd_txfm2d_8x8_sse2 },  // TX_8X8    // 8x8 transform
  { av1_fwd_txfm2d_16x16_c, av1_fwd_txfm2d_16x16_sse2 },  // TX_16X16
  { NULL, NULL },                                         // TX_32X32
#if CONFIG_TX64X64
  { NULL, NULL },  // TX_64X64
#endif             // CONFIG_TX64X64
  { NULL, NULL },  // TX_4X8
  { NULL, NULL },  // TX_8X4
  { NULL, NULL },  // TX_8X16
  { NULL, NULL },  // TX_16X8
  { NULL, NULL },  // TX_16X32
  { NULL, NULL },  // TX_32X16
#if CONFIG_TX64X64
  { NULL, NULL },  // TX_32X64
  { NULL, NULL },  // TX_64X32
#endif             // CONFIG_TX64X64
  { NULL, NULL },  // TX_4X16
  { NULL, NULL },  // TX_16X4
  { NULL, NULL },  // TX_8X32
  { NULL, NULL },  // TX_32X8
#if CONFIG_TX64X64
  { NULL, NULL },  // TX_16X64
  { NULL, NULL },  // TX_64X16
#endif             // CONFIG_TX64X64
};

TEST(av1_fwd_txfm2d_sse2, match) {
  const int bd = 8;
  for (int tx_size = TX_4X4; tx_size < TX_SIZES_ALL; ++tx_size) {
    for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
      Fwd_Txfm2d_Func ref_func = fwd_func_sse2_list[tx_size][0];
      Fwd_Txfm2d_Func target_func = fwd_func_sse2_list[tx_size][1];
      if (ref_func != NULL && target_func != NULL) {
        int16_t input[64 * 64] = { 0 };
        int32_t output[64 * 64] = { 0 };
        int32_t ref_output[64 * 64] = { 0 };
        int input_stride = 64;
        ACMRandom rnd(ACMRandom::DeterministicSeed());
        int rows = tx_size_high[tx_size];
        int cols = tx_size_wide[tx_size];
        for (int cnt = 0; cnt < 500; ++cnt) {
          if (cnt == 0) {
            for (int r = 0; r < rows; ++r) {
              for (int c = 0; c < cols; ++c) {
                input[r * input_stride + c] = (1 << bd) - 1;
              }
            }
          } else {
            for (int r = 0; r < rows; ++r) {
              for (int c = 0; c < cols; ++c) {
                input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
              }
            }
          }
          ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
          target_func(input, output, input_stride, (TX_TYPE)tx_type, bd);
          for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
              ASSERT_EQ(ref_output[r * cols + c], output[r * cols + c])
                  << "[" << r << "," << c << "]";
            }
          }
        }
      }
    }
  }
}
#endif  // HAVE_SSE2

}  // namespace
