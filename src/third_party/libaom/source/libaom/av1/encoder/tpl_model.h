/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_TPL_MODEL_H_
#define AOM_AV1_ENCODER_TPL_MODEL_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!\cond */

struct AV1_PRIMARY;
struct AV1_COMP;
struct AV1_SEQ_CODING_TOOLS;
struct EncodeFrameParams;
struct EncodeFrameInput;

#include "config/aom_config.h"

#include "aom_scale/yv12config.h"

#include "av1/common/mv.h"
#include "av1/common/scale.h"
#include "av1/encoder/block.h"
#include "av1/encoder/lookahead.h"

static INLINE BLOCK_SIZE convert_length_to_bsize(int length) {
  switch (length) {
    case 64: return BLOCK_64X64;
    case 32: return BLOCK_32X32;
    case 16: return BLOCK_16X16;
    case 8: return BLOCK_8X8;
    case 4: return BLOCK_4X4;
    default:
      assert(0 && "Invalid block size for tpl model");
      return BLOCK_16X16;
  }
}

typedef struct AV1TplRowMultiThreadSync {
#if CONFIG_MULTITHREAD
  // Synchronization objects for top-right dependency.
  pthread_mutex_t *mutex_;
  pthread_cond_t *cond_;
#endif
  // Buffer to store the macroblock whose encoding is complete.
  // num_finished_cols[i] stores the number of macroblocks which finished
  // encoding in the ith macroblock row.
  int *num_finished_cols;
  // Number of extra macroblocks of the top row to be complete for encoding
  // of the current macroblock to start. A value of 1 indicates top-right
  // dependency.
  int sync_range;
  // Number of macroblock rows.
  int rows;
  // Number of threads processing the current tile.
  int num_threads_working;
} AV1TplRowMultiThreadSync;

typedef struct AV1TplRowMultiThreadInfo {
  // Row synchronization related function pointers.
  void (*sync_read_ptr)(AV1TplRowMultiThreadSync *tpl_mt_sync, int r, int c);
  void (*sync_write_ptr)(AV1TplRowMultiThreadSync *tpl_mt_sync, int r, int c,
                         int cols);
} AV1TplRowMultiThreadInfo;

// TODO(jingning): This needs to be cleaned up next.

// TPL stats buffers are prepared for every frame in the GOP,
// including (internal) overlays and (internal) arfs.
// In addition, frames in the lookahead that are outside of the GOP
// are also used.
// Thus it should use
// (gop_length) + (# overlays) + (MAX_LAG_BUFFERS - gop_len) =
// MAX_LAG_BUFFERS + (# overlays)
// 2 * MAX_LAG_BUFFERS is therefore a safe estimate.
// TODO(bohanli): test setting it to 1.5 * MAX_LAG_BUFFER
#define MAX_TPL_FRAME_IDX (2 * MAX_LAG_BUFFERS)
// The first REF_FRAMES + 1 buffers are reserved.
// tpl_data->tpl_frame starts after REF_FRAMES + 1
#define MAX_LENGTH_TPL_FRAME_STATS (MAX_TPL_FRAME_IDX + REF_FRAMES + 1)
#define MAX_TPL_EXTEND (MAX_LAG_BUFFERS - MAX_GF_INTERVAL)
#define TPL_DEP_COST_SCALE_LOG2 4

#define TPL_EPSILON 0.0000001

typedef struct TplTxfmStats {
  double abs_coeff_sum[256];  // Assume we are using 16x16 transform block
  int txfm_block_count;
} TplTxfmStats;

typedef struct TplDepStats {
  int64_t intra_cost;
  int64_t inter_cost;
  int64_t srcrf_dist;
  int64_t recrf_dist;
  int64_t cmp_recrf_dist[2];
  int64_t srcrf_rate;
  int64_t recrf_rate;
  int64_t cmp_recrf_rate[2];
  int64_t mc_dep_rate;
  int64_t mc_dep_dist;
  int_mv mv[INTER_REFS_PER_FRAME];
  int ref_frame_index[2];
  int64_t pred_error[INTER_REFS_PER_FRAME];
} TplDepStats;

typedef struct TplDepFrame {
  uint8_t is_valid;
  TplDepStats *tpl_stats_ptr;
  const YV12_BUFFER_CONFIG *gf_picture;
  YV12_BUFFER_CONFIG *rec_picture;
  int ref_map_index[REF_FRAMES];
  int stride;
  int width;
  int height;
  int mi_rows;
  int mi_cols;
  int base_rdmult;
  uint32_t frame_display_index;
  double abs_coeff_sum[256];  // Assume we are using 16x16 transform block
  double abs_coeff_mean[256];
  int coeff_num;  // number of coefficients in a transform block
  int txfm_block_count;
} TplDepFrame;

/*!\endcond */
/*!
 * \brief Params related to temporal dependency model.
 */
typedef struct TplParams {
  /*!
   * Block granularity of tpl score storage.
   */
  uint8_t tpl_stats_block_mis_log2;

  /*!
   * Tpl motion estimation block 1d size. tpl_bsize_1d >= 16.
   */
  uint8_t tpl_bsize_1d;

  /*!
   * Buffer to store the frame level tpl information for each frame in a gf
   * group. tpl_stats_buffer[i] stores the tpl information of ith frame in a gf
   * group
   */
  TplDepFrame tpl_stats_buffer[MAX_LENGTH_TPL_FRAME_STATS];

  /*!
   * Buffer to store tpl stats at block granularity.
   * tpl_stats_pool[i][j] stores the tpl stats of jth block of ith frame in a gf
   * group.
   */
  TplDepStats *tpl_stats_pool[MAX_LAG_BUFFERS];

  /*!
   * Buffer to store tpl reconstructed frame.
   * tpl_rec_pool[i] stores the reconstructed frame of ith frame in a gf group.
   */
  YV12_BUFFER_CONFIG tpl_rec_pool[MAX_LAG_BUFFERS];

  /*!
   * Pointer to tpl_stats_buffer.
   */
  TplDepFrame *tpl_frame;

  /*!
   * Scale factors for the current frame.
   */
  struct scale_factors sf;

  /*!
   * GF group index of the current frame.
   */
  int frame_idx;

  /*!
   * Array of pointers to the frame buffers holding the source frame.
   * src_ref_frame[i] stores the pointer to the source frame of the ith
   * reference frame type.
   */
  const YV12_BUFFER_CONFIG *src_ref_frame[INTER_REFS_PER_FRAME];

  /*!
   * Array of pointers to the frame buffers holding the tpl reconstructed frame.
   * ref_frame[i] stores the pointer to the tpl reconstructed frame of the ith
   * reference frame type.
   */
  const YV12_BUFFER_CONFIG *ref_frame[INTER_REFS_PER_FRAME];

  /*!
   * Parameters related to synchronization for top-right dependency in row based
   * multi-threading of tpl
   */
  AV1TplRowMultiThreadSync tpl_mt_sync;

  /*!
   * Frame border for tpl frame.
   */
  int border_in_pixels;
} TplParams;

/*!\brief Allocate buffers used by tpl model
 *
 * \param[in]    Top-level encode/decode structure
 * \param[in]    lag_in_frames  number of lookahead frames
 *
 * \param[out]   tpl_data  tpl data structure
 */

void av1_setup_tpl_buffers(struct AV1_PRIMARY *const ppi,
                           CommonModeInfoParams *const mi_params, int width,
                           int height, int byte_alignment, int lag_in_frames);

/*!\brief Implements temporal dependency modelling for a GOP (GF/ARF
 * group) and selects between 16 and 32 frame GOP structure.
 *
 *\ingroup tpl_modelling
 *
 * \param[in]    cpi           Top - level encoder instance structure
 * \param[in]    gop_eval      Flag if it is in the GOP length decision stage
 * \param[in]    frame_params  Per frame encoding parameters
 * \param[in]    frame_input   Input frame buffers
 *
 * \return Indicates whether or not we should use a longer GOP length.
 */
int av1_tpl_setup_stats(struct AV1_COMP *cpi, int gop_eval,
                        const struct EncodeFrameParams *const frame_params,
                        const struct EncodeFrameInput *const frame_input);

/*!\cond */

int av1_tpl_ptr_pos(int mi_row, int mi_col, int stride, uint8_t right_shift);

void av1_init_tpl_stats(TplParams *const tpl_data);

void av1_tpl_rdmult_setup(struct AV1_COMP *cpi);

void av1_tpl_rdmult_setup_sb(struct AV1_COMP *cpi, MACROBLOCK *const x,
                             BLOCK_SIZE sb_size, int mi_row, int mi_col);

void av1_mc_flow_dispenser_row(struct AV1_COMP *cpi,
                               TplTxfmStats *tpl_txfm_stats, MACROBLOCK *x,
                               int mi_row, BLOCK_SIZE bsize, TX_SIZE tx_size);

/*!\brief  Compute the entropy of an exponential probability distribution
 * function (pdf) subjected to uniform quantization.
 *
 * pdf(x) = b*exp(-b*x)
 *
 *\ingroup tpl_modelling
 *
 * \param[in]    q_step        quantizer step size
 * \param[in]    b             parameter of exponential distribution
 *
 * \return entropy cost
 */
double av1_exponential_entropy(double q_step, double b);

/*!\brief  Compute the entropy of a Laplace probability distribution
 * function (pdf) subjected to non-uniform quantization.
 *
 * pdf(x) = 0.5*b*exp(-0.5*b*|x|)
 *
 *\ingroup tpl_modelling
 *
 * \param[in]    q_step          quantizer step size for non-zero bins
 * \param[in]    b               parameter of Laplace distribution
 * \param[in]    zero_bin_ratio  zero bin's size is zero_bin_ratio * q_step
 *
 * \return entropy cost
 */
double av1_laplace_entropy(double q_step, double b, double zero_bin_ratio);

/*!\brief  Compute the frame rate using transform block stats
 *
 * Assume each position i in the transform block is of Laplace distribution
 * with mean absolute deviation abs_coeff_mean[i]
 *
 * Then we can use av1_laplace_entropy() to compute the expected frame
 * rate.
 *
 *\ingroup tpl_modelling
 *
 * \param[in]    q_index         quantizer index
 * \param[in]    block_count     number of transform blocks
 * \param[in]    abs_coeff_mean  array of mean absolute deviation
 * \param[in]    coeff_num       number of coefficients per transform block
 *
 * \return expected frame rate
 */
double av1_laplace_estimate_frame_rate(int q_index, int block_count,
                                       const double *abs_coeff_mean,
                                       int coeff_num);

/*!\brief  Init data structure storing transform stats
 *
 *\ingroup tpl_modelling
 *
 * \param[in]    tpl_frame       pointer of tpl frame data structure
 * \param[in]    tpl_bsize_1d    length of the side of a square transform block
 *
 */
void av1_tpl_stats_init_txfm_stats(TplDepFrame *tpl_frame, int tpl_bsize_1d);

/*!\brief  Estimate coefficient entropy using Laplace dsitribution
 *
 *\ingroup tpl_modelling
 *
 * This function is equivalent to -log2(laplace_prob()), where laplace_prob() is
 * defined in tpl_model_test.cc
 *
 * \param[in]    q_step          quantizer step size without any scaling
 * \param[in]    b               mean absolute deviation of Laplace distribution
 * \param[in]    zero_bin_ratio  zero bin's size is zero_bin_ratio * q_step
 * \param[in]    qcoeff          quantized coefficient
 *
 * \return estimated coefficient entropy
 *
 */
double av1_estimate_coeff_entropy(double q_step, double b,
                                  double zero_bin_ratio, int qcoeff);

/*!\brief  Estimate entropy of a transform block using Laplace dsitribution
 *
 *\ingroup tpl_modelling
 *
 * \param[in]    q_index         quantizer index
 * \param[in]    abs_coeff_mean  array of mean absolute deviations
 * \param[in]    qcoeff_arr      array of quantized coefficients
 * \param[in]    coeff_num       number of coefficients per transform block
 *
 * \return estimated transform block entropy
 *
 */
double av1_estimate_txfm_block_entropy(int q_index,
                                       const double *abs_coeff_mean,
                                       int *qcoeff_arr, int coeff_num);

// TODO(angiebird): Add doxygen description here.
int64_t av1_delta_rate_cost(int64_t delta_rate, int64_t recrf_dist,
                            int64_t srcrf_dist, int pix_num);

/*!\brief  Compute the overlap area between two blocks with the same size
 *
 *\ingroup tpl_modelling
 *
 * If there is no overlap, this function should return zero.
 *
 * \param[in]    row_a  row position of the first block
 * \param[in]    col_a  column position of the first block
 * \param[in]    row_b  row position of the second block
 * \param[in]    col_b  column position of the second block
 * \param[in]    width  width shared by the two blocks
 * \param[in]    height height shared by the two blocks
 *
 * \return overlap area of the two blocks
 */
int av1_get_overlap_area(int row_a, int col_a, int row_b, int col_b, int width,
                         int height);

/*!\endcond */
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_TPL_MODEL_H_
