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

#include <limits.h>

#include "av1/common/reconintra.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/rdopt.h"

#include "aom_dsp/aom_dsp_common.h"

#define MAX_MESH_SPEED 5  // Max speed setting for mesh motion method
// Max speed setting for tx domain evaluation
#define MAX_TX_DOMAIN_EVAL_SPEED 5
static MESH_PATTERN
    good_quality_mesh_patterns[MAX_MESH_SPEED + 1][MAX_MESH_STEP] = {
      { { 64, 8 }, { 28, 4 }, { 15, 1 }, { 7, 1 } },
      { { 64, 8 }, { 28, 4 }, { 15, 1 }, { 7, 1 } },
      { { 64, 8 }, { 14, 2 }, { 7, 1 }, { 7, 1 } },
      { { 64, 16 }, { 24, 8 }, { 12, 4 }, { 7, 1 } },
      { { 64, 16 }, { 24, 8 }, { 12, 4 }, { 7, 1 } },
      { { 64, 16 }, { 24, 8 }, { 12, 4 }, { 7, 1 } },
    };

// TODO(huisu@google.com): These settings are pretty relaxed, tune them for
// each speed setting
static MESH_PATTERN intrabc_mesh_patterns[MAX_MESH_SPEED + 1][MAX_MESH_STEP] = {
  { { 256, 1 }, { 256, 1 }, { 0, 0 }, { 0, 0 } },
  { { 256, 1 }, { 256, 1 }, { 0, 0 }, { 0, 0 } },
  { { 64, 1 }, { 64, 1 }, { 0, 0 }, { 0, 0 } },
  { { 64, 1 }, { 64, 1 }, { 0, 0 }, { 0, 0 } },
  { { 64, 4 }, { 16, 1 }, { 0, 0 }, { 0, 0 } },
  { { 64, 4 }, { 16, 1 }, { 0, 0 }, { 0, 0 } },
};

// Threshold values to be used for pruning the txfm_domain_distortion
// based on block MSE
// Index 0: Default mode evaluation, Winner mode processing is not
// applicable (Eg : IntraBc). Index 1: Mode evaluation.
// Index 2: Winner mode evaluation. Index 1 and 2 are applicable when
// enable_winner_mode_for_use_tx_domain_dist speed feature is ON
// TODO(any): Experiment the threshold logic based on variance metric
static unsigned int tx_domain_dist_thresholds[3][MODE_EVAL_TYPES] = {
  { UINT_MAX, UINT_MAX, UINT_MAX }, { 22026, 22026, 22026 }, { 0, 0, 0 }
};

// Transform domain distortion type to be used for default, mode and winner mode
// evaluation Index 0: Default mode evaluation, Winner mode processing is not
// applicable (Eg : IntraBc). Index 1: Mode evaluation. Index 2: Winner mode
// evaluation. Index 1 and 2 are applicable when
// enable_winner_mode_for_use_tx_domain_dist speed feature is ON
static unsigned int tx_domain_dist_types[3][MODE_EVAL_TYPES] = { { 0, 2, 0 },
                                                                 { 1, 2, 0 },
                                                                 { 2, 2, 0 } };

// Threshold values to be used for disabling coeff RD-optimization
// based on block MSE / qstep^2.
// TODO(any): Experiment the threshold logic based on variance metric.
// For each row, the indices are as follows.
// Index 0: Default mode evaluation, Winner mode processing is not applicable
// (Eg : IntraBc)
// Index 1: Mode evaluation.
// Index 2: Winner mode evaluation.
// Index 1 and 2 are applicable when enable_winner_mode_for_coeff_opt speed
// feature is ON
// There are 7 levels with increasing speed, mapping to vertical indices.
static unsigned int coeff_opt_dist_thresholds[7][MODE_EVAL_TYPES] = {
  { UINT_MAX, UINT_MAX, UINT_MAX },
  { 3200, 250, UINT_MAX },
  { 1728, 142, UINT_MAX },
  { 864, 142, UINT_MAX },
  { 432, 86, UINT_MAX },
  { 216, 86, UINT_MAX },
  { 216, 0, UINT_MAX }
};

static unsigned int coeff_opt_satd_thresholds[3][MODE_EVAL_TYPES] = {
  { UINT_MAX, UINT_MAX, UINT_MAX },
  { 97, 16, UINT_MAX },
  { 25, 10, UINT_MAX },
};

// Transform size to be used for default, mode and winner mode evaluation
// Index 0: Default mode evaluation, Winner mode processing is not applicable
// (Eg : IntraBc) Index 1: Mode evaluation. Index 2: Winner mode evaluation.
// Index 1 and 2 are applicable when enable_winner_mode_for_tx_size_srch speed
// feature is ON
static TX_SIZE_SEARCH_METHOD tx_size_search_methods[3][MODE_EVAL_TYPES] = {
  { USE_FULL_RD, USE_LARGESTALL, USE_FULL_RD },
  { USE_FAST_RD, USE_LARGESTALL, USE_FULL_RD },
  { USE_LARGESTALL, USE_LARGESTALL, USE_FULL_RD }
};

// Predict transform skip levels to be used for default, mode and winner mode
// evaluation. Index 0: Default mode evaluation, Winner mode processing is not
// applicable. Index 1: Mode evaluation, Index 2: Winner mode evaluation
// Values indicate the aggressiveness of skip flag prediction.
// 0 : no early skip prediction
// 1 : conservative early skip prediction using DCT_DCT
// 2 : early skip prediction based on SSE
static unsigned int predict_skip_levels[3][MODE_EVAL_TYPES] = { { 0, 0, 0 },
                                                                { 1, 1, 1 },
                                                                { 1, 2, 1 } };

// Predict DC block levels to be used for default, mode and winner mode
// evaluation. Index 0: Default mode evaluation, Winner mode processing is not
// applicable. Index 1: Mode evaluation, Index 2: Winner mode evaluation
// Values indicate the aggressiveness of skip flag prediction.
// 0 : no early DC block prediction
// 1 : Early DC block prediction based on error variance
static unsigned int predict_dc_levels[3][MODE_EVAL_TYPES] = { { 0, 0, 0 },
                                                              { 1, 1, 0 },
                                                              { 1, 1, 1 } };

// This table holds the maximum number of reference frames for global motion.
// The table is indexed as per the speed feature 'gm_search_type'.
// 0 : All reference frames are allowed.
// 1 : All reference frames except L2 and L3 are allowed.
// 2 : All reference frames except L2, L3 and ARF2 are allowed.
// 3 : No reference frame is allowed.
static int gm_available_reference_frames[GM_DISABLE_SEARCH + 1] = {
  INTER_REFS_PER_FRAME, INTER_REFS_PER_FRAME - 2, INTER_REFS_PER_FRAME - 3, 0
};

// Qindex threshold levels used for selecting full-pel motion search.
// ms_qthresh[i][j][k] indicates the qindex boundary value for 'k'th qindex band
// for resolution index 'j' for aggressiveness level 'i'.
// Aggressiveness increases from i = 0 to 2.
// j = 0: lower than 720p resolution, j = 1: 720p or larger resolution.
// Currently invoked only for speed 0, 1 and 2.
static int ms_qindex_thresh[3][2][2] = { { { 200, 70 }, { MAXQ, 200 } },
                                         { { 170, 50 }, { MAXQ, 200 } },
                                         { { 170, 40 }, { 200, 40 } } };

// Full-pel search methods for aggressive search based on qindex.
// Index 0 is for resolutions lower than 720p, index 1 for 720p or larger
// resolutions. Currently invoked only for speed 1 and 2.
static SEARCH_METHODS motion_search_method[2] = { CLAMPED_DIAMOND, DIAMOND };

// Intra only frames, golden frames (except alt ref overlays) and
// alt ref frames tend to be coded at a higher than ambient quality
static int frame_is_boosted(const AV1_COMP *cpi) {
  return frame_is_kf_gf_arf(cpi);
}

static void set_good_speed_feature_framesize_dependent(
    const AV1_COMP *const cpi, SPEED_FEATURES *const sf, int speed) {
  const AV1_COMMON *const cm = &cpi->common;
  const int is_480p_or_larger = AOMMIN(cm->width, cm->height) >= 480;
  const int is_720p_or_larger = AOMMIN(cm->width, cm->height) >= 720;
  const int is_1080p_or_larger = AOMMIN(cm->width, cm->height) >= 1080;
  const int is_4k_or_larger = AOMMIN(cm->width, cm->height) >= 2160;
  const bool use_hbd = cpi->oxcf.use_highbitdepth;

  if (is_480p_or_larger) {
    sf->part_sf.use_square_partition_only_threshold = BLOCK_128X128;
    if (is_720p_or_larger)
      sf->part_sf.auto_max_partition_based_on_simple_motion = ADAPT_PRED;
    else
      sf->part_sf.auto_max_partition_based_on_simple_motion = RELAXED_PRED;
  } else {
    sf->part_sf.use_square_partition_only_threshold = BLOCK_64X64;
    sf->part_sf.auto_max_partition_based_on_simple_motion = DIRECT_PRED;
    if (use_hbd) sf->tx_sf.prune_tx_size_level = 1;
  }

  if (is_4k_or_larger) {
    sf->part_sf.default_min_partition_size = BLOCK_8X8;
  }

  // TODO(huisu@google.com): train models for 720P and above.
  if (!is_720p_or_larger) {
    sf->part_sf.ml_partition_search_breakout_thresh[0] = 200;  // BLOCK_8X8
    sf->part_sf.ml_partition_search_breakout_thresh[1] = 250;  // BLOCK_16X16
    sf->part_sf.ml_partition_search_breakout_thresh[2] = 300;  // BLOCK_32X32
    sf->part_sf.ml_partition_search_breakout_thresh[3] = 500;  // BLOCK_64X64
    sf->part_sf.ml_partition_search_breakout_thresh[4] = -1;   // BLOCK_128X128
    sf->part_sf.ml_early_term_after_part_split_level = 1;
  }

  if (is_720p_or_larger) {
    // TODO(chiyotsai@google.com): make this speed feature adaptive based on
    // current block's vertical texture instead of hardcoded with resolution
    sf->mv_sf.use_downsampled_sad = 1;
  }

  if (speed >= 1) {
    if (is_720p_or_larger) {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_128X128;
    } else if (is_480p_or_larger) {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_64X64;
    } else {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_32X32;
    }

    if (!is_720p_or_larger) {
      sf->part_sf.ml_partition_search_breakout_thresh[0] = 200;  // BLOCK_8X8
      sf->part_sf.ml_partition_search_breakout_thresh[1] = 250;  // BLOCK_16X16
      sf->part_sf.ml_partition_search_breakout_thresh[2] = 300;  // BLOCK_32X32
      sf->part_sf.ml_partition_search_breakout_thresh[3] = 300;  // BLOCK_64X64
      sf->part_sf.ml_partition_search_breakout_thresh[4] = -1;  // BLOCK_128X128
    }
    sf->part_sf.ml_early_term_after_part_split_level = 2;
  }

  if (speed >= 2) {
    if (is_720p_or_larger) {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_64X64;
    } else if (is_480p_or_larger) {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_32X32;
    } else {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_32X32;
    }

    if (is_720p_or_larger) {
      sf->part_sf.partition_search_breakout_dist_thr = (1 << 24);
      sf->part_sf.partition_search_breakout_rate_thr = 120;
    } else {
      sf->part_sf.partition_search_breakout_dist_thr = (1 << 22);
      sf->part_sf.partition_search_breakout_rate_thr = 100;
    }

    if (is_720p_or_larger) {
      sf->inter_sf.prune_obmc_prob_thresh = 16;
    } else {
      sf->inter_sf.prune_obmc_prob_thresh = 8;
    }

    if (is_480p_or_larger) {
      sf->tx_sf.tx_type_search.prune_tx_type_using_stats = 1;
      if (use_hbd) sf->tx_sf.prune_tx_size_level = 2;
    } else {
      if (use_hbd) sf->tx_sf.prune_tx_size_level = 3;
    }
  }

  if (speed >= 3) {
    sf->part_sf.ml_early_term_after_part_split_level = 0;

    if (is_720p_or_larger) {
      sf->part_sf.partition_search_breakout_dist_thr = (1 << 25);
      sf->part_sf.partition_search_breakout_rate_thr = 200;
    } else {
      sf->part_sf.max_intra_bsize = BLOCK_32X32;
      sf->part_sf.partition_search_breakout_dist_thr = (1 << 23);
      sf->part_sf.partition_search_breakout_rate_thr = 120;
    }
    if (use_hbd) sf->tx_sf.prune_tx_size_level = 3;
  }

  if (speed >= 4) {
    if (is_720p_or_larger) {
      sf->part_sf.partition_search_breakout_dist_thr = (1 << 26);
    } else {
      sf->part_sf.partition_search_breakout_dist_thr = (1 << 24);
    }

    if (is_480p_or_larger) {
      sf->tx_sf.tx_type_search.prune_tx_type_using_stats = 2;
    }

    sf->inter_sf.prune_obmc_prob_thresh = 16;
  }

  if (speed >= 5) {
    if (is_720p_or_larger) {
      sf->inter_sf.prune_warped_prob_thresh = 16;
    } else if (is_480p_or_larger) {
      sf->inter_sf.prune_warped_prob_thresh = 8;
    }
  }

  if (speed >= 6) {
    if (is_720p_or_larger) {
      sf->part_sf.auto_max_partition_based_on_simple_motion = NOT_IN_USE;
    } else if (is_480p_or_larger) {
      sf->part_sf.auto_max_partition_based_on_simple_motion = DIRECT_PRED;
    }

    if (is_1080p_or_larger) {
      sf->part_sf.default_min_partition_size = BLOCK_8X8;
    }

    if (is_720p_or_larger) {
      sf->inter_sf.disable_masked_comp = 1;
    }

    if (!is_720p_or_larger) {
      sf->inter_sf.mv_cost_upd_level = 2;
    }

    // TODO(yunqing): use BLOCK_32X32 for >= 4k.
    if (is_4k_or_larger) {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_64X64;
    } else if (is_720p_or_larger) {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_32X32;
    } else {
      sf->part_sf.use_square_partition_only_threshold = BLOCK_16X16;
    }

    if (is_720p_or_larger) {
      sf->inter_sf.prune_ref_mv_idx_search = 2;
    } else {
      sf->inter_sf.prune_ref_mv_idx_search = 1;
    }
  }
}

static void set_rt_speed_feature_framesize_dependent(const AV1_COMP *const cpi,
                                                     SPEED_FEATURES *const sf,
                                                     int speed) {
  const AV1_COMMON *const cm = &cpi->common;
  const int is_720p_or_larger = AOMMIN(cm->width, cm->height) >= 720;
  const int is_480p_or_larger = AOMMIN(cm->width, cm->height) >= 480;
  const int is_360p_or_larger = AOMMIN(cm->width, cm->height) >= 360;

  (void)is_720p_or_larger;  // Not used so far

  if (!is_360p_or_larger) {
    if (speed >= 6) sf->rt_sf.force_tx_search_off = 1;
    if (speed >= 8) {
      sf->rt_sf.use_modeled_non_rd_cost = 0;
      sf->rt_sf.use_nonrd_filter_search = 0;
    }
    if (speed >= 9) {
      sf->rt_sf.use_modeled_non_rd_cost = 1;
// TODO(kyslov) Re-enable when AV1 models are trained
#if 0
      if (!frame_is_intra_only(cm)) {
        sf->part_sf.partition_search_type = ML_BASED_PARTITION;
        sf->rt_sf.reuse_inter_pred_nonrd = 0;
      }
#endif
    }
  } else {
    if (speed == 8 && !cpi->use_svc) {
      sf->rt_sf.short_circuit_low_temp_var = 0;
      sf->rt_sf.use_nonrd_altref_frame = 1;
    }
  }
  if (!is_480p_or_larger) {
    if (speed == 7) {
      sf->rt_sf.nonrd_check_partition_merge_mode = 2;
    }
    if (speed >= 8) {
      sf->mv_sf.subpel_search_method = SUBPEL_TREE;
      sf->rt_sf.estimate_motion_for_var_based_partition = 1;
    }
    if (speed >= 9) {
      sf->mv_sf.subpel_search_method = SUBPEL_TREE_PRUNED;
      sf->rt_sf.estimate_motion_for_var_based_partition = 0;
    }
  }
}

static void set_good_speed_features_framesize_independent(
    const AV1_COMP *const cpi, SPEED_FEATURES *const sf, int speed) {
  const AV1_COMMON *const cm = &cpi->common;
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const int boosted = frame_is_boosted(cpi);
  const int is_boosted_arf2_bwd_type =
      boosted || gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE;
  const int allow_screen_content_tools =
      cm->features.allow_screen_content_tools;
  const int use_hbd = cpi->oxcf.use_highbitdepth;
  if (!cpi->oxcf.tile_cfg.enable_large_scale_tile) {
    sf->hl_sf.high_precision_mv_usage = LAST_MV_DATA;
  }

  // Speed 0 for all speed features that give neutral coding performance change.
  sf->gm_sf.gm_disable_recode = 1;
  sf->gm_sf.gm_search_type = GM_REDUCED_REF_SEARCH_SKIP_L2_L3;

  sf->part_sf.less_rectangular_check_level = 1;
  sf->part_sf.ml_prune_4_partition = 1;
  sf->part_sf.ml_prune_ab_partition = 1;
  sf->part_sf.ml_prune_rect_partition = 1;
  sf->part_sf.prune_ext_partition_types_search_level = 1;
  sf->part_sf.simple_motion_search_prune_rect = 1;
  sf->part_sf.ml_predict_breakout_level = use_hbd ? 1 : 3;

  sf->inter_sf.disable_wedge_search_var_thresh = 0;
  // TODO(debargha): Test, tweak and turn on either 1 or 2
  sf->inter_sf.inter_mode_rd_model_estimation = 1;
  sf->inter_sf.model_based_post_interp_filter_breakout = 1;
  sf->inter_sf.prune_compound_using_single_ref = 1;
  sf->inter_sf.prune_mode_search_simple_translation = 1;
  sf->inter_sf.prune_motion_mode_level = 1;
  sf->inter_sf.prune_ref_frame_for_rect_partitions =
      (boosted || (allow_screen_content_tools))
          ? 0
          : (is_boosted_arf2_bwd_type ? 1 : 2);
  sf->inter_sf.prune_wedge_pred_diff_based = 1;
  sf->inter_sf.reduce_inter_modes = boosted ? 1 : 2;
  sf->inter_sf.selective_ref_frame = 1;
  sf->inter_sf.use_dist_wtd_comp_flag = DIST_WTD_COMP_SKIP_MV_SEARCH;

  sf->interp_sf.use_fast_interpolation_filter_search = 1;

  sf->intra_sf.intra_pruning_with_hog = 1;
  sf->intra_sf.intra_pruning_with_hog_thresh = -1.2f;

  sf->tx_sf.adaptive_txb_search_level = 1;
  sf->tx_sf.intra_tx_size_search_init_depth_sqr = 1;
  sf->tx_sf.model_based_prune_tx_search_level = 1;
  sf->tx_sf.tx_type_search.use_reduced_intra_txset = 1;

  sf->tpl_sf.search_method = NSTEP_8PT;

  sf->rt_sf.use_nonrd_pick_mode = 0;
  sf->rt_sf.use_real_time_ref_set = 0;

  if (cpi->twopass.fr_content_type == FC_GRAPHICS_ANIMATION ||
      cpi->use_screen_content_tools) {
    sf->mv_sf.exhaustive_searches_thresh = (1 << 20);
  } else {
    sf->mv_sf.exhaustive_searches_thresh = (1 << 25);
  }

  sf->rd_sf.perform_coeff_opt = 1;
  sf->hl_sf.superres_auto_search_type = SUPERRES_AUTO_DUAL;

  if (speed >= 1) {
    sf->gm_sf.gm_search_type = GM_REDUCED_REF_SEARCH_SKIP_L2_L3_ARF2;
    sf->gm_sf.prune_ref_frame_for_gm_search = boosted ? 0 : 1;

    sf->part_sf.intra_cnn_split = 1;
    sf->part_sf.simple_motion_search_early_term_none = 1;
    // TODO(Venkat): Clean-up frame type dependency for
    // simple_motion_search_split in partition search function and set the
    // speed feature accordingly
    sf->part_sf.simple_motion_search_split = allow_screen_content_tools ? 1 : 2;
    sf->part_sf.ml_predict_breakout_level = use_hbd ? 2 : 3;

    sf->mv_sf.exhaustive_searches_thresh <<= 1;
    sf->mv_sf.obmc_full_pixel_search_level = 1;
    sf->mv_sf.use_accurate_subpel_search = USE_4_TAPS;

    sf->inter_sf.disable_interinter_wedge_newmv_search = boosted ? 0 : 1;
    sf->inter_sf.prune_comp_search_by_single_result = boosted ? 2 : 1;
    sf->inter_sf.prune_comp_type_by_comp_avg = 1;
    sf->inter_sf.prune_comp_type_by_model_rd = boosted ? 0 : 1;
    sf->inter_sf.prune_motion_mode_level = 2;
    sf->inter_sf.prune_ref_frame_for_rect_partitions =
        (frame_is_intra_only(&cpi->common) || (allow_screen_content_tools))
            ? 0
            : (boosted ? 1 : 2);
    sf->inter_sf.reduce_inter_modes = boosted ? 1 : 3;
    sf->inter_sf.reuse_inter_intra_mode = 1;
    sf->inter_sf.selective_ref_frame = 2;
    sf->inter_sf.skip_repeated_newmv = 1;

    sf->interp_sf.use_interp_filter = 1;
    sf->intra_sf.prune_palette_search_level = 1;

    sf->tx_sf.adaptive_txb_search_level = 2;
    sf->tx_sf.inter_tx_size_search_init_depth_rect = 1;
    sf->tx_sf.inter_tx_size_search_init_depth_sqr = 1;
    sf->tx_sf.intra_tx_size_search_init_depth_rect = 1;
    sf->tx_sf.model_based_prune_tx_search_level = 0;
    sf->tx_sf.tx_type_search.ml_tx_split_thresh = 4000;
    sf->tx_sf.tx_type_search.prune_2d_txfm_mode = TX_TYPE_PRUNE_2;
    sf->tx_sf.tx_type_search.skip_tx_search = 1;
    sf->tx_sf.use_intra_txb_hash = 1;

    sf->rd_sf.perform_coeff_opt = boosted ? 2 : 3;
    sf->rd_sf.tx_domain_dist_level = boosted ? 1 : 2;
    sf->rd_sf.tx_domain_dist_thres_level = 1;

    sf->lpf_sf.cdef_pick_method = CDEF_FAST_SEARCH_LVL1;
    sf->lpf_sf.dual_sgr_penalty_level = 1;
    sf->lpf_sf.enable_sgr_ep_pruning = 1;

    // TODO(any, yunqing): move this feature to speed 0.
    sf->tpl_sf.skip_alike_starting_mv = 1;
  }

  if (speed >= 2) {
    sf->part_sf.allow_partition_search_skip = 1;

    sf->mv_sf.auto_mv_step_size = 1;
    sf->mv_sf.subpel_iters_per_step = 1;

    // TODO(chiyotsai@google.com): We can get 10% speed up if we move
    // adaptive_rd_thresh to speed 1. But currently it performs poorly on some
    // clips (e.g. 5% loss on dinner_1080p). We need to examine the sequence a
    // bit more closely to figure out why.
    sf->inter_sf.adaptive_rd_thresh = 1;
    sf->inter_sf.comp_inter_joint_search_thresh = BLOCK_SIZES_ALL;
    sf->inter_sf.disable_wedge_search_var_thresh = 100;
    sf->inter_sf.fast_interintra_wedge_search = 1;
    sf->inter_sf.prune_comp_search_by_single_result = boosted ? 4 : 1;
    sf->inter_sf.prune_compound_using_neighbors = 1;
    sf->inter_sf.prune_comp_type_by_comp_avg = 2;
    sf->inter_sf.reuse_best_prediction_for_part_ab = 1;
    sf->inter_sf.selective_ref_frame = 3;
    sf->inter_sf.use_dist_wtd_comp_flag = DIST_WTD_COMP_DISABLED;

    // TODO(Sachin): Enable/Enhance this speed feature for speed 2 & 3
    sf->interp_sf.adaptive_interp_filter_search = 1;
    sf->interp_sf.disable_dual_filter = 1;

    sf->intra_sf.disable_smooth_intra =
        !frame_is_intra_only(&cpi->common) || (cpi->rc.frames_to_key != 1);

    sf->rd_sf.perform_coeff_opt = is_boosted_arf2_bwd_type ? 3 : 4;

    sf->lpf_sf.prune_wiener_based_on_src_var = 1;
    sf->lpf_sf.prune_sgr_based_on_wiener = 1;

    sf->tpl_sf.prune_ref_frames_in_tpl = 1;
  }

  if (speed >= 3) {
    sf->hl_sf.high_precision_mv_usage = CURRENT_Q;
    sf->hl_sf.recode_loop = ALLOW_RECODE_KFARFGF;

    sf->gm_sf.gm_search_type = GM_DISABLE_SEARCH;

    sf->part_sf.less_rectangular_check_level = 2;
    sf->part_sf.simple_motion_search_prune_agg = 1;
    sf->part_sf.prune_4_partition_using_split_info = 1;

    // adaptive_motion_search breaks encoder multi-thread tests.
    // The values in x->pred_mv[] differ for single and multi-thread cases.
    // See aomedia:1778.
    // sf->mv_sf.adaptive_motion_search = 1;
    sf->mv_sf.full_pixel_search_level = 1;
    sf->mv_sf.simple_motion_subpel_force_stop = QUARTER_PEL;
    sf->mv_sf.subpel_search_method = SUBPEL_TREE_PRUNED;
    sf->mv_sf.search_method = DIAMOND;

    sf->inter_sf.mv_cost_upd_level = 1;
    // TODO(yunqing): evaluate this speed feature for speed 1 & 2, and combine
    // it with cpi->sf.disable_wedge_search_var_thresh.
    sf->inter_sf.disable_wedge_interintra_search = 1;
    sf->inter_sf.disable_smooth_interintra = boosted ? 0 : 1;
    // TODO(any): Experiment with the early exit mechanism for speeds 0, 1 and 2
    // and clean-up the speed feature
    sf->inter_sf.perform_best_rd_based_gating_for_chroma = 1;
    sf->inter_sf.prune_inter_modes_based_on_tpl = boosted ? 0 : 1;
    sf->inter_sf.prune_comp_search_by_single_result = boosted ? 4 : 2;
    sf->inter_sf.selective_ref_frame = 5;
    sf->inter_sf.skip_repeated_ref_mv = 1;
    sf->inter_sf.skip_repeated_full_newmv = 1;
    sf->inter_sf.reuse_compound_type_decision = 1;
    sf->inter_sf.txfm_rd_gate_level =
        boosted ? 0 : (is_boosted_arf2_bwd_type ? 1 : 2);

    sf->intra_sf.prune_palette_search_level = 2;

    sf->tpl_sf.skip_alike_starting_mv = 2;
    sf->tpl_sf.prune_intra_modes = 1;
    sf->tpl_sf.prune_starting_mv = 1;
    sf->tpl_sf.reduce_first_step_size = 6;
    sf->tpl_sf.subpel_force_stop = QUARTER_PEL;
    sf->tpl_sf.search_method = DIAMOND;

    sf->tx_sf.adaptive_txb_search_level = boosted ? 2 : 3;
    sf->tx_sf.tx_type_search.use_skip_flag_prediction = 2;

    // TODO(any): Refactor the code related to following winner mode speed
    // features
    sf->winner_mode_sf.enable_winner_mode_for_coeff_opt = 1;
    // TODO(any): Experiment with this speed feature by enabling for key frames
    sf->winner_mode_sf.enable_winner_mode_for_tx_size_srch =
        frame_is_intra_only(&cpi->common) ? 0 : 1;
    sf->winner_mode_sf.enable_winner_mode_for_use_tx_domain_dist = 1;
    sf->winner_mode_sf.motion_mode_for_winner_cand =
        boosted
            ? 0
            : gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE ? 1
                                                                         : 2;

    // TODO(any): evaluate if these lpf features can be moved to speed 2.
    // For screen content, "prune_sgr_based_on_wiener = 2" cause large quality
    // loss.
    sf->lpf_sf.prune_sgr_based_on_wiener = allow_screen_content_tools ? 1 : 2;
    sf->lpf_sf.disable_loop_restoration_chroma = boosted ? 0 : 1;
    sf->lpf_sf.reduce_wiener_window_size = !boosted;
    sf->lpf_sf.prune_wiener_based_on_src_var = 2;
  }

  if (speed >= 4) {
    sf->mv_sf.subpel_search_method = SUBPEL_TREE_PRUNED_MORE;

    sf->part_sf.simple_motion_search_prune_agg = 2;
    sf->part_sf.simple_motion_search_reduce_search_steps = 4;
    sf->part_sf.prune_ab_partition_using_split_info = 1;
    sf->part_sf.early_term_after_none_split = 1;
    sf->part_sf.ml_predict_breakout_level = 3;

    sf->inter_sf.alt_ref_search_fp = 1;
    sf->inter_sf.txfm_rd_gate_level = boosted ? 0 : 4;

    sf->inter_sf.prune_inter_modes_based_on_tpl = boosted ? 0 : 2;
    sf->inter_sf.prune_compound_using_neighbors = 2;
    sf->inter_sf.prune_comp_using_best_single_mode_ref = 2;
    sf->inter_sf.disable_smooth_interintra = 1;
    sf->inter_sf.disable_obmc = 1;
    sf->inter_sf.disable_onesided_comp = 1;

    sf->interp_sf.cb_pred_filter_search = 1;
    sf->interp_sf.skip_sharp_interp_filter_search = 1;
    sf->interp_sf.use_interp_filter = 2;
    sf->interp_sf.adaptive_interp_filter_search = 2;

    sf->intra_sf.intra_uv_mode_mask[TX_16X16] = UV_INTRA_DC_H_V_CFL;
    sf->intra_sf.intra_uv_mode_mask[TX_32X32] = UV_INTRA_DC_H_V_CFL;
    sf->intra_sf.intra_uv_mode_mask[TX_64X64] = UV_INTRA_DC_H_V_CFL;
    sf->intra_sf.intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->intra_sf.intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_sf.intra_y_mode_mask[TX_64X64] = INTRA_DC_H_V;
    // TODO(any): Experiment with this speed feature set to 2 for higher quality
    // presets as well
    sf->intra_sf.skip_intra_in_interframe = 2;

    sf->mv_sf.simple_motion_subpel_force_stop = HALF_PEL;

    sf->tpl_sf.prune_starting_mv = 2;
    sf->tpl_sf.subpel_force_stop = HALF_PEL;
    sf->tpl_sf.search_method = FAST_BIGDIA;

    sf->tx_sf.tx_type_search.winner_mode_tx_type_pruning = 1;
    sf->tx_sf.tx_type_search.fast_intra_tx_type_search = 1;
    sf->tx_sf.tx_type_search.prune_2d_txfm_mode = TX_TYPE_PRUNE_3;
    sf->tx_sf.tx_type_search.prune_tx_type_est_rd = 1;
    // TODO(any): Experiment with enabling of this speed feature as hash state
    // is reset during winner mode processing
    sf->tx_sf.use_intra_txb_hash = 0;

    sf->rd_sf.perform_coeff_opt = is_boosted_arf2_bwd_type ? 3 : 5;
    sf->rd_sf.perform_coeff_opt_based_on_satd =
        is_boosted_arf2_bwd_type ? 1 : 2;
    sf->rd_sf.tx_domain_dist_thres_level = 2;

    // TODO(any): Extend multi-winner mode processing support for inter frames
    sf->winner_mode_sf.multi_winner_mode_type =
        frame_is_intra_only(&cpi->common) ? MULTI_WINNER_MODE_DEFAULT
                                          : MULTI_WINNER_MODE_OFF;
    sf->winner_mode_sf.enable_winner_mode_for_tx_size_srch = 1;

    sf->lpf_sf.lpf_pick = LPF_PICK_FROM_FULL_IMAGE_NON_DUAL;
    sf->lpf_sf.cdef_pick_method = CDEF_FAST_SEARCH_LVL3;

    sf->mv_sf.reduce_search_range = 1;
  }

  if (speed >= 5) {
    sf->part_sf.simple_motion_search_prune_agg = 3;
    sf->part_sf.ext_partition_eval_thresh =
        allow_screen_content_tools ? BLOCK_8X8 : BLOCK_16X16;

    sf->inter_sf.disable_interinter_wedge = 1;
    sf->inter_sf.prune_inter_modes_if_skippable = 1;
    sf->inter_sf.txfm_rd_gate_level = boosted ? 0 : 5;

    // TODO(any): Extend multi-winner mode processing support for inter frames
    sf->winner_mode_sf.multi_winner_mode_type =
        frame_is_intra_only(&cpi->common) ? MULTI_WINNER_MODE_FAST
                                          : MULTI_WINNER_MODE_OFF;

    sf->lpf_sf.use_coarse_filter_level_search =
        frame_is_intra_only(&cpi->common) ? 0 : 1;
    sf->lpf_sf.disable_lr_filter = 1;

    sf->mv_sf.prune_mesh_search = 1;

    sf->tpl_sf.prune_starting_mv = 3;

    sf->winner_mode_sf.dc_blk_pred_level = 1;
  }

  if (speed >= 6) {
    sf->hl_sf.disable_extra_sc_testing = 1;

    sf->inter_sf.prune_inter_modes_based_on_tpl = boosted ? 0 : 3;
    sf->inter_sf.prune_nearmv_using_neighbors = 1;

    sf->part_sf.prune_rectangular_split_based_on_qidx =
        boosted || allow_screen_content_tools ? 0 : 1;
    sf->part_sf.prune_sub_8x8_partition_level =
        allow_screen_content_tools ? 0
                                   : frame_is_intra_only(&cpi->common) ? 1 : 2;

    sf->mv_sf.simple_motion_subpel_force_stop = FULL_PEL;
    sf->mv_sf.use_bsize_dependent_search_method = 1;

    sf->tpl_sf.disable_gop_length_decision = 1;
    sf->tpl_sf.subpel_force_stop = FULL_PEL;
    sf->tpl_sf.disable_filtered_key_tpl = 1;

    sf->tx_sf.tx_type_search.winner_mode_tx_type_pruning = 2;
    sf->tx_sf.use_intra_txb_hash = 1;
    sf->tx_sf.tx_type_search.prune_tx_type_est_rd = 0;

    sf->rd_sf.perform_coeff_opt = is_boosted_arf2_bwd_type ? 4 : 6;

    sf->winner_mode_sf.dc_blk_pred_level = 2;
    sf->winner_mode_sf.multi_winner_mode_type = MULTI_WINNER_MODE_OFF;

    sf->lpf_sf.cdef_pick_method = CDEF_FAST_SEARCH_LVL4;
  }

  // Intra txb hash is currently not compatible with multi-winner mode as the
  // hashes got reset during multi-winner mode processing.
  assert(IMPLIES(
      sf->winner_mode_sf.multi_winner_mode_type != MULTI_WINNER_MODE_OFF,
      !sf->tx_sf.use_intra_txb_hash));
}

// TODO(kyslov): now this is very similar to
// set_good_speed_features_framesize_independent
//               except it sets non-rd flag on speed8. This function will likely
//               be modified in the future with RT-specific speed features
static void set_rt_speed_features_framesize_independent(AV1_COMP *cpi,
                                                        SPEED_FEATURES *sf,
                                                        int speed) {
  AV1_COMMON *const cm = &cpi->common;
  const int boosted = frame_is_boosted(cpi);

  // Speed 0 for all speed features that give neutral coding performance change.
  sf->gm_sf.gm_disable_recode = 1;
  sf->gm_sf.gm_search_type = GM_REDUCED_REF_SEARCH_SKIP_L2_L3;

  sf->part_sf.less_rectangular_check_level = 1;
  sf->part_sf.ml_prune_4_partition = 1;
  sf->part_sf.ml_prune_ab_partition = 1;
  sf->part_sf.ml_prune_rect_partition = 1;
  sf->part_sf.prune_ext_partition_types_search_level = 1;

  // TODO(debargha): Test, tweak and turn on either 1 or 2
  sf->inter_sf.inter_mode_rd_model_estimation = 0;
  sf->inter_sf.disable_wedge_search_var_thresh = 0;
  sf->inter_sf.model_based_post_interp_filter_breakout = 1;
  sf->inter_sf.prune_compound_using_single_ref = 0;
  sf->inter_sf.prune_mode_search_simple_translation = 1;
  sf->inter_sf.prune_motion_mode_level = 1;
  sf->inter_sf.prune_ref_frame_for_rect_partitions = !boosted;
  sf->inter_sf.prune_wedge_pred_diff_based = 1;
  sf->inter_sf.reduce_inter_modes = 1;
  sf->inter_sf.selective_ref_frame = 1;
  sf->inter_sf.use_dist_wtd_comp_flag = DIST_WTD_COMP_SKIP_MV_SEARCH;

  sf->interp_sf.use_fast_interpolation_filter_search = 1;

  sf->intra_sf.intra_pruning_with_hog = 1;
  sf->intra_sf.intra_pruning_with_hog_thresh = -1.2f;

  sf->mv_sf.full_pixel_search_level = 1;
  sf->mv_sf.exhaustive_searches_thresh = INT_MAX;

  sf->rt_sf.check_intra_pred_nonrd = 1;
  sf->rt_sf.estimate_motion_for_var_based_partition = 1;
  sf->rt_sf.hybrid_intra_pickmode = 0;
  sf->rt_sf.nonrd_prune_ref_frame_search = 0;
  sf->rt_sf.reuse_inter_pred_nonrd = 0;
  sf->rt_sf.use_comp_ref_nonrd = 1;
  sf->rt_sf.use_nonrd_filter_search = 1;
  sf->rt_sf.use_nonrd_pick_mode = 0;
  sf->rt_sf.use_real_time_ref_set = 0;
  sf->rt_sf.check_scene_detection = 0;
  sf->rt_sf.overshoot_detection_cbr = NO_DETECTION;
  sf->tx_sf.adaptive_txb_search_level = 1;
  sf->tx_sf.intra_tx_size_search_init_depth_sqr = 1;
  sf->tx_sf.model_based_prune_tx_search_level = 1;
  sf->tx_sf.tx_type_search.use_reduced_intra_txset = 1;
  sf->rt_sf.fullpel_search_step_param = 0;
  sf->rt_sf.skip_loopfilter_non_reference = 0;

  sf->hl_sf.superres_auto_search_type = SUPERRES_AUTO_SOLO;

  if (speed >= 1) {
    sf->gm_sf.gm_search_type = GM_REDUCED_REF_SEARCH_SKIP_L2_L3_ARF2;

    sf->part_sf.prune_ext_partition_types_search_level = 2;
    sf->part_sf.simple_motion_search_prune_rect = 1;

    sf->mv_sf.obmc_full_pixel_search_level = 1;
    sf->mv_sf.use_accurate_subpel_search = USE_4_TAPS;

    sf->inter_sf.prune_comp_search_by_single_result = 1;
    sf->inter_sf.reuse_inter_intra_mode = 1;
    sf->inter_sf.selective_ref_frame = 2;
    sf->inter_sf.skip_repeated_newmv = 1;
    sf->inter_sf.disable_wedge_search_var_thresh = 0;
    sf->inter_sf.prune_comp_type_by_comp_avg = 1;
    sf->inter_sf.prune_motion_mode_level = 2;

    sf->interp_sf.cb_pred_filter_search = 1;
    sf->interp_sf.use_interp_filter = 1;

    sf->tx_sf.adaptive_txb_search_level = 2;
    sf->tx_sf.intra_tx_size_search_init_depth_rect = 1;
    sf->tx_sf.tx_size_search_lgr_block = 1;
    sf->tx_sf.tx_type_search.ml_tx_split_thresh = 4000;
    sf->tx_sf.tx_type_search.skip_tx_search = 1;
    sf->tx_sf.use_intra_txb_hash = 1;

    sf->rd_sf.tx_domain_dist_level = boosted ? 0 : 1;
    sf->rd_sf.tx_domain_dist_thres_level = 1;

    sf->lpf_sf.dual_sgr_penalty_level = 1;
  }

  if (speed >= 2) {
    sf->part_sf.allow_partition_search_skip = 1;
    sf->part_sf.partition_search_breakout_rate_thr = 80;

    sf->mv_sf.auto_mv_step_size = 1;
    sf->mv_sf.subpel_iters_per_step = 1;

    sf->inter_sf.adaptive_rd_thresh = 1;
    sf->inter_sf.comp_inter_joint_search_thresh = BLOCK_SIZES_ALL;
    sf->inter_sf.disable_wedge_search_var_thresh = 100;
    sf->inter_sf.fast_wedge_sign_estimate = 1;
    sf->inter_sf.prune_comp_type_by_comp_avg = 2;
    sf->inter_sf.selective_ref_frame = 3;
    sf->inter_sf.use_dist_wtd_comp_flag = DIST_WTD_COMP_DISABLED;

    sf->interp_sf.adaptive_interp_filter_search = 1;
    sf->interp_sf.cb_pred_filter_search = 0;
    sf->interp_sf.disable_dual_filter = 1;

    sf->tx_sf.inter_tx_size_search_init_depth_rect = 1;
    sf->tx_sf.inter_tx_size_search_init_depth_sqr = 1;
    sf->tx_sf.model_based_prune_tx_search_level = 0;

    sf->lpf_sf.cdef_pick_method = CDEF_FAST_SEARCH_LVL1;
  }

  if (speed >= 3) {
    sf->hl_sf.recode_loop = ALLOW_RECODE_KFARFGF;

    sf->gm_sf.gm_search_type = GM_DISABLE_SEARCH;

    sf->part_sf.less_rectangular_check_level = 2;

    sf->mv_sf.use_accurate_subpel_search = USE_2_TAPS;
    // adaptive_motion_search breaks encoder multi-thread tests.
    // The values in x->pred_mv[] differ for single and multi-thread cases.
    // See aomedia:1778.
    // sf->mv_sf.adaptive_motion_search = 1;

    sf->inter_sf.adaptive_rd_thresh = 2;
    sf->inter_sf.mv_cost_upd_level = 1;
    // TODO(yunqing): evaluate this speed feature for speed 1 & 2, and combine
    // it with cpi->sf.disable_wedge_search_var_thresh.
    sf->inter_sf.disable_wedge_interintra_search = 1;
    sf->inter_sf.prune_comp_search_by_single_result = 2;
    sf->inter_sf.prune_motion_mode_level = boosted ? 2 : 3;
    sf->inter_sf.selective_ref_frame = 4;

    sf->tx_sf.tx_type_search.prune_2d_txfm_mode = TX_TYPE_PRUNE_2;

    sf->rd_sf.tx_domain_dist_level = 1;

    sf->winner_mode_sf.tx_size_search_level = boosted ? 0 : 2;
  }

  if (speed >= 4) {
    sf->mv_sf.subpel_search_method = SUBPEL_TREE_PRUNED;

    sf->inter_sf.alt_ref_search_fp = 1;

    sf->interp_sf.skip_sharp_interp_filter_search = 1;

    sf->tx_sf.tx_type_search.fast_inter_tx_type_search = 1;
    sf->tx_sf.tx_type_search.fast_intra_tx_type_search = 1;
    sf->tx_sf.use_intra_txb_hash = 0;

    sf->rd_sf.use_mb_rd_hash = 0;

    sf->winner_mode_sf.tx_size_search_level = frame_is_intra_only(cm) ? 0 : 2;
  }

  if (speed >= 5) {
    sf->hl_sf.recode_loop = ALLOW_RECODE_KFMAXBW;

    sf->inter_sf.adaptive_rd_thresh = 4;

    sf->rd_sf.tx_domain_dist_level = 2;
    sf->rd_sf.tx_domain_dist_thres_level = 2;
    sf->winner_mode_sf.tx_size_search_level = 1;

    sf->rt_sf.mode_search_skip_flags =
        (cm->current_frame.frame_type == KEY_FRAME)
            ? 0
            : FLAG_SKIP_INTRA_DIRMISMATCH | FLAG_SKIP_INTRA_BESTINTER |
                  FLAG_SKIP_COMP_BESTINTRA | FLAG_SKIP_INTRA_LOWVAR |
                  FLAG_EARLY_TERMINATE;
    sf->hl_sf.frame_parameter_update = 0;

    sf->part_sf.default_max_partition_size = BLOCK_128X128;
    sf->part_sf.default_min_partition_size = BLOCK_8X8;
    sf->part_sf.max_intra_bsize = BLOCK_32X32;
    sf->part_sf.partition_search_breakout_rate_thr = 500;
    sf->part_sf.partition_search_type = VAR_BASED_PARTITION;
    sf->part_sf.adjust_var_based_rd_partitioning = 2;

    sf->mv_sf.search_method = FAST_DIAMOND;
    sf->mv_sf.subpel_force_stop = QUARTER_PEL;
    sf->mv_sf.use_fullpel_costlist = 1;
    sf->mv_sf.subpel_search_method = SUBPEL_TREE_PRUNED_MORE;

    sf->inter_sf.inter_mode_rd_model_estimation = 2;

    for (int i = 0; i < TX_SIZES; ++i) {
      sf->intra_sf.intra_y_mode_mask[i] = INTRA_DC;
      sf->intra_sf.intra_uv_mode_mask[i] = UV_INTRA_DC_CFL;
    }

    sf->tx_sf.tx_type_search.prune_2d_txfm_mode = TX_TYPE_PRUNE_3;
    sf->tx_sf.use_inter_txb_hash = 0;
    sf->tx_sf.refine_fast_tx_search_results = 0;

    sf->rd_sf.optimize_coefficients = NO_TRELLIS_OPT;
    sf->rd_sf.simple_model_rd_from_var = 1;

    sf->lpf_sf.cdef_pick_method = CDEF_PICK_FROM_Q;
    sf->lpf_sf.lpf_pick = LPF_PICK_FROM_Q;

    sf->rt_sf.mode_search_skip_flags |= FLAG_SKIP_INTRA_DIRMISMATCH;
    sf->rt_sf.num_inter_modes_for_tx_search = 5;
    sf->rt_sf.skip_interp_filter_search = 1;
    sf->rt_sf.use_comp_ref_nonrd = 0;
    sf->rt_sf.use_real_time_ref_set = 1;
    sf->rt_sf.use_simple_rd_model = 1;

    if (cpi->oxcf.rc_cfg.mode == AOM_CBR) {
      sf->rt_sf.check_scene_detection = 1;
      if (cm->current_frame.frame_type != KEY_FRAME)
        sf->rt_sf.overshoot_detection_cbr = FAST_DETECTION_MAXQ;
    }
    // Keeping this off for now as some clips show ~6% BDRate regression with
    // moderate speed-up (~20%)
    sf->rt_sf.use_temporal_noise_estimate = 0;
  }

  if (speed >= 6) {
    sf->part_sf.adjust_var_based_rd_partitioning = 1;
  }

  if (speed >= 7) {
    sf->part_sf.default_max_partition_size = BLOCK_128X128;
    sf->part_sf.default_min_partition_size = BLOCK_8X8;
    sf->part_sf.partition_search_type = VAR_BASED_PARTITION;

    sf->mv_sf.search_method = FAST_DIAMOND;
    sf->mv_sf.subpel_force_stop = QUARTER_PEL;
    sf->mv_sf.subpel_search_method = SUBPEL_TREE_PRUNED;

    sf->inter_sf.inter_mode_rd_model_estimation = 2;

    sf->lpf_sf.cdef_pick_method = CDEF_PICK_FROM_Q;
    sf->lpf_sf.lpf_pick = LPF_PICK_FROM_Q;

    sf->rt_sf.mode_search_skip_flags |= FLAG_SKIP_INTRA_DIRMISMATCH;
    sf->rt_sf.nonrd_prune_ref_frame_search = 1;
    sf->rt_sf.reuse_inter_pred_nonrd = 0;
    sf->rt_sf.short_circuit_low_temp_var = 0;
    sf->rt_sf.skip_interp_filter_search = 0;
    sf->rt_sf.use_comp_ref_nonrd = 0;
    // For spatial layers, only LAST and GOLDEN are currently used in the SVC
    // for nonrd. The flag use_nonrd_altref_frame can disable GOLDEN in the
    // get_ref_frame_flags() for some patterns, so disable it here for
    // spatial layers.
    sf->rt_sf.use_nonrd_altref_frame =
        (cpi->svc.number_spatial_layers > 1) ? 0 : 1;
    sf->rt_sf.use_nonrd_pick_mode = 1;
    sf->rt_sf.nonrd_check_partition_merge_mode = 1;
    sf->rt_sf.nonrd_check_partition_split = 0;
    sf->rt_sf.hybrid_intra_pickmode = 1;
    sf->rt_sf.skip_intra_pred_if_tx_skip = 1;
    // For SVC: use better mv search on base temporal layer, and only
    // on base spatial layer if highest resolution is above 640x360.
    if (cpi->svc.number_temporal_layers > 1) {
      if (cpi->svc.temporal_layer_id == 0 &&
          (cpi->svc.spatial_layer_id == 0 ||
           cpi->oxcf.frm_dim_cfg.width * cpi->oxcf.frm_dim_cfg.height <=
               640 * 360)) {
        sf->mv_sf.search_method = NSTEP;
        sf->mv_sf.subpel_search_method = SUBPEL_TREE;
        sf->rt_sf.fullpel_search_step_param = 6;
      } else if (cpi->svc.non_reference_frame) {
        sf->mv_sf.subpel_search_method = SUBPEL_TREE_PRUNED_MORE;
        sf->rt_sf.fullpel_search_step_param = 10;
      }
    }
    // TODO(marpan): Look into why enabling skip_loopfilter_non_reference is
    // not bitexact on rtc testset, its very close (< ~0.01 bdrate), but not
    // always bitexact.
    if (cpi->use_svc && cpi->svc.non_reference_frame &&
        sf->lpf_sf.cdef_pick_method == CDEF_PICK_FROM_Q &&
        sf->lpf_sf.lpf_pick == LPF_PICK_FROM_Q)
      sf->rt_sf.skip_loopfilter_non_reference = 1;
    // Set mask for intra modes.
    for (int i = 0; i < BLOCK_SIZES; ++i)
      if (i >= BLOCK_32X32)
        sf->rt_sf.intra_y_mode_bsize_mask_nrd[i] = INTRA_DC;
      else
        // Use DC, H, V intra mode for block sizes < 32X32.
        sf->rt_sf.intra_y_mode_bsize_mask_nrd[i] = INTRA_DC_H_V;
  }

  if (speed >= 8) {
    sf->rt_sf.estimate_motion_for_var_based_partition = 0;
    sf->rt_sf.short_circuit_low_temp_var = 1;
    sf->rt_sf.reuse_inter_pred_nonrd = 1;
    sf->rt_sf.use_nonrd_altref_frame = 0;
    sf->rt_sf.nonrd_prune_ref_frame_search = 2;
    sf->rt_sf.nonrd_check_partition_merge_mode = 0;
    sf->rt_sf.nonrd_check_partition_split = 0;
    sf->rt_sf.use_modeled_non_rd_cost = 1;
    sf->rt_sf.source_metrics_sb_nonrd = 1;
    sf->rt_sf.skip_intra_pred_if_tx_skip = 0;
    sf->interp_sf.cb_pred_filter_search = 1;
  }
  if (speed >= 9) {
    sf->rt_sf.force_large_partition_blocks = 1;
    for (int i = 0; i < BLOCK_SIZES; ++i)
      sf->rt_sf.intra_y_mode_bsize_mask_nrd[i] = INTRA_DC;
  }
}

static AOM_INLINE void init_hl_sf(HIGH_LEVEL_SPEED_FEATURES *hl_sf) {
  // best quality defaults
  hl_sf->frame_parameter_update = 1;
  hl_sf->recode_loop = ALLOW_RECODE;
  // Recode loop tolerance %.
  hl_sf->recode_tolerance = 25;
  hl_sf->high_precision_mv_usage = CURRENT_Q;
  hl_sf->superres_auto_search_type = SUPERRES_AUTO_ALL;
  hl_sf->disable_extra_sc_testing = 0;
}

static AOM_INLINE void init_tpl_sf(TPL_SPEED_FEATURES *tpl_sf) {
  tpl_sf->disable_gop_length_decision = 0;
  tpl_sf->prune_intra_modes = 0;
  tpl_sf->prune_starting_mv = 0;
  tpl_sf->reduce_first_step_size = 0;
  tpl_sf->skip_alike_starting_mv = 0;
  tpl_sf->subpel_force_stop = EIGHTH_PEL;
  tpl_sf->search_method = NSTEP;
  tpl_sf->disable_filtered_key_tpl = 0;
  tpl_sf->prune_ref_frames_in_tpl = 0;
}

static AOM_INLINE void init_gm_sf(GLOBAL_MOTION_SPEED_FEATURES *gm_sf) {
  gm_sf->selective_ref_gm = 1;
  gm_sf->gm_search_type = GM_FULL_SEARCH;
  gm_sf->gm_disable_recode = 0;
  gm_sf->prune_ref_frame_for_gm_search = 0;
}

static AOM_INLINE void init_part_sf(PARTITION_SPEED_FEATURES *part_sf) {
  part_sf->partition_search_type = SEARCH_PARTITION;
  part_sf->less_rectangular_check_level = 0;
  part_sf->use_square_partition_only_threshold = BLOCK_128X128;
  part_sf->auto_max_partition_based_on_simple_motion = NOT_IN_USE;
  part_sf->auto_min_partition_based_on_simple_motion = 0;
  part_sf->default_max_partition_size = BLOCK_LARGEST;
  part_sf->default_min_partition_size = BLOCK_4X4;
  part_sf->adjust_var_based_rd_partitioning = 0;
  part_sf->allow_partition_search_skip = 0;
  part_sf->max_intra_bsize = BLOCK_LARGEST;
  // This setting only takes effect when partition_search_type is set
  // to FIXED_PARTITION.
  part_sf->fixed_partition_size = BLOCK_16X16;
  // Recode loop tolerance %.
  part_sf->partition_search_breakout_dist_thr = 0;
  part_sf->partition_search_breakout_rate_thr = 0;
  part_sf->prune_ext_partition_types_search_level = 0;
  part_sf->ml_prune_rect_partition = 0;
  part_sf->ml_prune_ab_partition = 0;
  part_sf->ml_prune_4_partition = 0;
  part_sf->ml_early_term_after_part_split_level = 0;
  for (int i = 0; i < PARTITION_BLOCK_SIZES; ++i) {
    part_sf->ml_partition_search_breakout_thresh[i] =
        -1;  // -1 means not enabled.
  }
  part_sf->simple_motion_search_prune_agg = 0;
  part_sf->simple_motion_search_split = 0;
  part_sf->simple_motion_search_prune_rect = 0;
  part_sf->simple_motion_search_early_term_none = 0;
  part_sf->simple_motion_search_reduce_search_steps = 0;
  part_sf->intra_cnn_split = 0;
  part_sf->ext_partition_eval_thresh = BLOCK_8X8;
  part_sf->prune_4_partition_using_split_info = 0;
  part_sf->prune_ab_partition_using_split_info = 0;
  part_sf->prune_rectangular_split_based_on_qidx = 0;
  part_sf->early_term_after_none_split = 0;
  part_sf->ml_predict_breakout_level = 0;
  part_sf->prune_sub_8x8_partition_level = 0;
}

static AOM_INLINE void init_mv_sf(MV_SPEED_FEATURES *mv_sf) {
  mv_sf->full_pixel_search_level = 0;
  mv_sf->auto_mv_step_size = 0;
  mv_sf->exhaustive_searches_thresh = 0;
  mv_sf->obmc_full_pixel_search_level = 0;
  mv_sf->prune_mesh_search = 0;
  mv_sf->reduce_search_range = 0;
  mv_sf->search_method = NSTEP;
  mv_sf->simple_motion_subpel_force_stop = EIGHTH_PEL;
  mv_sf->subpel_force_stop = EIGHTH_PEL;
  mv_sf->subpel_iters_per_step = 2;
  mv_sf->subpel_search_method = SUBPEL_TREE;
  mv_sf->use_accurate_subpel_search = USE_8_TAPS;
  mv_sf->use_bsize_dependent_search_method = 0;
  mv_sf->use_fullpel_costlist = 0;
  mv_sf->use_downsampled_sad = 0;
}

static AOM_INLINE void init_inter_sf(INTER_MODE_SPEED_FEATURES *inter_sf) {
  inter_sf->comp_inter_joint_search_thresh = BLOCK_4X4;
  inter_sf->adaptive_rd_thresh = 0;
  inter_sf->model_based_post_interp_filter_breakout = 0;
  inter_sf->reduce_inter_modes = 0;
  inter_sf->alt_ref_search_fp = 0;
  inter_sf->selective_ref_frame = 0;
  inter_sf->prune_ref_frame_for_rect_partitions = 0;
  inter_sf->disable_wedge_search_var_thresh = 0;
  inter_sf->fast_wedge_sign_estimate = 0;
  inter_sf->prune_wedge_pred_diff_based = 0;
  inter_sf->use_dist_wtd_comp_flag = DIST_WTD_COMP_ENABLED;
  inter_sf->reuse_inter_intra_mode = 0;
  inter_sf->disable_sb_level_coeff_cost_upd = 0;
  inter_sf->mv_cost_upd_level = 0;
  inter_sf->prune_inter_modes_based_on_tpl = 0;
  inter_sf->prune_nearmv_using_neighbors = 0;
  inter_sf->prune_comp_search_by_single_result = 0;
  inter_sf->skip_repeated_ref_mv = 0;
  inter_sf->skip_repeated_newmv = 0;
  inter_sf->skip_repeated_full_newmv = 0;
  inter_sf->inter_mode_rd_model_estimation = 0;
  inter_sf->prune_compound_using_single_ref = 0;
  inter_sf->prune_compound_using_neighbors = 0;
  inter_sf->prune_comp_using_best_single_mode_ref = 0;
  inter_sf->disable_onesided_comp = 0;
  inter_sf->prune_mode_search_simple_translation = 0;
  inter_sf->prune_comp_type_by_comp_avg = 0;
  inter_sf->disable_interinter_wedge_newmv_search = 0;
  inter_sf->enable_interinter_diffwtd_newmv_search = 0;
  inter_sf->disable_smooth_interintra = 0;
  inter_sf->prune_motion_mode_level = 0;
  inter_sf->disable_wedge_interintra_search = 0;
  inter_sf->fast_interintra_wedge_search = 0;
  inter_sf->prune_comp_type_by_model_rd = 0;
  inter_sf->perform_best_rd_based_gating_for_chroma = 0;
  inter_sf->prune_obmc_prob_thresh = 0;
  inter_sf->disable_obmc = 0;
  inter_sf->disable_interinter_wedge = 0;
  inter_sf->prune_ref_mv_idx_search = 0;
  inter_sf->prune_warped_prob_thresh = 0;
  inter_sf->reuse_compound_type_decision = 0;
  inter_sf->txfm_rd_gate_level = 0;
  inter_sf->prune_inter_modes_if_skippable = 0;
  inter_sf->disable_masked_comp = 0;
  inter_sf->reuse_best_prediction_for_part_ab = 0;
}

static AOM_INLINE void init_interp_sf(INTERP_FILTER_SPEED_FEATURES *interp_sf) {
  interp_sf->adaptive_interp_filter_search = 0;
  interp_sf->cb_pred_filter_search = 0;
  interp_sf->disable_dual_filter = 0;
  interp_sf->skip_sharp_interp_filter_search = 0;
  interp_sf->use_fast_interpolation_filter_search = 0;
  interp_sf->use_interp_filter = 0;
}

static AOM_INLINE void init_intra_sf(INTRA_MODE_SPEED_FEATURES *intra_sf) {
  intra_sf->skip_intra_in_interframe = 1;
  intra_sf->intra_pruning_with_hog = 0;
  intra_sf->src_var_thresh_intra_skip = 1;
  intra_sf->prune_palette_search_level = 0;

  for (int i = 0; i < TX_SIZES; i++) {
    intra_sf->intra_y_mode_mask[i] = INTRA_ALL;
    intra_sf->intra_uv_mode_mask[i] = UV_INTRA_ALL;
  }
  intra_sf->disable_smooth_intra = 0;
}

static AOM_INLINE void init_tx_sf(TX_SPEED_FEATURES *tx_sf) {
  tx_sf->inter_tx_size_search_init_depth_sqr = 0;
  tx_sf->inter_tx_size_search_init_depth_rect = 0;
  tx_sf->intra_tx_size_search_init_depth_rect = 0;
  tx_sf->intra_tx_size_search_init_depth_sqr = 0;
  tx_sf->tx_size_search_lgr_block = 0;
  tx_sf->model_based_prune_tx_search_level = 0;
  tx_sf->tx_type_search.prune_2d_txfm_mode = TX_TYPE_PRUNE_1;
  tx_sf->tx_type_search.ml_tx_split_thresh = 8500;
  tx_sf->tx_type_search.use_skip_flag_prediction = 1;
  tx_sf->tx_type_search.use_reduced_intra_txset = 0;
  tx_sf->tx_type_search.fast_intra_tx_type_search = 0;
  tx_sf->tx_type_search.fast_inter_tx_type_search = 0;
  tx_sf->tx_type_search.skip_tx_search = 0;
  tx_sf->tx_type_search.prune_tx_type_using_stats = 0;
  tx_sf->tx_type_search.prune_tx_type_est_rd = 0;
  tx_sf->tx_type_search.winner_mode_tx_type_pruning = 0;
  tx_sf->txb_split_cap = 1;
  tx_sf->adaptive_txb_search_level = 0;
  tx_sf->use_intra_txb_hash = 0;
  tx_sf->use_inter_txb_hash = 1;
  tx_sf->refine_fast_tx_search_results = 1;
  tx_sf->prune_tx_size_level = 0;
}

static AOM_INLINE void init_rd_sf(RD_CALC_SPEED_FEATURES *rd_sf,
                                  const AV1EncoderConfig *oxcf) {
  const int disable_trellis_quant = oxcf->algo_cfg.disable_trellis_quant;
  if (disable_trellis_quant == 3) {
    rd_sf->optimize_coefficients = !is_lossless_requested(&oxcf->rc_cfg)
                                       ? NO_ESTIMATE_YRD_TRELLIS_OPT
                                       : NO_TRELLIS_OPT;
  } else if (disable_trellis_quant == 2) {
    rd_sf->optimize_coefficients = !is_lossless_requested(&oxcf->rc_cfg)
                                       ? FINAL_PASS_TRELLIS_OPT
                                       : NO_TRELLIS_OPT;
  } else if (disable_trellis_quant == 0) {
    if (is_lossless_requested(&oxcf->rc_cfg)) {
      rd_sf->optimize_coefficients = NO_TRELLIS_OPT;
    } else {
      rd_sf->optimize_coefficients = FULL_TRELLIS_OPT;
    }
  } else if (disable_trellis_quant == 1) {
    rd_sf->optimize_coefficients = NO_TRELLIS_OPT;
  } else {
    assert(0 && "Invalid disable_trellis_quant value");
  }
  rd_sf->use_mb_rd_hash = 1;
  rd_sf->simple_model_rd_from_var = 0;
  rd_sf->tx_domain_dist_level = 0;
  rd_sf->tx_domain_dist_thres_level = 0;
  rd_sf->perform_coeff_opt = 0;
  rd_sf->perform_coeff_opt_based_on_satd = 0;
}

static AOM_INLINE void init_winner_mode_sf(
    WINNER_MODE_SPEED_FEATURES *winner_mode_sf) {
  winner_mode_sf->motion_mode_for_winner_cand = 0;
  // Set this at the appropriate speed levels
  winner_mode_sf->tx_size_search_level = USE_FULL_RD;
  winner_mode_sf->enable_winner_mode_for_coeff_opt = 0;
  winner_mode_sf->enable_winner_mode_for_tx_size_srch = 0;
  winner_mode_sf->enable_winner_mode_for_use_tx_domain_dist = 0;
  winner_mode_sf->multi_winner_mode_type = 0;
  winner_mode_sf->dc_blk_pred_level = 0;
}

static AOM_INLINE void init_lpf_sf(LOOP_FILTER_SPEED_FEATURES *lpf_sf) {
  lpf_sf->disable_loop_restoration_chroma = 0;
  lpf_sf->prune_wiener_based_on_src_var = 0;
  lpf_sf->prune_sgr_based_on_wiener = 0;
  lpf_sf->enable_sgr_ep_pruning = 0;
  lpf_sf->reduce_wiener_window_size = 0;
  lpf_sf->lpf_pick = LPF_PICK_FROM_FULL_IMAGE;
  lpf_sf->use_coarse_filter_level_search = 0;
  lpf_sf->cdef_pick_method = CDEF_FULL_SEARCH;
  // Set decoder side speed feature to use less dual sgr modes
  lpf_sf->dual_sgr_penalty_level = 0;
  lpf_sf->disable_lr_filter = 0;
}

static AOM_INLINE void init_rt_sf(REAL_TIME_SPEED_FEATURES *rt_sf) {
  rt_sf->mode_search_skip_flags = 0;
  rt_sf->skip_interp_filter_search = 0;
  rt_sf->force_tx_search_off = 0;
  rt_sf->num_inter_modes_for_tx_search = INT_MAX;
  rt_sf->use_simple_rd_model = 0;
  rt_sf->nonrd_check_partition_merge_mode = 0;
  rt_sf->nonrd_check_partition_split = 0;
  rt_sf->skip_intra_pred_if_tx_skip = 0;
}

void av1_set_speed_features_framesize_dependent(AV1_COMP *cpi, int speed) {
  SPEED_FEATURES *const sf = &cpi->sf;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  if (oxcf->mode == GOOD) {
    set_good_speed_feature_framesize_dependent(cpi, sf, speed);
  } else if (oxcf->mode == REALTIME) {
    set_rt_speed_feature_framesize_dependent(cpi, sf, speed);
  }

  // This is only used in motion vector unit test.
  if (cpi->oxcf.unit_test_cfg.motion_vector_unit_test == 1)
    cpi->mv_search_params.find_fractional_mv_step = av1_return_max_sub_pixel_mv;
  else if (cpi->oxcf.unit_test_cfg.motion_vector_unit_test == 2)
    cpi->mv_search_params.find_fractional_mv_step = av1_return_min_sub_pixel_mv;
}

void av1_set_speed_features_framesize_independent(AV1_COMP *cpi, int speed) {
  SPEED_FEATURES *const sf = &cpi->sf;
  WinnerModeParams *const winner_mode_params = &cpi->winner_mode_params;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  int i;

  init_hl_sf(&sf->hl_sf);
  init_tpl_sf(&sf->tpl_sf);
  init_gm_sf(&sf->gm_sf);
  init_part_sf(&sf->part_sf);
  init_mv_sf(&sf->mv_sf);
  init_inter_sf(&sf->inter_sf);
  init_interp_sf(&sf->interp_sf);
  init_intra_sf(&sf->intra_sf);
  init_tx_sf(&sf->tx_sf);
  init_rd_sf(&sf->rd_sf, oxcf);
  init_winner_mode_sf(&sf->winner_mode_sf);
  init_lpf_sf(&sf->lpf_sf);
  init_rt_sf(&sf->rt_sf);

  if (oxcf->mode == GOOD)
    set_good_speed_features_framesize_independent(cpi, sf, speed);
  else if (oxcf->mode == REALTIME)
    set_rt_speed_features_framesize_independent(cpi, sf, speed);

  if (!cpi->seq_params_locked) {
    cpi->common.seq_params.enable_dual_filter &=
        !sf->interp_sf.disable_dual_filter;
    cpi->common.seq_params.enable_restoration &= !sf->lpf_sf.disable_lr_filter;

    cpi->common.seq_params.enable_masked_compound &=
        !sf->inter_sf.disable_masked_comp;
    cpi->common.seq_params.enable_interintra_compound &=
        !sf->inter_sf.disable_wedge_interintra_search;
  }

  // sf->part_sf.partition_search_breakout_dist_thr is set assuming max 64x64
  // blocks. Normalise this if the blocks are bigger.
  if (MAX_SB_SIZE_LOG2 > 6) {
    sf->part_sf.partition_search_breakout_dist_thr <<=
        2 * (MAX_SB_SIZE_LOG2 - 6);
  }

  const int mesh_speed = AOMMIN(speed, MAX_MESH_SPEED);
  for (i = 0; i < MAX_MESH_STEP; ++i) {
    sf->mv_sf.mesh_patterns[i].range =
        good_quality_mesh_patterns[mesh_speed][i].range;
    sf->mv_sf.mesh_patterns[i].interval =
        good_quality_mesh_patterns[mesh_speed][i].interval;
  }

  // Update the mesh pattern of exhaustive motion search for intraBC
  // Though intraBC mesh pattern is populated for all frame types, it is used
  // only for intra frames of screen contents
  for (i = 0; i < MAX_MESH_STEP; ++i) {
    sf->mv_sf.intrabc_mesh_patterns[i].range =
        intrabc_mesh_patterns[mesh_speed][i].range;
    sf->mv_sf.intrabc_mesh_patterns[i].interval =
        intrabc_mesh_patterns[mesh_speed][i].interval;
  }

  // Slow quant, dct and trellis not worthwhile for first pass
  // so make sure they are always turned off.
  if (is_stat_generation_stage(cpi))
    sf->rd_sf.optimize_coefficients = NO_TRELLIS_OPT;

  // No recode or trellis for 1 pass.
  if (oxcf->pass == 0 && has_no_stats_stage(cpi))
    sf->hl_sf.recode_loop = DISALLOW_RECODE;

  MotionVectorSearchParams *const mv_search_params = &cpi->mv_search_params;
  if (sf->mv_sf.subpel_search_method == SUBPEL_TREE) {
    mv_search_params->find_fractional_mv_step = av1_find_best_sub_pixel_tree;
  } else if (sf->mv_sf.subpel_search_method == SUBPEL_TREE_PRUNED) {
    mv_search_params->find_fractional_mv_step =
        av1_find_best_sub_pixel_tree_pruned;
  } else if (sf->mv_sf.subpel_search_method == SUBPEL_TREE_PRUNED_MORE) {
    mv_search_params->find_fractional_mv_step =
        av1_find_best_sub_pixel_tree_pruned_more;
  }

  // This is only used in motion vector unit test.
  if (cpi->oxcf.unit_test_cfg.motion_vector_unit_test == 1)
    mv_search_params->find_fractional_mv_step = av1_return_max_sub_pixel_mv;
  else if (cpi->oxcf.unit_test_cfg.motion_vector_unit_test == 2)
    mv_search_params->find_fractional_mv_step = av1_return_min_sub_pixel_mv;

  // assert ensures that tx_domain_dist_level is accessed correctly
  assert(cpi->sf.rd_sf.tx_domain_dist_thres_level >= 0 &&
         cpi->sf.rd_sf.tx_domain_dist_thres_level < 3);
  memcpy(winner_mode_params->tx_domain_dist_threshold,
         tx_domain_dist_thresholds[cpi->sf.rd_sf.tx_domain_dist_thres_level],
         sizeof(winner_mode_params->tx_domain_dist_threshold));

  assert(cpi->sf.rd_sf.tx_domain_dist_level >= 0 &&
         cpi->sf.rd_sf.tx_domain_dist_level < 3);
  memcpy(winner_mode_params->use_transform_domain_distortion,
         tx_domain_dist_types[cpi->sf.rd_sf.tx_domain_dist_level],
         sizeof(winner_mode_params->use_transform_domain_distortion));

  // assert ensures that coeff_opt_dist_thresholds is accessed correctly
  assert(cpi->sf.rd_sf.perform_coeff_opt >= 0 &&
         cpi->sf.rd_sf.perform_coeff_opt < 7);
  memcpy(winner_mode_params->coeff_opt_dist_threshold,
         coeff_opt_dist_thresholds[cpi->sf.rd_sf.perform_coeff_opt],
         sizeof(winner_mode_params->coeff_opt_dist_threshold));

  // assert ensures that coeff_opt_satd_thresholds is accessed correctly
  assert(cpi->sf.rd_sf.perform_coeff_opt_based_on_satd >= 0 &&
         cpi->sf.rd_sf.perform_coeff_opt_based_on_satd < 3);
  memcpy(
      winner_mode_params->coeff_opt_satd_threshold,
      coeff_opt_satd_thresholds[cpi->sf.rd_sf.perform_coeff_opt_based_on_satd],
      sizeof(winner_mode_params->coeff_opt_satd_threshold));

  // assert ensures that predict_skip_levels is accessed correctly
  assert(cpi->sf.tx_sf.tx_type_search.use_skip_flag_prediction >= 0 &&
         cpi->sf.tx_sf.tx_type_search.use_skip_flag_prediction < 3);
  memcpy(winner_mode_params->skip_txfm_level,
         predict_skip_levels[cpi->sf.tx_sf.tx_type_search
                                 .use_skip_flag_prediction],
         sizeof(winner_mode_params->skip_txfm_level));

  // assert ensures that tx_size_search_level is accessed correctly
  assert(cpi->sf.winner_mode_sf.tx_size_search_level >= 0 &&
         cpi->sf.winner_mode_sf.tx_size_search_level < 3);
  memcpy(winner_mode_params->tx_size_search_methods,
         tx_size_search_methods[cpi->sf.winner_mode_sf.tx_size_search_level],
         sizeof(winner_mode_params->tx_size_search_methods));
  memcpy(winner_mode_params->predict_dc_level,
         predict_dc_levels[cpi->sf.winner_mode_sf.dc_blk_pred_level],
         sizeof(winner_mode_params->predict_dc_level));

  if (cpi->oxcf.row_mt == 1 && (cpi->oxcf.max_threads > 1)) {
    if (sf->inter_sf.inter_mode_rd_model_estimation == 1) {
      // Revert to type 2
      sf->inter_sf.inter_mode_rd_model_estimation = 2;
    }

    // Disable the speed feature 'prune_ref_frame_for_gm_search' to achieve
    // better parallelism when number of threads available are greater than or
    // equal to maximum number of reference frames allowed for global motion.
    if (sf->gm_sf.gm_search_type != GM_DISABLE_SEARCH &&
        (cpi->oxcf.max_threads >=
         gm_available_reference_frames[sf->gm_sf.gm_search_type]))
      sf->gm_sf.prune_ref_frame_for_gm_search = 0;
  }
}

// Override some speed features based on qindex
void av1_set_speed_features_qindex_dependent(AV1_COMP *cpi, int speed) {
  AV1_COMMON *const cm = &cpi->common;
  SPEED_FEATURES *const sf = &cpi->sf;
  WinnerModeParams *const winner_mode_params = &cpi->winner_mode_params;
  const int boosted = frame_is_boosted(cpi);
  const int is_720p_or_larger = AOMMIN(cm->width, cm->height) >= 720;
  const int is_1080p_or_larger = AOMMIN(cm->width, cm->height) >= 1080;
  const int is_arf2_bwd_type =
      cpi->gf_group.update_type[cpi->gf_group.index] == INTNL_ARF_UPDATE;

  if (cpi->oxcf.mode == GOOD && speed == 0) {
    // qindex_thresh for resolution < 720p
    const int qindex_thresh = boosted ? 70 : (is_arf2_bwd_type ? 110 : 140);
    if (!is_720p_or_larger && cm->quant_params.base_qindex <= qindex_thresh) {
      sf->inter_sf.skip_repeated_newmv = 1;
      sf->part_sf.simple_motion_search_split =
          cm->features.allow_screen_content_tools ? 1 : 2;
      sf->part_sf.simple_motion_search_early_term_none = 1;
      sf->tx_sf.model_based_prune_tx_search_level = 0;
    }

    if (is_720p_or_larger && cm->quant_params.base_qindex <= 128) {
      sf->rd_sf.perform_coeff_opt = 2 + is_1080p_or_larger;
      memcpy(winner_mode_params->coeff_opt_dist_threshold,
             coeff_opt_dist_thresholds[sf->rd_sf.perform_coeff_opt],
             sizeof(winner_mode_params->coeff_opt_dist_threshold));
      sf->part_sf.simple_motion_search_split =
          cm->features.allow_screen_content_tools ? 1 : 2;
      sf->tx_sf.inter_tx_size_search_init_depth_rect = 1;
      sf->tx_sf.inter_tx_size_search_init_depth_sqr = 1;
      sf->tx_sf.intra_tx_size_search_init_depth_rect = 1;
      sf->inter_sf.skip_repeated_newmv = 1;
      sf->tx_sf.model_based_prune_tx_search_level = 0;

      if (is_1080p_or_larger && cm->quant_params.base_qindex <= 108) {
        sf->inter_sf.selective_ref_frame = 2;
        sf->rd_sf.tx_domain_dist_level = boosted ? 1 : 2;
        sf->rd_sf.tx_domain_dist_thres_level = 1;
        sf->part_sf.simple_motion_search_early_term_none = 1;
        sf->tx_sf.tx_type_search.ml_tx_split_thresh = 4000;
        sf->interp_sf.cb_pred_filter_search = 0;
        sf->tx_sf.tx_type_search.prune_2d_txfm_mode = TX_TYPE_PRUNE_2;
        sf->tx_sf.tx_type_search.skip_tx_search = 1;
        sf->tx_sf.use_intra_txb_hash = 1;
      }
    }
  }

  if (cpi->oxcf.mode == GOOD && speed >= 3) {
    // Disable extended partitions for lower quantizers
    const int qindex_thresh =
        cm->features.allow_screen_content_tools ? 50 : 100;
    if (cm->quant_params.base_qindex <= qindex_thresh && !boosted) {
      sf->part_sf.ext_partition_eval_thresh = BLOCK_128X128;
    }
  }

  if (cpi->oxcf.mode == GOOD && speed >= 4) {
    // Disable extended partitions for lower quantizers
    const int qindex_thresh = boosted ? 80 : 120;
    if (cm->quant_params.base_qindex <= qindex_thresh &&
        !frame_is_intra_only(&cpi->common)) {
      sf->part_sf.ext_partition_eval_thresh = BLOCK_128X128;
    }
  }

  if (cpi->oxcf.mode == GOOD && speed >= 5) {
    const int qindex_thresh = boosted ? 100 : 160;
    if (cm->quant_params.base_qindex <= qindex_thresh &&
        !frame_is_intra_only(&cpi->common)) {
      sf->part_sf.ext_partition_eval_thresh = BLOCK_128X128;
    }
  }

  if (cpi->oxcf.mode == GOOD && (speed <= 2)) {
    if (!is_stat_generation_stage(cpi)) {
      // Use faster full-pel motion search for high quantizers.
      // Also use reduced total search range for low resolutions at high
      // quantizers.
      const int aggr = speed;
      const int qindex_thresh1 = ms_qindex_thresh[aggr][is_720p_or_larger][0];
      const int qindex_thresh2 = ms_qindex_thresh[aggr][is_720p_or_larger][1];
      const SEARCH_METHODS search_method =
          motion_search_method[is_720p_or_larger];
      if (cm->quant_params.base_qindex > qindex_thresh1) {
        sf->mv_sf.search_method = search_method;
        sf->tpl_sf.search_method = search_method;
      } else if (cm->quant_params.base_qindex > qindex_thresh2) {
        sf->mv_sf.search_method = NSTEP_8PT;
      }
    }
  }
}
