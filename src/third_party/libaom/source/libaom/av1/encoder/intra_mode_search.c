/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/av1_common_int.h"
#include "av1/common/reconintra.h"

#include "av1/encoder/intra_mode_search.h"
#include "av1/encoder/intra_mode_search_utils.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/tx_search.h"

/*!\cond */
static const PREDICTION_MODE intra_rd_search_mode_order[INTRA_MODES] = {
  DC_PRED,       H_PRED,        V_PRED,    SMOOTH_PRED, PAETH_PRED,
  SMOOTH_V_PRED, SMOOTH_H_PRED, D135_PRED, D203_PRED,   D157_PRED,
  D67_PRED,      D113_PRED,     D45_PRED,
};

static const UV_PREDICTION_MODE uv_rd_search_mode_order[UV_INTRA_MODES] = {
  UV_DC_PRED,     UV_CFL_PRED,   UV_H_PRED,        UV_V_PRED,
  UV_SMOOTH_PRED, UV_PAETH_PRED, UV_SMOOTH_V_PRED, UV_SMOOTH_H_PRED,
  UV_D135_PRED,   UV_D203_PRED,  UV_D157_PRED,     UV_D67_PRED,
  UV_D113_PRED,   UV_D45_PRED,
};

// The bitmask corresponds to the filter intra modes as defined in enums.h
// FILTER_INTRA_MODE enumeration type. Setting a bit to 0 in the mask means to
// disable the evaluation of corresponding filter intra mode. The table
// av1_derived_filter_intra_mode_used_flag is used when speed feature
// prune_filter_intra_level is 1. The evaluated filter intra modes are union
// of the following:
// 1) FILTER_DC_PRED
// 2) mode that corresponds to best mode so far of DC_PRED, V_PRED, H_PRED,
// D157_PRED and PAETH_PRED. (Eg: FILTER_V_PRED if best mode so far is V_PRED).
static const uint8_t av1_derived_filter_intra_mode_used_flag[INTRA_MODES] = {
  0x01,  // DC_PRED:           0000 0001
  0x03,  // V_PRED:            0000 0011
  0x05,  // H_PRED:            0000 0101
  0x01,  // D45_PRED:          0000 0001
  0x01,  // D135_PRED:         0000 0001
  0x01,  // D113_PRED:         0000 0001
  0x09,  // D157_PRED:         0000 1001
  0x01,  // D203_PRED:         0000 0001
  0x01,  // D67_PRED:          0000 0001
  0x01,  // SMOOTH_PRED:       0000 0001
  0x01,  // SMOOTH_V_PRED:     0000 0001
  0x01,  // SMOOTH_H_PRED:     0000 0001
  0x11   // PAETH_PRED:        0001 0001
};

// The bitmask corresponds to the chroma intra modes as defined in enums.h
// UV_PREDICTION_MODE enumeration type. Setting a bit to 0 in the mask means to
// disable the evaluation of corresponding chroma intra mode. The table
// av1_derived_chroma_intra_mode_used_flag is used when speed feature
// prune_chroma_modes_using_luma_winner is enabled. The evaluated chroma
// intra modes are union of the following:
// 1) UV_DC_PRED
// 2) UV_SMOOTH_PRED
// 3) UV_CFL_PRED
// 4) mode that corresponds to luma intra mode winner (Eg : UV_V_PRED if luma
// intra mode winner is V_PRED).
static const uint16_t av1_derived_chroma_intra_mode_used_flag[INTRA_MODES] = {
  0x2201,  // DC_PRED:           0010 0010 0000 0001
  0x2203,  // V_PRED:            0010 0010 0000 0011
  0x2205,  // H_PRED:            0010 0010 0000 0101
  0x2209,  // D45_PRED:          0010 0010 0000 1001
  0x2211,  // D135_PRED:         0010 0010 0001 0001
  0x2221,  // D113_PRED:         0010 0010 0010 0001
  0x2241,  // D157_PRED:         0010 0010 0100 0001
  0x2281,  // D203_PRED:         0010 0010 1000 0001
  0x2301,  // D67_PRED:          0010 0011 0000 0001
  0x2201,  // SMOOTH_PRED:       0010 0010 0000 0001
  0x2601,  // SMOOTH_V_PRED:     0010 0110 0000 0001
  0x2a01,  // SMOOTH_H_PRED:     0010 1010 0000 0001
  0x3201   // PAETH_PRED:        0011 0010 0000 0001
};
/*!\endcond */

/*!\brief Calculate the rdcost of a given luma intra angle
 *
 * \ingroup intra_mode_search
 * \callergraph
 * This function runs rd calculation for a given luma intra prediction angle.
 * This is used to select the best angle delta.
 *
 * \return Returns the rdcost of the angle and updates the mbmi if the
 * new rdcost is better.
 */
static int64_t calc_rd_given_intra_angle(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int mode_cost,
    int64_t best_rd_in, int8_t angle_delta, int max_angle_delta, int *rate,
    RD_STATS *rd_stats, int *best_angle_delta, TX_SIZE *best_tx_size,
    int64_t *best_rd, int64_t *best_model_rd, uint8_t *best_tx_type_map,
    uint8_t *best_blk_skip, int skip_model_rd) {
  RD_STATS tokenonly_rd_stats;
  int64_t this_rd;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int n4 = bsize_to_num_blk(bsize);
  assert(!is_inter_block(mbmi));
  mbmi->angle_delta[PLANE_TYPE_Y] = angle_delta;
  if (!skip_model_rd) {
    if (model_intra_yrd_and_prune(cpi, x, bsize, best_model_rd)) {
      return INT64_MAX;
    }
  }
  av1_pick_uniform_tx_size_type_yrd(cpi, x, &tokenonly_rd_stats, bsize,
                                    best_rd_in);
  if (tokenonly_rd_stats.rate == INT_MAX) return INT64_MAX;

  int this_rate =
      mode_cost + tokenonly_rd_stats.rate +
      x->mode_costs
          .angle_delta_cost[mbmi->mode - V_PRED][max_angle_delta + angle_delta];
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

  if (this_rd < *best_rd) {
    memcpy(best_blk_skip, x->txfm_search_info.blk_skip,
           sizeof(best_blk_skip[0]) * n4);
    av1_copy_array(best_tx_type_map, xd->tx_type_map, n4);
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[PLANE_TYPE_Y];
    *best_tx_size = mbmi->tx_size;
    *rate = this_rate;
    rd_stats->rate = tokenonly_rd_stats.rate;
    rd_stats->dist = tokenonly_rd_stats.dist;
    rd_stats->skip_txfm = tokenonly_rd_stats.skip_txfm;
  }
  return this_rd;
}

/*!\brief Search for the best filter_intra mode when coding intra frame.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * This function loops through all filter_intra modes to find the best one.
 *
 * \return Returns 1 if a new filter_intra mode is selected; 0 otherwise.
 */
static int rd_pick_filter_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, int mode_cost,
                                    PREDICTION_MODE best_mode_so_far,
                                    int64_t *best_rd, int64_t *best_model_rd,
                                    PICK_MODE_CONTEXT *ctx) {
  // Skip the evaluation of filter intra modes.
  if (cpi->sf.intra_sf.prune_filter_intra_level == 2) return 0;

  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  int filter_intra_selected_flag = 0;
  FILTER_INTRA_MODE mode;
  TX_SIZE best_tx_size = TX_8X8;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  av1_zero(filter_intra_mode_info);
  mbmi->filter_intra_mode_info.use_filter_intra = 1;
  mbmi->mode = DC_PRED;
  mbmi->palette_mode_info.palette_size[0] = 0;

  // Skip the evaluation of filter-intra if cached MB_MODE_INFO does not have
  // filter-intra as winner.
  if (x->use_mb_mode_cache &&
      !x->mb_mode_cache->filter_intra_mode_info.use_filter_intra)
    return 0;

  for (mode = 0; mode < FILTER_INTRA_MODES; ++mode) {
    int64_t this_rd;
    RD_STATS tokenonly_rd_stats;
    mbmi->filter_intra_mode_info.filter_intra_mode = mode;

    if ((cpi->sf.intra_sf.prune_filter_intra_level == 1) &&
        !(av1_derived_filter_intra_mode_used_flag[best_mode_so_far] &
          (1 << mode)))
      continue;

    // Skip the evaluation of modes that do not match with the winner mode in
    // x->mb_mode_cache.
    if (x->use_mb_mode_cache &&
        mode != x->mb_mode_cache->filter_intra_mode_info.filter_intra_mode)
      continue;

    if (model_intra_yrd_and_prune(cpi, x, bsize, best_model_rd)) {
      continue;
    }
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &tokenonly_rd_stats, bsize,
                                      *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) continue;
    const int this_rate =
        tokenonly_rd_stats.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost);
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    // Collect mode stats for multiwinner mode processing
    const int txfm_search_done = 1;
    store_winner_mode_stats(
        &cpi->common, x, mbmi, NULL, NULL, NULL, 0, NULL, bsize, this_rd,
        cpi->sf.winner_mode_sf.multi_winner_mode_type, txfm_search_done);
    if (this_rd < *best_rd) {
      *best_rd = this_rd;
      best_tx_size = mbmi->tx_size;
      filter_intra_mode_info = mbmi->filter_intra_mode_info;
      av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      memcpy(ctx->blk_skip, x->txfm_search_info.blk_skip,
             sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip_txfm;
      filter_intra_selected_flag = 1;
    }
  }

  if (filter_intra_selected_flag) {
    mbmi->mode = DC_PRED;
    mbmi->tx_size = best_tx_size;
    mbmi->filter_intra_mode_info = filter_intra_mode_info;
    av1_copy_array(ctx->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
    return 1;
  } else {
    return 0;
  }
}

void av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                      int *val_count, int *num_colors) {
  const int max_pix_val = 1 << 8;
  memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int this_val = src[r * stride + c];
      assert(this_val < max_pix_val);
      ++val_count[this_val];
    }
  }
  int n = 0;
  for (int i = 0; i < max_pix_val; ++i) {
    if (val_count[i]) ++n;
  }
  *num_colors = n;
}

void av1_count_colors_highbd(const uint8_t *src8, int stride, int rows,
                             int cols, int bit_depth, int *val_count,
                             int *bin_val_count, int *num_color_bins,
                             int *num_colors) {
  assert(bit_depth <= 12);
  const int max_bin_val = 1 << 8;
  const int max_pix_val = 1 << bit_depth;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  memset(bin_val_count, 0, max_bin_val * sizeof(val_count[0]));
  if (val_count != NULL)
    memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      /*
       * Down-convert the pixels to 8-bit domain before counting.
       * This provides consistency of behavior for palette search
       * between lbd and hbd encodes. This down-converted pixels
       * are only used for calculating the threshold (n).
       */
      const int this_val = ((src[r * stride + c]) >> (bit_depth - 8));
      assert(this_val < max_bin_val);
      if (this_val >= max_bin_val) continue;
      ++bin_val_count[this_val];
      if (val_count != NULL) ++val_count[(src[r * stride + c])];
    }
  }
  int n = 0;
  // Count the colors based on 8-bit domain used to gate the palette path
  for (int i = 0; i < max_bin_val; ++i) {
    if (bin_val_count[i]) ++n;
  }
  *num_color_bins = n;

  // Count the actual hbd colors used to create top_colors
  n = 0;
  if (val_count != NULL) {
    for (int i = 0; i < max_pix_val; ++i) {
      if (val_count[i]) ++n;
    }
    *num_colors = n;
  }
}

// Run RD calculation with given chroma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t pick_intra_angle_routine_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int rate_overhead, int64_t best_rd_in, int *rate, RD_STATS *rd_stats,
    int *best_angle_delta, int64_t *best_rd) {
  MB_MODE_INFO *mbmi = x->e_mbd.mi[0];
  assert(!is_inter_block(mbmi));
  int this_rate;
  int64_t this_rd;
  RD_STATS tokenonly_rd_stats;

  if (!av1_txfm_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in))
    return INT64_MAX;
  this_rate = tokenonly_rd_stats.rate +
              intra_mode_info_cost_uv(cpi, x, mbmi, bsize, rate_overhead);
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
    *rate = this_rate;
    rd_stats->rate = tokenonly_rd_stats.rate;
    rd_stats->dist = tokenonly_rd_stats.dist;
    rd_stats->skip_txfm = tokenonly_rd_stats.skip_txfm;
  }
  return this_rd;
}

/*!\brief Search for the best angle delta for chroma prediction
 *
 * \ingroup intra_mode_search
 * \callergraph
 * Given a chroma directional intra prediction mode, this function will try to
 * estimate the best delta_angle.
 *
 * \returns Return if there is a new mode with smaller rdcost than best_rd.
 */
static int rd_pick_intra_angle_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, int rate_overhead,
                                    int64_t best_rd, int *rate,
                                    RD_STATS *rd_stats) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  int i, angle_delta, best_angle_delta = 0;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];

  rd_stats->rate = INT_MAX;
  rd_stats->skip_txfm = 0;
  rd_stats->dist = INT64_MAX;
  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> ((angle_delta == 0) ? 3 : 5)));
      mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
      this_rd = pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead,
                                              best_rd_in, rate, rd_stats,
                                              &best_angle_delta, &best_rd);
      rd_cost[2 * angle_delta + i] = this_rd;
      if (angle_delta == 0) {
        if (this_rd == INT64_MAX) return 0;
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    int64_t rd_thresh;
    for (i = 0; i < 2; ++i) {
      int skip_search = 0;
      rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
        pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                      rate, rd_stats, &best_angle_delta,
                                      &best_rd);
      }
    }
  }

  mbmi->angle_delta[PLANE_TYPE_UV] = best_angle_delta;
  return rd_stats->rate != INT_MAX;
}

#define PLANE_SIGN_TO_JOINT_SIGN(plane, a, b) \
  (plane == CFL_PRED_U ? a * CFL_SIGNS + b - 1 : b * CFL_SIGNS + a - 1)

static void cfl_idx_to_sign_and_alpha(int cfl_idx, CFL_SIGN_TYPE *cfl_sign,
                                      int *cfl_alpha) {
  int cfl_linear_idx = cfl_idx - CFL_INDEX_ZERO;
  if (cfl_linear_idx == 0) {
    *cfl_sign = CFL_SIGN_ZERO;
    *cfl_alpha = 0;
  } else {
    *cfl_sign = cfl_linear_idx > 0 ? CFL_SIGN_POS : CFL_SIGN_NEG;
    *cfl_alpha = abs(cfl_linear_idx) - 1;
  }
}

static int64_t cfl_compute_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                              int plane, TX_SIZE tx_size,
                              BLOCK_SIZE plane_bsize, int cfl_idx,
                              int fast_mode, RD_STATS *rd_stats) {
  assert(IMPLIES(fast_mode, rd_stats == NULL));
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int cfl_plane = get_cfl_pred_type(plane);
  CFL_SIGN_TYPE cfl_sign;
  int cfl_alpha;
  cfl_idx_to_sign_and_alpha(cfl_idx, &cfl_sign, &cfl_alpha);
  // We conly build CFL for a given plane, the other plane's sign is dummy
  int dummy_sign = CFL_SIGN_NEG;
  const int8_t orig_cfl_alpha_signs = mbmi->cfl_alpha_signs;
  const uint8_t orig_cfl_alpha_idx = mbmi->cfl_alpha_idx;
  mbmi->cfl_alpha_signs =
      PLANE_SIGN_TO_JOINT_SIGN(cfl_plane, cfl_sign, dummy_sign);
  mbmi->cfl_alpha_idx = (cfl_alpha << CFL_ALPHABET_SIZE_LOG2) + cfl_alpha;
  int64_t cfl_cost;
  if (fast_mode) {
    cfl_cost =
        intra_model_rd(cm, x, plane, plane_bsize, tx_size, /*use_hadamard=*/0);
  } else {
    av1_init_rd_stats(rd_stats);
    av1_txfm_rd_in_plane(x, cpi, rd_stats, INT64_MAX, 0, plane, plane_bsize,
                         tx_size, FTXS_NONE, 0);
    av1_rd_cost_update(x->rdmult, rd_stats);
    cfl_cost = rd_stats->rdcost;
  }
  mbmi->cfl_alpha_signs = orig_cfl_alpha_signs;
  mbmi->cfl_alpha_idx = orig_cfl_alpha_idx;
  return cfl_cost;
}

static void cfl_pick_plane_parameter(const AV1_COMP *const cpi, MACROBLOCK *x,
                                     int plane, TX_SIZE tx_size,
                                     int cfl_search_range,
                                     RD_STATS cfl_rd_arr[CFL_MAGS_SIZE]) {
  assert(cfl_search_range >= 1 && cfl_search_range <= CFL_MAGS_SIZE);
  MACROBLOCKD *const xd = &x->e_mbd;

  xd->cfl.use_dc_pred_cache = 1;

  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->uv_mode == UV_CFL_PRED);
  const MACROBLOCKD_PLANE *pd = &xd->plane[plane];
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mbmi->bsize, pd->subsampling_x, pd->subsampling_y);

  const int dir_ls[2] = { 1, -1 };

  int est_best_cfl_idx = CFL_INDEX_ZERO;
  if (cfl_search_range < CFL_MAGS_SIZE) {
    int fast_mode = 1;
    int start_cfl_idx = CFL_INDEX_ZERO;
    int64_t best_cfl_cost = cfl_compute_rd(cpi, x, plane, tx_size, plane_bsize,
                                           start_cfl_idx, fast_mode, NULL);
    for (int si = 0; si < 2; ++si) {
      const int dir = dir_ls[si];
      for (int i = 1; i < CFL_MAGS_SIZE; ++i) {
        int cfl_idx = start_cfl_idx + dir * i;
        if (cfl_idx < 0 || cfl_idx >= CFL_MAGS_SIZE) break;
        int64_t cfl_cost = cfl_compute_rd(cpi, x, plane, tx_size, plane_bsize,
                                          cfl_idx, fast_mode, NULL);
        if (cfl_cost < best_cfl_cost) {
          best_cfl_cost = cfl_cost;
          est_best_cfl_idx = cfl_idx;
        } else {
          break;
        }
      }
    }
  }

  for (int cfl_idx = 0; cfl_idx < CFL_MAGS_SIZE; ++cfl_idx) {
    av1_invalid_rd_stats(&cfl_rd_arr[cfl_idx]);
  }

  int fast_mode = 0;
  int start_cfl_idx = est_best_cfl_idx;
  cfl_compute_rd(cpi, x, plane, tx_size, plane_bsize, start_cfl_idx, fast_mode,
                 &cfl_rd_arr[start_cfl_idx]);
  for (int si = 0; si < 2; ++si) {
    const int dir = dir_ls[si];
    for (int i = 1; i < cfl_search_range; ++i) {
      int cfl_idx = start_cfl_idx + dir * i;
      if (cfl_idx < 0 || cfl_idx >= CFL_MAGS_SIZE) break;
      cfl_compute_rd(cpi, x, plane, tx_size, plane_bsize, cfl_idx, fast_mode,
                     &cfl_rd_arr[cfl_idx]);
    }
  }
  xd->cfl.use_dc_pred_cache = 0;
  xd->cfl.dc_pred_is_cached[0] = 0;
  xd->cfl.dc_pred_is_cached[1] = 0;
}

/*!\brief Pick the optimal parameters for Chroma to Luma (CFL) component
 *
 * \ingroup intra_mode_search
 * \callergraph
 *
 * This function will use DCT_DCT followed by computing SATD (sum of absolute
 * transformed differences) to estimate the RD score and find the best possible
 * CFL parameter.
 *
 * Then the function will apply a full RD search near the best possible CFL
 * parameter to find the best actual CFL parameter.
 *
 * Side effect:
 * We use ths buffers in x->plane[] and xd->plane[] as throw-away buffers for RD
 * search.
 *
 * \param[in] x                Encoder prediction block structure.
 * \param[in] cpi              Top-level encoder instance structure.
 * \param[in] tx_size          Transform size.
 * \param[in] ref_best_rd      Reference best RD.
 * \param[in] cfl_search_range The search range of full RD search near the
 *                             estimated best CFL parameter.
 *
 * \param[out]   best_rd_stats          RD stats of the best CFL parameter
 * \param[out]   best_cfl_alpha_idx     Best CFL alpha index
 * \param[out]   best_cfl_alpha_signs   Best CFL joint signs
 *
 */
static int cfl_rd_pick_alpha(MACROBLOCK *const x, const AV1_COMP *const cpi,
                             TX_SIZE tx_size, int64_t ref_best_rd,
                             int cfl_search_range, RD_STATS *best_rd_stats,
                             uint8_t *best_cfl_alpha_idx,
                             int8_t *best_cfl_alpha_signs) {
  assert(cfl_search_range >= 1 && cfl_search_range <= CFL_MAGS_SIZE);
  const ModeCosts *mode_costs = &x->mode_costs;
  RD_STATS cfl_rd_arr_u[CFL_MAGS_SIZE];
  RD_STATS cfl_rd_arr_v[CFL_MAGS_SIZE];

  av1_invalid_rd_stats(best_rd_stats);

  cfl_pick_plane_parameter(cpi, x, 1, tx_size, cfl_search_range, cfl_rd_arr_u);
  cfl_pick_plane_parameter(cpi, x, 2, tx_size, cfl_search_range, cfl_rd_arr_v);

  for (int ui = 0; ui < CFL_MAGS_SIZE; ++ui) {
    if (cfl_rd_arr_u[ui].rate == INT_MAX) continue;
    int cfl_alpha_u;
    CFL_SIGN_TYPE cfl_sign_u;
    cfl_idx_to_sign_and_alpha(ui, &cfl_sign_u, &cfl_alpha_u);
    for (int vi = 0; vi < CFL_MAGS_SIZE; ++vi) {
      if (cfl_rd_arr_v[vi].rate == INT_MAX) continue;
      int cfl_alpha_v;
      CFL_SIGN_TYPE cfl_sign_v;
      cfl_idx_to_sign_and_alpha(vi, &cfl_sign_v, &cfl_alpha_v);
      // cfl_sign_u == CFL_SIGN_ZERO && cfl_sign_v == CFL_SIGN_ZERO is not a
      // valid parameter for CFL
      if (cfl_sign_u == CFL_SIGN_ZERO && cfl_sign_v == CFL_SIGN_ZERO) continue;
      int joint_sign = cfl_sign_u * CFL_SIGNS + cfl_sign_v - 1;
      RD_STATS rd_stats = cfl_rd_arr_u[ui];
      av1_merge_rd_stats(&rd_stats, &cfl_rd_arr_v[vi]);
      if (rd_stats.rate != INT_MAX) {
        rd_stats.rate +=
            mode_costs->cfl_cost[joint_sign][CFL_PRED_U][cfl_alpha_u];
        rd_stats.rate +=
            mode_costs->cfl_cost[joint_sign][CFL_PRED_V][cfl_alpha_v];
      }
      av1_rd_cost_update(x->rdmult, &rd_stats);
      if (rd_stats.rdcost < best_rd_stats->rdcost) {
        *best_rd_stats = rd_stats;
        *best_cfl_alpha_idx =
            (cfl_alpha_u << CFL_ALPHABET_SIZE_LOG2) + cfl_alpha_v;
        *best_cfl_alpha_signs = joint_sign;
      }
    }
  }
  if (best_rd_stats->rdcost >= ref_best_rd) {
    av1_invalid_rd_stats(best_rd_stats);
    // Set invalid CFL parameters here since the rdcost is not better than
    // ref_best_rd.
    *best_cfl_alpha_idx = 0;
    *best_cfl_alpha_signs = 0;
    return 0;
  }
  return 1;
}

int64_t av1_rd_pick_intra_sbuv_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  MB_MODE_INFO best_mbmi = *mbmi;
  int64_t best_rd = INT64_MAX, this_rd;
  const ModeCosts *mode_costs = &x->mode_costs;
  const IntraModeCfg *const intra_mode_cfg = &cpi->oxcf.intra_mode_cfg;

  init_sbuv_mode(mbmi);

  // Return if the current block does not correspond to a chroma block.
  if (!xd->is_chroma_ref) {
    *rate = 0;
    *rate_tokenonly = 0;
    *distortion = 0;
    *skippable = 1;
    return INT64_MAX;
  }

  // Only store reconstructed luma when there's chroma RDO. When there's no
  // chroma RDO, the reconstructed luma will be stored in encode_superblock().
  xd->cfl.store_y = store_cfl_required_rdo(cm, x);
  if (xd->cfl.store_y) {
    // Restore reconstructed luma values.
    // TODO(chiyotsai@google.com): right now we are re-computing the txfm in
    // this function everytime we search through uv modes. There is some
    // potential speed up here if we cache the result to avoid redundant
    // computation.
    av1_encode_intra_block_plane(cpi, x, mbmi->bsize, AOM_PLANE_Y,
                                 DRY_RUN_NORMAL,
                                 cpi->optimize_seg_arr[mbmi->segment_id]);
    xd->cfl.store_y = 0;
  }
  IntraModeSearchState intra_search_state;
  init_intra_mode_search_state(&intra_search_state);

  // Search through all non-palette modes.
  for (int mode_idx = 0; mode_idx < UV_INTRA_MODES; ++mode_idx) {
    int this_rate;
    RD_STATS tokenonly_rd_stats;
    UV_PREDICTION_MODE mode = uv_rd_search_mode_order[mode_idx];
    const int is_diagonal_mode = av1_is_diagonal_mode(get_uv_mode(mode));
    const int is_directional_mode = av1_is_directional_mode(get_uv_mode(mode));

    if (is_diagonal_mode && !cpi->oxcf.intra_mode_cfg.enable_diagonal_intra)
      continue;

    if (!(cpi->sf.intra_sf.intra_uv_mode_mask[txsize_sqr_up_map[max_tx_size]] &
          (1 << mode)))
      continue;
    if (!intra_mode_cfg->enable_smooth_intra && mode >= UV_SMOOTH_PRED &&
        mode <= UV_SMOOTH_H_PRED)
      continue;

    if (!intra_mode_cfg->enable_paeth_intra && mode == UV_PAETH_PRED) continue;

    assert(mbmi->mode < INTRA_MODES);
    if (cpi->sf.intra_sf.prune_chroma_modes_using_luma_winner &&
        !(av1_derived_chroma_intra_mode_used_flag[mbmi->mode] & (1 << mode)))
      continue;

    mbmi->uv_mode = mode;

    // Init variables for cfl and angle delta
    const SPEED_FEATURES *sf = &cpi->sf;
    mbmi->angle_delta[PLANE_TYPE_UV] = 0;
    if (mode == UV_CFL_PRED) {
      if (!is_cfl_allowed(xd) || !intra_mode_cfg->enable_cfl_intra) continue;
      assert(!is_directional_mode);
      const TX_SIZE uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
      if (!cfl_rd_pick_alpha(x, cpi, uv_tx_size, best_rd,
                             sf->intra_sf.cfl_search_range, &tokenonly_rd_stats,
                             &mbmi->cfl_alpha_idx, &mbmi->cfl_alpha_signs)) {
        continue;
      }
    } else if (is_directional_mode && av1_use_angle_delta(mbmi->bsize) &&
               intra_mode_cfg->enable_angle_delta) {
      if (sf->intra_sf.chroma_intra_pruning_with_hog &&
          !intra_search_state.dir_mode_skip_mask_ready) {
        static const float thresh[2][4] = {
          { -1.2f, 0.0f, 0.0f, 1.2f },    // Interframe
          { -1.2f, -1.2f, -0.6f, 0.4f },  // Intraframe
        };
        const int is_chroma = 1;
        const int is_intra_frame = frame_is_intra_only(cm);
        prune_intra_mode_with_hog(
            x, bsize, cm->seq_params.sb_size,
            thresh[is_intra_frame]
                  [sf->intra_sf.chroma_intra_pruning_with_hog - 1],
            intra_search_state.directional_mode_skip_mask, is_chroma);
        intra_search_state.dir_mode_skip_mask_ready = 1;
      }
      if (intra_search_state.directional_mode_skip_mask[mode]) {
        continue;
      }

      // Search through angle delta
      const int rate_overhead =
          mode_costs->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode];
      if (!rd_pick_intra_angle_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                    &this_rate, &tokenonly_rd_stats))
        continue;
    } else {
      // Predict directly if we don't need to search for angle delta.
      if (!av1_txfm_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd)) {
        continue;
      }
    }
    const int mode_cost =
        mode_costs->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode];
    this_rate = tokenonly_rd_stats.rate +
                intra_mode_info_cost_uv(cpi, x, mbmi, bsize, mode_cost);
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip_txfm;
    }
  }

  // Search palette mode
  const int try_palette =
      cpi->oxcf.tool_cfg.enable_palette &&
      av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                        mbmi->bsize);
  if (try_palette) {
    uint8_t *best_palette_color_map = x->palette_buffer->best_palette_color_map;
    av1_rd_pick_palette_intra_sbuv(
        cpi, x,
        mode_costs
            ->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][UV_DC_PRED],
        best_palette_color_map, &best_mbmi, &best_rd, rate, rate_tokenonly,
        distortion, skippable);
  }

  *mbmi = best_mbmi;
  // Make sure we actually chose a mode
  assert(best_rd < INT64_MAX);
  return best_rd;
}

// Searches palette mode for luma channel in inter frame.
int av1_search_palette_mode(IntraModeSearchState *intra_search_state,
                            const AV1_COMP *cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, unsigned int ref_frame_cost,
                            PICK_MODE_CONTEXT *ctx, RD_STATS *this_rd_cost,
                            int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MB_MODE_INFO *const mbmi = x->e_mbd.mi[0];
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  int rate2 = 0;
  int64_t distortion2 = 0, best_rd_palette = best_rd, this_rd,
          best_model_rd_palette = INT64_MAX;
  int skippable = 0;
  uint8_t *const best_palette_color_map =
      x->palette_buffer->best_palette_color_map;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  MB_MODE_INFO best_mbmi_palette = *mbmi;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const ModeCosts *mode_costs = &x->mode_costs;
  const int *const intra_mode_cost =
      mode_costs->mbmode_cost[size_group_lookup[bsize]];
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];

  mbmi->mode = DC_PRED;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  av1_zero(pmi->palette_size);

  RD_STATS rd_stats_y;
  av1_invalid_rd_stats(&rd_stats_y);
  av1_rd_pick_palette_intra_sby(
      cpi, x, bsize, intra_mode_cost[DC_PRED], &best_mbmi_palette,
      best_palette_color_map, &best_rd_palette, &best_model_rd_palette,
      &rd_stats_y.rate, NULL, &rd_stats_y.dist, &rd_stats_y.skip_txfm, NULL,
      ctx, best_blk_skip, best_tx_type_map);
  if (rd_stats_y.rate == INT_MAX || pmi->palette_size[0] == 0) {
    this_rd_cost->rdcost = INT64_MAX;
    return skippable;
  }

  memcpy(x->txfm_search_info.blk_skip, best_blk_skip,
         sizeof(best_blk_skip[0]) * bsize_to_num_blk(bsize));
  av1_copy_array(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
  memcpy(color_map, best_palette_color_map,
         rows * cols * sizeof(best_palette_color_map[0]));

  skippable = rd_stats_y.skip_txfm;
  distortion2 = rd_stats_y.dist;
  rate2 = rd_stats_y.rate + ref_frame_cost;
  if (num_planes > 1) {
    if (intra_search_state->rate_uv_intra == INT_MAX) {
      // We have not found any good uv mode yet, so we need to search for it.
      TX_SIZE uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
      av1_rd_pick_intra_sbuv_mode(cpi, x, &intra_search_state->rate_uv_intra,
                                  &intra_search_state->rate_uv_tokenonly,
                                  &intra_search_state->dist_uvs,
                                  &intra_search_state->skip_uvs, bsize, uv_tx);
      intra_search_state->mode_uv = mbmi->uv_mode;
      intra_search_state->pmi_uv = *pmi;
      intra_search_state->uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
    }

    // We have found at least one good uv mode before, so copy and paste it
    // over.
    mbmi->uv_mode = intra_search_state->mode_uv;
    pmi->palette_size[1] = intra_search_state->pmi_uv.palette_size[1];
    if (pmi->palette_size[1] > 0) {
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             intra_search_state->pmi_uv.palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = intra_search_state->uv_angle_delta;
    skippable = skippable && intra_search_state->skip_uvs;
    distortion2 += intra_search_state->dist_uvs;
    rate2 += intra_search_state->rate_uv_intra;
  }

  if (skippable) {
    rate2 -= rd_stats_y.rate;
    if (num_planes > 1) rate2 -= intra_search_state->rate_uv_tokenonly;
    rate2 += mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][1];
  } else {
    rate2 += mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][0];
  }
  this_rd = RDCOST(x->rdmult, rate2, distortion2);
  this_rd_cost->rate = rate2;
  this_rd_cost->dist = distortion2;
  this_rd_cost->rdcost = this_rd;
  return skippable;
}

/*!\brief Get the intra prediction by searching through tx_type and tx_size.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * Currently this function is only used in the intra frame code path for
 * winner-mode processing.
 *
 * \return Returns whether the current mode is an improvement over best_rd.
 */
static AOM_INLINE int intra_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      BLOCK_SIZE bsize, const int *bmode_costs,
                                      int64_t *best_rd, int *rate,
                                      int *rate_tokenonly, int64_t *distortion,
                                      int *skippable, MB_MODE_INFO *best_mbmi,
                                      PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  RD_STATS rd_stats;
  // In order to improve txfm search avoid rd based breakouts during winner
  // mode evaluation. Hence passing ref_best_rd as a maximum value
  av1_pick_uniform_tx_size_type_yrd(cpi, x, &rd_stats, bsize, INT64_MAX);
  if (rd_stats.rate == INT_MAX) return 0;
  int this_rate_tokenonly = rd_stats.rate;
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->bsize)) {
    // av1_pick_uniform_tx_size_type_yrd above includes the cost of the tx_size
    // in the tokenonly rate, but for intra blocks, tx_size is always coded
    // (prediction granularity), so we account for it in the full rate,
    // not the tokenonly rate.
    this_rate_tokenonly -= tx_size_cost(x, bsize, mbmi->tx_size);
  }
  const int this_rate =
      rd_stats.rate +
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode]);
  const int64_t this_rd = RDCOST(x->rdmult, this_rate, rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_mbmi = *mbmi;
    *best_rd = this_rd;
    *rate = this_rate;
    *rate_tokenonly = this_rate_tokenonly;
    *distortion = rd_stats.dist;
    *skippable = rd_stats.skip_txfm;
    av1_copy_array(ctx->blk_skip, x->txfm_search_info.blk_skip,
                   ctx->num_4x4_blk);
    av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    return 1;
  }
  return 0;
}

/*!\brief Search for the best angle delta for luma prediction
 *
 * \ingroup intra_mode_search
 * \callergraph
 * Given a luma directional intra prediction mode, this function will try to
 * estimate the best delta_angle.
 *
 * \return Returns the new rdcost of the best intra angle.
 */
static int64_t rd_pick_intra_angle_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       int *rate, RD_STATS *rd_stats,
                                       BLOCK_SIZE bsize, int mode_cost,
                                       int64_t best_rd, int64_t *best_model_rd,
                                       int skip_model_rd_for_zero_deg) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));

  int best_angle_delta = 0;
  int64_t rd_cost[2 * (MAX_ANGLE_DELTA + 2)];
  TX_SIZE best_tx_size = mbmi->tx_size;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];

  for (int i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  int first_try = 1;
  for (int angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (int i = 0; i < 2; ++i) {
      const int64_t best_rd_in =
          (best_rd == INT64_MAX) ? INT64_MAX
                                 : (best_rd + (best_rd >> (first_try ? 3 : 5)));
      const int64_t this_rd = calc_rd_given_intra_angle(
          cpi, x, bsize, mode_cost, best_rd_in, (1 - 2 * i) * angle_delta,
          MAX_ANGLE_DELTA, rate, rd_stats, &best_angle_delta, &best_tx_size,
          &best_rd, best_model_rd, best_tx_type_map, best_blk_skip,
          (skip_model_rd_for_zero_deg & !angle_delta));
      rd_cost[2 * angle_delta + i] = this_rd;
      if (first_try && this_rd == INT64_MAX) return best_rd;
      first_try = 0;
      if (angle_delta == 0) {
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (int angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (int i = 0; i < 2; ++i) {
      int skip_search = 0;
      const int64_t rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        calc_rd_given_intra_angle(
            cpi, x, bsize, mode_cost, best_rd, (1 - 2 * i) * angle_delta,
            MAX_ANGLE_DELTA, rate, rd_stats, &best_angle_delta, &best_tx_size,
            &best_rd, best_model_rd, best_tx_type_map, best_blk_skip, 0);
      }
    }
  }

  if (rd_stats->rate != INT_MAX) {
    mbmi->tx_size = best_tx_size;
    mbmi->angle_delta[PLANE_TYPE_Y] = best_angle_delta;
    const int n4 = bsize_to_num_blk(bsize);
    memcpy(x->txfm_search_info.blk_skip, best_blk_skip,
           sizeof(best_blk_skip[0]) * n4);
    av1_copy_array(xd->tx_type_map, best_tx_type_map, n4);
  }
  return best_rd;
}

/*!\brief Search for the best filter_intra mode when coding inter frame.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * This function loops through all filter_intra modes to find the best one.
 *
 * \return Returns nothing, but updates the mbmi and rd_stats.
 */
static INLINE void handle_filter_intra_mode(const AV1_COMP *cpi, MACROBLOCK *x,
                                            BLOCK_SIZE bsize,
                                            const PICK_MODE_CONTEXT *ctx,
                                            RD_STATS *rd_stats_y, int mode_cost,
                                            int64_t best_rd,
                                            int64_t best_rd_so_far) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->mode == DC_PRED &&
         av1_filter_intra_allowed_bsize(&cpi->common, bsize));

  RD_STATS rd_stats_y_fi;
  int filter_intra_selected_flag = 0;
  TX_SIZE best_tx_size = mbmi->tx_size;
  FILTER_INTRA_MODE best_fi_mode = FILTER_DC_PRED;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  memcpy(best_blk_skip, x->txfm_search_info.blk_skip,
         sizeof(best_blk_skip[0]) * ctx->num_4x4_blk);
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
  mbmi->filter_intra_mode_info.use_filter_intra = 1;
  for (FILTER_INTRA_MODE fi_mode = FILTER_DC_PRED; fi_mode < FILTER_INTRA_MODES;
       ++fi_mode) {
    mbmi->filter_intra_mode_info.filter_intra_mode = fi_mode;
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &rd_stats_y_fi, bsize, best_rd);
    if (rd_stats_y_fi.rate == INT_MAX) continue;
    const int this_rate_tmp =
        rd_stats_y_fi.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost);
    const int64_t this_rd_tmp =
        RDCOST(x->rdmult, this_rate_tmp, rd_stats_y_fi.dist);

    if (this_rd_tmp != INT64_MAX && this_rd_tmp / 2 > best_rd) {
      break;
    }
    if (this_rd_tmp < best_rd_so_far) {
      best_tx_size = mbmi->tx_size;
      av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      memcpy(best_blk_skip, x->txfm_search_info.blk_skip,
             sizeof(best_blk_skip[0]) * ctx->num_4x4_blk);
      best_fi_mode = fi_mode;
      *rd_stats_y = rd_stats_y_fi;
      filter_intra_selected_flag = 1;
      best_rd_so_far = this_rd_tmp;
    }
  }

  mbmi->tx_size = best_tx_size;
  av1_copy_array(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
  memcpy(x->txfm_search_info.blk_skip, best_blk_skip,
         sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);

  if (filter_intra_selected_flag) {
    mbmi->filter_intra_mode_info.use_filter_intra = 1;
    mbmi->filter_intra_mode_info.filter_intra_mode = best_fi_mode;
  } else {
    mbmi->filter_intra_mode_info.use_filter_intra = 0;
  }
}

int av1_handle_intra_y_mode(IntraModeSearchState *intra_search_state,
                            const AV1_COMP *cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, unsigned int ref_frame_cost,
                            const PICK_MODE_CONTEXT *ctx, RD_STATS *rd_stats_y,
                            int64_t best_rd, int *mode_cost_y, int64_t *rd_y) {
  const AV1_COMMON *cm = &cpi->common;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->ref_frame[0] == INTRA_FRAME);
  const PREDICTION_MODE mode = mbmi->mode;
  const ModeCosts *mode_costs = &x->mode_costs;
  const int mode_cost =
      mode_costs->mbmode_cost[size_group_lookup[bsize]][mode] + ref_frame_cost;
  const int skip_ctx = av1_get_skip_txfm_context(xd);

  int known_rate = mode_cost;
  const int intra_cost_penalty = av1_get_intra_cost_penalty(
      cm->quant_params.base_qindex, cm->quant_params.y_dc_delta_q,
      cm->seq_params.bit_depth);

  if (mode != DC_PRED && mode != PAETH_PRED) known_rate += intra_cost_penalty;
  known_rate += AOMMIN(mode_costs->skip_txfm_cost[skip_ctx][0],
                       mode_costs->skip_txfm_cost[skip_ctx][1]);
  const int64_t known_rd = RDCOST(x->rdmult, known_rate, 0);
  if (known_rd > best_rd) {
    intra_search_state->skip_intra_modes = 1;
    return 0;
  }

  const int is_directional_mode = av1_is_directional_mode(mode);
  if (is_directional_mode && av1_use_angle_delta(bsize) &&
      cpi->oxcf.intra_mode_cfg.enable_angle_delta) {
    if (sf->intra_sf.intra_pruning_with_hog &&
        !intra_search_state->dir_mode_skip_mask_ready) {
      const float thresh[4] = { -1.2f, 0.0f, 0.0f, 1.2f };
      const int is_chroma = 0;
      prune_intra_mode_with_hog(x, bsize, cm->seq_params.sb_size,
                                thresh[sf->intra_sf.intra_pruning_with_hog - 1],
                                intra_search_state->directional_mode_skip_mask,
                                is_chroma);
      intra_search_state->dir_mode_skip_mask_ready = 1;
    }
    if (intra_search_state->directional_mode_skip_mask[mode]) return 0;
    av1_init_rd_stats(rd_stats_y);
    rd_stats_y->rate = INT_MAX;
    int64_t model_rd = INT64_MAX;
    int rate_dummy;
    rd_pick_intra_angle_sby(cpi, x, &rate_dummy, rd_stats_y, bsize, mode_cost,
                            best_rd, &model_rd, 0);

  } else {
    av1_init_rd_stats(rd_stats_y);
    mbmi->angle_delta[PLANE_TYPE_Y] = 0;
    av1_pick_uniform_tx_size_type_yrd(cpi, x, rd_stats_y, bsize, best_rd);
  }

  // Pick filter intra modes.
  if (mode == DC_PRED && av1_filter_intra_allowed_bsize(cm, bsize)) {
    int try_filter_intra = 1;
    int64_t best_rd_so_far = INT64_MAX;
    if (rd_stats_y->rate != INT_MAX) {
      const int tmp_rate = rd_stats_y->rate +
                           mode_costs->filter_intra_cost[bsize][0] + mode_cost;
      best_rd_so_far = RDCOST(x->rdmult, tmp_rate, rd_stats_y->dist);
      try_filter_intra = (best_rd_so_far / 2) <= best_rd;
    }

    if (try_filter_intra) {
      handle_filter_intra_mode(cpi, x, bsize, ctx, rd_stats_y, mode_cost,
                               best_rd, best_rd_so_far);
    }
  }

  if (rd_stats_y->rate == INT_MAX) return 0;

  *mode_cost_y = intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost);
  const int rate_y = rd_stats_y->skip_txfm
                         ? mode_costs->skip_txfm_cost[skip_ctx][1]
                         : rd_stats_y->rate;
  *rd_y = RDCOST(x->rdmult, rate_y + *mode_cost_y, rd_stats_y->dist);
  if (best_rd < (INT64_MAX / 2) && *rd_y > (best_rd + (best_rd >> 2))) {
    intra_search_state->skip_intra_modes = 1;
    return 0;
  }

  return 1;
}

int av1_search_intra_uv_modes_in_interframe(
    IntraModeSearchState *intra_search_state, const AV1_COMP *cpi,
    MACROBLOCK *x, BLOCK_SIZE bsize, RD_STATS *rd_stats,
    const RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv, int64_t best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->ref_frame[0] == INTRA_FRAME);

  // TODO(chiyotsai@google.com): Consolidate the chroma search code here with
  // the one in av1_search_palette_mode.
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      cpi->oxcf.tool_cfg.enable_palette &&
      av1_allow_palette(cm->features.allow_screen_content_tools, mbmi->bsize);

  assert(intra_search_state->rate_uv_intra == INT_MAX);
  if (intra_search_state->rate_uv_intra == INT_MAX) {
    // If no good uv-predictor had been found, search for it.
    const TX_SIZE uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
    av1_rd_pick_intra_sbuv_mode(cpi, x, &intra_search_state->rate_uv_intra,
                                &intra_search_state->rate_uv_tokenonly,
                                &intra_search_state->dist_uvs,
                                &intra_search_state->skip_uvs, bsize, uv_tx);
    intra_search_state->mode_uv = mbmi->uv_mode;
    if (try_palette) intra_search_state->pmi_uv = *pmi;
    intra_search_state->uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];

    const int uv_rate = intra_search_state->rate_uv_tokenonly;
    const int64_t uv_dist = intra_search_state->dist_uvs;
    const int64_t uv_rd = RDCOST(x->rdmult, uv_rate, uv_dist);
    if (uv_rd > best_rd) {
      // If there is no good intra uv-mode available, we can skip all intra
      // modes.
      intra_search_state->skip_intra_modes = 1;
      return 0;
    }
  }

  // If we are here, then the encoder has found at least one good intra uv
  // predictor, so we can directly copy its statistics over.
  // TODO(any): the stats here is not right if the best uv mode is CFL but the
  // best y mode is palette.
  rd_stats_uv->rate = intra_search_state->rate_uv_tokenonly;
  rd_stats_uv->dist = intra_search_state->dist_uvs;
  rd_stats_uv->skip_txfm = intra_search_state->skip_uvs;
  rd_stats->skip_txfm = rd_stats_y->skip_txfm && rd_stats_uv->skip_txfm;
  mbmi->uv_mode = intra_search_state->mode_uv;
  if (try_palette) {
    pmi->palette_size[1] = intra_search_state->pmi_uv.palette_size[1];
    memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
           intra_search_state->pmi_uv.palette_colors + PALETTE_MAX_SIZE,
           2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
  }
  mbmi->angle_delta[PLANE_TYPE_UV] = intra_search_state->uv_angle_delta;

  return 1;
}

// Finds the best non-intrabc mode on an intra frame.
int64_t av1_rd_pick_intra_sby_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   int *rate, int *rate_tokenonly,
                                   int64_t *distortion, int *skippable,
                                   BLOCK_SIZE bsize, int64_t best_rd,
                                   PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  int64_t best_model_rd = INT64_MAX;
  int is_directional_mode;
  uint8_t directional_mode_skip_mask[INTRA_MODES] = { 0 };
  // Flag to check rd of any intra mode is better than best_rd passed to this
  // function
  int beat_best_rd = 0;
  const int *bmode_costs;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      cpi->oxcf.tool_cfg.enable_palette &&
      av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                        mbmi->bsize);
  uint8_t *best_palette_color_map =
      try_palette ? x->palette_buffer->best_palette_color_map : NULL;
  const MB_MODE_INFO *above_mi = xd->above_mbmi;
  const MB_MODE_INFO *left_mi = xd->left_mbmi;
  const PREDICTION_MODE A = av1_above_block_mode(above_mi);
  const PREDICTION_MODE L = av1_left_block_mode(left_mi);
  const int above_ctx = intra_mode_context[A];
  const int left_ctx = intra_mode_context[L];
  bmode_costs = x->mode_costs.y_mode_costs[above_ctx][left_ctx];

  mbmi->angle_delta[PLANE_TYPE_Y] = 0;
  if (cpi->sf.intra_sf.intra_pruning_with_hog) {
    // Less aggressive thresholds are used here than those used in inter frame
    // encoding in av1_handle_intra_y_mode() because we want key frames/intra
    // frames to have higher quality.
    const float thresh[4] = { -1.2f, -1.2f, -0.6f, 0.4f };
    const int is_chroma = 0;
    prune_intra_mode_with_hog(
        x, bsize, cpi->common.seq_params.sb_size,
        thresh[cpi->sf.intra_sf.intra_pruning_with_hog - 1],
        directional_mode_skip_mask, is_chroma);
  }
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  pmi->palette_size[0] = 0;

  // Set params for mode evaluation
  set_mode_eval_params(cpi, x, MODE_EVAL);

  MB_MODE_INFO best_mbmi = *mbmi;
  av1_zero_array(x->winner_mode_stats, MAX_WINNER_MODE_COUNT_INTRA);
  x->winner_mode_count = 0;

  // Searches the intra-modes except for intrabc, palette, and filter_intra.
  for (int mode_idx = INTRA_MODE_START; mode_idx < INTRA_MODE_END; ++mode_idx) {
    RD_STATS this_rd_stats;
    int this_rate, this_rate_tokenonly, s;
    int is_diagonal_mode;
    int64_t this_distortion, this_rd;
    mbmi->mode = intra_rd_search_mode_order[mode_idx];

    is_diagonal_mode = av1_is_diagonal_mode(mbmi->mode);
    if (is_diagonal_mode && !cpi->oxcf.intra_mode_cfg.enable_diagonal_intra)
      continue;

    // The smooth prediction mode appears to be more frequently picked
    // than horizontal / vertical smooth prediction modes. Hence treat
    // them differently in speed features.
    if ((!cpi->oxcf.intra_mode_cfg.enable_smooth_intra ||
         cpi->sf.intra_sf.disable_smooth_intra) &&
        (mbmi->mode == SMOOTH_H_PRED || mbmi->mode == SMOOTH_V_PRED))
      continue;
    if (!cpi->oxcf.intra_mode_cfg.enable_smooth_intra &&
        mbmi->mode == SMOOTH_PRED)
      continue;

    // The functionality of filter intra modes and smooth prediction
    // overlap. Hence smooth prediction is pruned only if all the
    // filter intra modes are enabled.
    if (cpi->sf.intra_sf.disable_smooth_intra &&
        cpi->sf.intra_sf.prune_filter_intra_level == 0 &&
        mbmi->mode == SMOOTH_PRED)
      continue;
    if (!cpi->oxcf.intra_mode_cfg.enable_paeth_intra &&
        mbmi->mode == PAETH_PRED)
      continue;
    mbmi->angle_delta[PLANE_TYPE_Y] = 0;

    // Skip the evaluation of modes that do not match with the winner mode in
    // x->mb_mode_cache.
    if (x->use_mb_mode_cache && mbmi->mode != x->mb_mode_cache->mode) continue;

    is_directional_mode = av1_is_directional_mode(mbmi->mode);
    if (is_directional_mode && directional_mode_skip_mask[mbmi->mode]) continue;
    if (is_directional_mode && av1_use_angle_delta(bsize) &&
        cpi->oxcf.intra_mode_cfg.enable_angle_delta) {
      // Searches through the best angle_delta if this option is available.
      this_rd_stats.rate = INT_MAX;
      rd_pick_intra_angle_sby(cpi, x, &this_rate, &this_rd_stats, bsize,
                              bmode_costs[mbmi->mode], best_rd, &best_model_rd,
                              1);
    } else {
      if (model_intra_yrd_and_prune(cpi, x, bsize, &best_model_rd)) {
        continue;
      }

      // Builds the actual prediction. The prediction from
      // model_intra_yrd_and_prune was just an estimation that did not take into
      // account the effect of txfm pipeline, so we need to redo it for real
      // here.
      av1_pick_uniform_tx_size_type_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
    }
    this_rate_tokenonly = this_rd_stats.rate;
    this_distortion = this_rd_stats.dist;
    s = this_rd_stats.skip_txfm;

    if (this_rate_tokenonly == INT_MAX) continue;

    if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->bsize)) {
      // av1_pick_uniform_tx_size_type_yrd above includes the cost of the
      // tx_size in the tokenonly rate, but for intra blocks, tx_size is always
      // coded (prediction granularity), so we account for it in the full rate,
      // not the tokenonly rate.
      this_rate_tokenonly -= tx_size_cost(x, bsize, mbmi->tx_size);
    }
    this_rate =
        this_rd_stats.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode]);
    this_rd = RDCOST(x->rdmult, this_rate, this_distortion);
    // Collect mode stats for multiwinner mode processing
    const int txfm_search_done = 1;
    store_winner_mode_stats(
        &cpi->common, x, mbmi, NULL, NULL, NULL, 0, NULL, bsize, this_rd,
        cpi->sf.winner_mode_sf.multi_winner_mode_type, txfm_search_done);
    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      // Setting beat_best_rd flag because current mode rd is better than
      // best_rd passed to this function
      beat_best_rd = 1;
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_distortion;
      *skippable = s;
      memcpy(ctx->blk_skip, x->txfm_search_info.blk_skip,
             sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);
      av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    }
  }

  // Searches palette
  if (try_palette) {
    av1_rd_pick_palette_intra_sby(
        cpi, x, bsize, bmode_costs[DC_PRED], &best_mbmi, best_palette_color_map,
        &best_rd, &best_model_rd, rate, rate_tokenonly, distortion, skippable,
        &beat_best_rd, ctx, ctx->blk_skip, ctx->tx_type_map);
  }

  // Searches filter_intra
  if (beat_best_rd && av1_filter_intra_allowed_bsize(&cpi->common, bsize)) {
    if (rd_pick_filter_intra_sby(cpi, x, rate, rate_tokenonly, distortion,
                                 skippable, bsize, bmode_costs[DC_PRED],
                                 best_mbmi.mode, &best_rd, &best_model_rd,
                                 ctx)) {
      best_mbmi = *mbmi;
    }
  }

  // No mode is identified with less rd value than best_rd passed to this
  // function. In such cases winner mode processing is not necessary and return
  // best_rd as INT64_MAX to indicate best mode is not identified
  if (!beat_best_rd) return INT64_MAX;

  // In multi-winner mode processing, perform tx search for few best modes
  // identified during mode evaluation. Winner mode processing uses best tx
  // configuration for tx search.
  if (cpi->sf.winner_mode_sf.multi_winner_mode_type) {
    int best_mode_idx = 0;
    int block_width, block_height;
    uint8_t *color_map_dst = xd->plane[PLANE_TYPE_Y].color_index_map;
    av1_get_block_dimensions(bsize, AOM_PLANE_Y, xd, &block_width,
                             &block_height, NULL, NULL);

    for (int mode_idx = 0; mode_idx < x->winner_mode_count; mode_idx++) {
      *mbmi = x->winner_mode_stats[mode_idx].mbmi;
      if (is_winner_mode_processing_enabled(cpi, mbmi, mbmi->mode)) {
        // Restore color_map of palette mode before winner mode processing
        if (mbmi->palette_mode_info.palette_size[0] > 0) {
          uint8_t *color_map_src =
              x->winner_mode_stats[mode_idx].color_index_map;
          memcpy(color_map_dst, color_map_src,
                 block_width * block_height * sizeof(*color_map_src));
        }
        // Set params for winner mode evaluation
        set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);

        // Winner mode processing
        // If previous searches use only the default tx type/no R-D optimization
        // of quantized coeffs, do an extra search for the best tx type/better
        // R-D optimization of quantized coeffs
        if (intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                            rate_tokenonly, distortion, skippable, &best_mbmi,
                            ctx))
          best_mode_idx = mode_idx;
      }
    }
    // Copy color_map of palette mode for final winner mode
    if (best_mbmi.palette_mode_info.palette_size[0] > 0) {
      uint8_t *color_map_src =
          x->winner_mode_stats[best_mode_idx].color_index_map;
      memcpy(color_map_dst, color_map_src,
             block_width * block_height * sizeof(*color_map_src));
    }
  } else {
    // If previous searches use only the default tx type/no R-D optimization of
    // quantized coeffs, do an extra search for the best tx type/better R-D
    // optimization of quantized coeffs
    if (is_winner_mode_processing_enabled(cpi, mbmi, best_mbmi.mode)) {
      // Set params for winner mode evaluation
      set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);
      *mbmi = best_mbmi;
      intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                      rate_tokenonly, distortion, skippable, &best_mbmi, ctx);
    }
  }
  *mbmi = best_mbmi;
  av1_copy_array(xd->tx_type_map, ctx->tx_type_map, ctx->num_4x4_blk);
  return best_rd;
}
