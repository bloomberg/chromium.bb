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

#ifndef AOM_AV1_ENCODER_SPEED_FEATURES_H_
#define AOM_AV1_ENCODER_SPEED_FEATURES_H_

#include "av1/common/enums.h"
#include "av1/encoder/enc_enums.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/encodemb.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! @file */

/*!\cond */
#define MAX_MESH_STEP 4

typedef struct MESH_PATTERN {
  int range;
  int interval;
} MESH_PATTERN;

enum {
  GM_FULL_SEARCH,
  GM_REDUCED_REF_SEARCH_SKIP_L2_L3,
  GM_REDUCED_REF_SEARCH_SKIP_L2_L3_ARF2,
  GM_DISABLE_SEARCH
} UENUM1BYTE(GM_SEARCH_TYPE);

enum {
  DIST_WTD_COMP_ENABLED,
  DIST_WTD_COMP_SKIP_MV_SEARCH,
  DIST_WTD_COMP_DISABLED,
} UENUM1BYTE(DIST_WTD_COMP_FLAG);

enum {
  INTRA_ALL = (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED) | (1 << D45_PRED) |
              (1 << D135_PRED) | (1 << D113_PRED) | (1 << D157_PRED) |
              (1 << D203_PRED) | (1 << D67_PRED) | (1 << SMOOTH_PRED) |
              (1 << SMOOTH_V_PRED) | (1 << SMOOTH_H_PRED) | (1 << PAETH_PRED),
  UV_INTRA_ALL =
      (1 << UV_DC_PRED) | (1 << UV_V_PRED) | (1 << UV_H_PRED) |
      (1 << UV_D45_PRED) | (1 << UV_D135_PRED) | (1 << UV_D113_PRED) |
      (1 << UV_D157_PRED) | (1 << UV_D203_PRED) | (1 << UV_D67_PRED) |
      (1 << UV_SMOOTH_PRED) | (1 << UV_SMOOTH_V_PRED) |
      (1 << UV_SMOOTH_H_PRED) | (1 << UV_PAETH_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC = (1 << UV_DC_PRED),
  UV_INTRA_DC_CFL = (1 << UV_DC_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_TM = (1 << UV_DC_PRED) | (1 << UV_PAETH_PRED),
  UV_INTRA_DC_PAETH_CFL =
      (1 << UV_DC_PRED) | (1 << UV_PAETH_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_H_V = (1 << UV_DC_PRED) | (1 << UV_V_PRED) | (1 << UV_H_PRED),
  UV_INTRA_DC_H_V_CFL = (1 << UV_DC_PRED) | (1 << UV_V_PRED) |
                        (1 << UV_H_PRED) | (1 << UV_CFL_PRED),
  UV_INTRA_DC_PAETH_H_V = (1 << UV_DC_PRED) | (1 << UV_PAETH_PRED) |
                          (1 << UV_V_PRED) | (1 << UV_H_PRED),
  UV_INTRA_DC_PAETH_H_V_CFL = (1 << UV_DC_PRED) | (1 << UV_PAETH_PRED) |
                              (1 << UV_V_PRED) | (1 << UV_H_PRED) |
                              (1 << UV_CFL_PRED),
  INTRA_DC = (1 << DC_PRED),
  INTRA_DC_TM = (1 << DC_PRED) | (1 << PAETH_PRED),
  INTRA_DC_H_V = (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED),
  INTRA_DC_H_V_SMOOTH =
      (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED) | (1 << SMOOTH_PRED),
  INTRA_DC_PAETH_H_V =
      (1 << DC_PRED) | (1 << PAETH_PRED) | (1 << V_PRED) | (1 << H_PRED)
};

enum {
  INTER_ALL = (1 << NEARESTMV) | (1 << NEARMV) | (1 << GLOBALMV) |
              (1 << NEWMV) | (1 << NEAREST_NEARESTMV) | (1 << NEAR_NEARMV) |
              (1 << NEW_NEWMV) | (1 << NEAREST_NEWMV) | (1 << NEAR_NEWMV) |
              (1 << NEW_NEARMV) | (1 << NEW_NEARESTMV) | (1 << GLOBAL_GLOBALMV),
  INTER_NEAREST_NEAR_ZERO = (1 << NEARESTMV) | (1 << NEARMV) | (1 << GLOBALMV) |
                            (1 << NEAREST_NEARESTMV) | (1 << GLOBAL_GLOBALMV) |
                            (1 << NEAREST_NEWMV) | (1 << NEW_NEARESTMV) |
                            (1 << NEW_NEARMV) | (1 << NEAR_NEWMV) |
                            (1 << NEAR_NEARMV),
};

enum {
  DISABLE_ALL_INTER_SPLIT = (1 << THR_COMP_GA) | (1 << THR_COMP_LA) |
                            (1 << THR_ALTR) | (1 << THR_GOLD) | (1 << THR_LAST),

  DISABLE_ALL_SPLIT = (1 << THR_INTRA) | DISABLE_ALL_INTER_SPLIT,

  DISABLE_COMPOUND_SPLIT = (1 << THR_COMP_GA) | (1 << THR_COMP_LA),

  LAST_AND_INTRA_SPLIT_ONLY = (1 << THR_COMP_GA) | (1 << THR_COMP_LA) |
                              (1 << THR_ALTR) | (1 << THR_GOLD)
};

enum {
  TXFM_CODING_SF = 1,
  INTER_PRED_SF = 2,
  INTRA_PRED_SF = 4,
  PARTITION_SF = 8,
  LOOP_FILTER_SF = 16,
  RD_SKIP_SF = 32,
  RESERVE_2_SF = 64,
  RESERVE_3_SF = 128,
} UENUM1BYTE(DEV_SPEED_FEATURES);

/* This enumeration defines when the rate control recode loop will be
 * enabled.
 */
enum {
  /*
   * No recodes allowed
   */
  DISALLOW_RECODE = 0,
  /*
   * Allow recode only for KF/ARF/GF frames
   */
  ALLOW_RECODE_KFARFGF = 1,
  /*
   * Allow recode for all frame types based on bitrate constraints.
   */
  ALLOW_RECODE = 2,
} UENUM1BYTE(RECODE_LOOP_TYPE);

enum {
  SUBPEL_TREE = 0,
  SUBPEL_TREE_PRUNED = 1,       // Prunes 1/2-pel searches
  SUBPEL_TREE_PRUNED_MORE = 2,  // Prunes 1/2-pel searches more aggressively
} UENUM1BYTE(SUBPEL_SEARCH_METHODS);

enum {
  // Try the full image with different values.
  LPF_PICK_FROM_FULL_IMAGE,
  // Try the full image filter search with non-dual filter only.
  LPF_PICK_FROM_FULL_IMAGE_NON_DUAL,
  // Try a small portion of the image with different values.
  LPF_PICK_FROM_SUBIMAGE,
  // Estimate the level based on quantizer and frame type
  LPF_PICK_FROM_Q,
  // Pick 0 to disable LPF if LPF was enabled last frame
  LPF_PICK_MINIMAL_LPF
} UENUM1BYTE(LPF_PICK_METHOD);
/*!\endcond */

/*!\enum CDEF_PICK_METHOD
 * \brief This enumeration defines a variety of CDEF pick methods
 */
typedef enum {
  CDEF_FULL_SEARCH,      /**< Full search */
  CDEF_FAST_SEARCH_LVL1, /**< Search among a subset of all possible filters. */
  CDEF_FAST_SEARCH_LVL2, /**< Search reduced subset of filters than Level 1. */
  CDEF_FAST_SEARCH_LVL3, /**< Search reduced subset of secondary filters than
                              Level 2. */
  CDEF_FAST_SEARCH_LVL4, /**< Search reduced subset of filters than Level 3. */
  CDEF_PICK_FROM_Q,      /**< Estimate filter strength based on quantizer. */
  CDEF_PICK_METHODS
} CDEF_PICK_METHOD;

/*!\cond */
enum {
  // Terminate search early based on distortion so far compared to
  // qp step, distortion in the neighborhood of the frame, etc.
  FLAG_EARLY_TERMINATE = 1 << 0,

  // Skips comp inter modes if the best so far is an intra mode.
  FLAG_SKIP_COMP_BESTINTRA = 1 << 1,

  // Skips oblique intra modes if the best so far is an inter mode.
  FLAG_SKIP_INTRA_BESTINTER = 1 << 3,

  // Skips oblique intra modes  at angles 27, 63, 117, 153 if the best
  // intra so far is not one of the neighboring directions.
  FLAG_SKIP_INTRA_DIRMISMATCH = 1 << 4,

  // Skips intra modes other than DC_PRED if the source variance is small
  FLAG_SKIP_INTRA_LOWVAR = 1 << 5,
} UENUM1BYTE(MODE_SEARCH_SKIP_LOGIC);

enum {
  // No tx type pruning
  TX_TYPE_PRUNE_0 = 0,
  // adaptively prunes the least perspective tx types out of all 16
  // (tuned to provide negligible quality loss)
  TX_TYPE_PRUNE_1 = 1,
  // similar, but applies much more aggressive pruning to get better speed-up
  TX_TYPE_PRUNE_2 = 2,
  TX_TYPE_PRUNE_3 = 3,
  // More aggressive pruning based on tx type score and allowed tx count
  TX_TYPE_PRUNE_4 = 4,
  TX_TYPE_PRUNE_5 = 5,
} UENUM1BYTE(TX_TYPE_PRUNE_MODE);

enum {
  // No reaction to rate control on a detected slide/scene change.
  NO_DETECTION = 0,

  // Set to larger Q based only on the detected slide/scene change and
  // current/past Q.
  FAST_DETECTION_MAXQ = 1,
} UENUM1BYTE(OVERSHOOT_DETECTION_CBR);

enum {
  // Turns off multi-winner mode. So we will do txfm search on either all modes
  // if winner mode is off, or we will only on txfm search on a single winner
  // mode.
  MULTI_WINNER_MODE_OFF = 0,

  // Limits the number of winner modes to at most 2
  MULTI_WINNER_MODE_FAST = 1,

  // Uses the default number of winner modes, which is 3 for intra mode, and 1
  // for inter mode.
  MULTI_WINNER_MODE_DEFAULT = 2,
} UENUM1BYTE(MULTI_WINNER_MODE_TYPE);

typedef struct {
  TX_TYPE_PRUNE_MODE prune_2d_txfm_mode;
  int fast_intra_tx_type_search;
  int fast_inter_tx_type_search;

  // prune two least frequently chosen transforms for each intra mode
  int use_reduced_intra_txset;

  // Use a skip flag prediction model to detect blocks with skip = 1 early
  // and avoid doing full TX type search for such blocks.
  int use_skip_flag_prediction;

  // Threshold used by the ML based method to predict TX block split decisions.
  int ml_tx_split_thresh;

  // skip remaining transform type search when we found the rdcost of skip is
  // better than applying transform
  int skip_tx_search;

  // Prune tx type search using previous frame stats.
  int prune_tx_type_using_stats;
  // Prune tx type search using estimated RDcost
  int prune_tx_type_est_rd;

  // Flag used to control the winner mode processing for tx type pruning for
  // inter blocks. It enables further tx type mode pruning based on ML model for
  // mode evaluation and disables tx type mode pruning for winner mode
  // processing.
  int winner_mode_tx_type_pruning;
} TX_TYPE_SEARCH;

enum {
  // Search partitions using RD criterion
  SEARCH_PARTITION,

  // Always use a fixed size partition
  FIXED_PARTITION,

  // Partition using source variance
  VAR_BASED_PARTITION,

#if CONFIG_RT_ML_PARTITIONING
  // Partition using ML model
  ML_BASED_PARTITION
#endif
} UENUM1BYTE(PARTITION_SEARCH_TYPE);

enum {
  NOT_IN_USE,
  DIRECT_PRED,
  RELAXED_PRED,
  ADAPT_PRED
} UENUM1BYTE(MAX_PART_PRED_MODE);

enum {
  LAST_MV_DATA,
  CURRENT_Q,
  QTR_ONLY,
} UENUM1BYTE(MV_PREC_LOGIC);

enum {
  SUPERRES_AUTO_ALL,   // Tries all possible superres ratios
  SUPERRES_AUTO_DUAL,  // Tries no superres and q-based superres ratios
  SUPERRES_AUTO_SOLO,  // Only apply the q-based superres ratio
} UENUM1BYTE(SUPERRES_AUTO_SEARCH_TYPE);

/*!\endcond */
/*!
 * \brief Sequence/frame level speed vs quality features
 */
typedef struct HIGH_LEVEL_SPEED_FEATURES {
  /*!\cond */
  // Frame level coding parameter update
  int frame_parameter_update;

  /*!\endcond */
  /*!
   * Cases and frame types for which the recode loop is enabled.
   */
  RECODE_LOOP_TYPE recode_loop;

  /*!
   * Controls the tolerance vs target rate used in deciding whether to
   * recode a frame. It has no meaning if recode is disabled.
   */
  int recode_tolerance;

  /*!\cond */
  // Determine how motion vector precision is chosen. The possibilities are:
  // LAST_MV_DATA: use the mv data from the last coded frame
  // CURRENT_Q: use the current q as a threshold
  // QTR_ONLY: use quarter pel precision only.
  MV_PREC_LOGIC high_precision_mv_usage;

  // Always set to 0. If on it enables 0 cost background transmission
  // (except for the initial transmission of the segmentation). The feature is
  // disabled because the addition of very large block sizes make the
  // backgrounds very to cheap to encode, and the segmentation we have
  // adds overhead.
  int static_segmentation;

  /*!
   * Superres-auto mode search type:
   */
  SUPERRES_AUTO_SEARCH_TYPE superres_auto_search_type;
  /*!\endcond */

  /*!
   * Enable/disable extra screen content test by encoding key frame twice.
   */
  int disable_extra_sc_testing;

  /*!
   * Enable/disable second_alt_ref temporal filtering.
   */
  int second_alt_ref_filtering;
} HIGH_LEVEL_SPEED_FEATURES;

/*!\cond */
typedef struct TPL_SPEED_FEATURES {
  // Enable/disable GOP length adaptive decision.
  int disable_gop_length_decision;
  // Prune the intra modes search by tpl.
  // If set to 0, we will search all intra modes from DC_PRED to PAETH_PRED.
  // If set to 1, we only search DC_PRED, V_PRED, and H_PRED.
  int prune_intra_modes;
  // This parameter controls which step in the n-step process we start at.
  int reduce_first_step_size;
  // Skip motion estimation based on the precision of center MVs and the
  // difference between center MVs.
  // If set to 0, motion estimation is skipped for duplicate center MVs
  // (default). If set to 1, motion estimation is skipped for duplicate
  // full-pixel center MVs. If set to 2, motion estimation is skipped if the
  // difference between center MVs is less than the threshold.
  int skip_alike_starting_mv;

  // When to stop subpel search.
  SUBPEL_FORCE_STOP subpel_force_stop;

  // Which search method to use.
  SEARCH_METHODS search_method;

  // Prune starting mvs in TPL based on sad scores.
  int prune_starting_mv;

  // Not run TPL for filtered Key frame.
  int disable_filtered_key_tpl;

  // Prune reference frames in TPL.
  int prune_ref_frames_in_tpl;

  // Support compound predictions.
  int allow_compound_pred;

  // Calculate rate and distortion based on Y plane only.
  int use_y_only_rate_distortion;
} TPL_SPEED_FEATURES;

typedef struct GLOBAL_MOTION_SPEED_FEATURES {
  GM_SEARCH_TYPE gm_search_type;

  // During global motion estimation, prune remaining reference frames in a
  // given direction(past/future), if the evaluated ref_frame in that direction
  // yields gm_type as INVALID/TRANSLATION/IDENTITY
  int prune_ref_frame_for_gm_search;
} GLOBAL_MOTION_SPEED_FEATURES;

typedef struct PARTITION_SPEED_FEATURES {
  PARTITION_SEARCH_TYPE partition_search_type;

  // Used if partition_search_type = FIXED_PARTITION
  BLOCK_SIZE fixed_partition_size;

  // Prune extended partition types search
  // Can take values 0 - 2, 0 referring to no pruning, and 1 - 2 increasing
  // aggressiveness of pruning in order.
  int prune_ext_partition_types_search_level;

  // Prune part4 based on block size
  int prune_part4_search;

  // Use a ML model to prune rectangular, ab and 4-way horz
  // and vert partitions
  int ml_prune_partition;

  // Use a ML model to adaptively terminate partition search after trying
  // PARTITION_SPLIT. Can take values 0 - 2, 0 meaning not being enabled, and
  // 1 - 2 increasing aggressiveness in order.
  int ml_early_term_after_part_split_level;

  // Skip rectangular partition test when partition type none gives better
  // rd than partition type split. Can take values 0 - 2, 0 referring to no
  // skipping, and 1 - 2 increasing aggressiveness of skipping in order.
  int less_rectangular_check_level;

  // Use square partition only beyond this block size.
  BLOCK_SIZE use_square_partition_only_threshold;

  // Sets max square partition levels for this superblock based on
  // motion vector and prediction error distribution produced from 16x16
  // simple motion search
  MAX_PART_PRED_MODE auto_max_partition_based_on_simple_motion;

  // Min and max square partition size we enable (block_size) as per auto
  // min max, but also used by adjust partitioning, and pick_partitioning.
  BLOCK_SIZE default_min_partition_size;
  BLOCK_SIZE default_max_partition_size;

  // Sets level of adjustment of variance-based partitioning during
  // rd_use_partition 0 - no partition adjustment, 1 - try to merge partitions
  // for small blocks and high QP, 2 - always try to merge leaf partitions, 3 -
  // try to merge and split leaf partitions
  int adjust_var_based_rd_partitioning;

  // Partition search early breakout thresholds.
  int64_t partition_search_breakout_dist_thr;
  int partition_search_breakout_rate_thr;

  // Thresholds for ML based partition search breakout.
  int ml_partition_search_breakout_thresh[PARTITION_BLOCK_SIZES];

  // Allow skipping partition search for still image frame
  int allow_partition_search_skip;

  // The aggressiveness of pruning with simple_motion_search.
  // Currently 0 is the lowest, and 2 the highest.
  int simple_motion_search_prune_agg;

  // Perform simple_motion_search on each possible subblock and use it to prune
  // PARTITION_HORZ and PARTITION_VERT.
  int simple_motion_search_prune_rect;

  // Perform simple motion search before none_partition to decide if we
  // want to remove all partitions other than PARTITION_SPLIT. If set to 0, this
  // model is disabled. If set to 1, the model attempts to perform
  // PARTITION_SPLIT only. If set to 2, the model also attempts to prune
  // PARTITION_SPLIT.
  int simple_motion_search_split;

  // Use features from simple_motion_search to terminate prediction block
  // partition after PARTITION_NONE
  int simple_motion_search_early_term_none;

  // Controls whether to reduce the number of motion search steps. If this is 0,
  // then simple_motion_search has the same number of steps as
  // single_motion_search (assuming no other speed features). Otherwise, reduce
  // the number of steps by the value contained in this variable.
  int simple_motion_search_reduce_search_steps;

  // This variable controls the maximum block size where intra blocks can be
  // used in inter frames.
  // TODO(aconverse): Fold this into one of the other many mode skips
  BLOCK_SIZE max_intra_bsize;

  // Use CNN with luma pixels on source frame on each of the 64x64 subblock to
  // perform split/no_split decision on intra-frames.
  int intra_cnn_split;

  // Disable extended partition search for lower block sizes.
  int ext_partition_eval_thresh;

  // prune extended partition search
  // 0 : no pruning
  // 1 : prune 1:4 partition search using winner info from split partitions
  // 2 : prune 1:4 and AB partition search using split and HORZ/VERT info
  int prune_ext_part_using_split_info;

  // Prunt rectangular, AB and 4-way partition based on q index and block size
  int prune_rectangular_split_based_on_qidx;

  // Terminate partition search for child partition,
  // when NONE and SPLIT partition rd_costs are INT64_MAX.
  int early_term_after_none_split;

  // Level used to adjust threshold for av1_ml_predict_breakout(). At lower
  // levels, more conservative threshold is used, and value of 0 indicates
  // av1_ml_predict_breakout() is disabled. Value of 3 corresponds to default
  // case with no adjustment to lbd thresholds.
  int ml_predict_breakout_level;

  // Prune sub_8x8 (BLOCK_4X4, BLOCK_4X8 and BLOCK_8X4) partitions.
  // 0 : no pruning
  // 1 : pruning based on neighbour block information
  // 2 : prune always
  int prune_sub_8x8_partition_level;
} PARTITION_SPEED_FEATURES;

typedef struct MV_SPEED_FEATURES {
  // Motion search method (Diamond, NSTEP, Hex, Big Diamond, Square, etc).
  SEARCH_METHODS search_method;

  // Enable the use of faster, less accurate mv search method on bsize >=
  // BLOCK_32X32.
  // TODO(chiyotsai@google.com): Take the clip's resolution and mv activity into
  // account.
  int use_bsize_dependent_search_method;

  // If this is set to 1, we limit the motion search range to 2 times the
  // largest motion vector found in the last frame.
  int auto_mv_step_size;

  // Subpel_search_method can only be subpel_tree which does a subpixel
  // logarithmic search that keeps stepping at 1/2 pixel units until
  // you stop getting a gain, and then goes on to 1/4 and repeats
  // the same process. Along the way it skips many diagonals.
  SUBPEL_SEARCH_METHODS subpel_search_method;

  // Maximum number of steps in logarithmic subpel search before giving up.
  int subpel_iters_per_step;

  // When to stop subpel search.
  SUBPEL_FORCE_STOP subpel_force_stop;

  // When to stop subpel search in simple motion search.
  SUBPEL_FORCE_STOP simple_motion_subpel_force_stop;

  // If true, sub-pixel search uses the exact convolve function used for final
  // encoding and decoding; otherwise, it uses bilinear interpolation.
  SUBPEL_SEARCH_TYPE use_accurate_subpel_search;

  // Threshold for allowing exhaustive motion search.
  int exhaustive_searches_thresh;

  // Pattern to be used for any exhaustive mesh searches (except intraBC ME).
  MESH_PATTERN mesh_patterns[MAX_MESH_STEP];

  // Pattern to be used for exhaustive mesh searches of intraBC ME.
  MESH_PATTERN intrabc_mesh_patterns[MAX_MESH_STEP];

  // Reduce single motion search range based on MV result of prior ref_mv_idx.
  int reduce_search_range;

  // Prune mesh search.
  int prune_mesh_search;

  // Use the rd cost around the best FULLPEL_MV to speed up subpel search
  int use_fullpel_costlist;

  // Set the full pixel search level of obmc
  // 0: obmc_full_pixel_diamond
  // 1: obmc_refining_search_sad (faster)
  int obmc_full_pixel_search_level;

  // Accurate full pixel motion search based on TPL stats.
  int full_pixel_search_level;

  // Whether to downsample the rows in sad calculation during motion search.
  // This is only active when there are at least 16 rows.
  int use_downsampled_sad;

  // Enable/disable extensive joint motion search.
  int disable_extensive_joint_motion_search;

  // Enable second best mv check in joint mv search.
  int disable_second_mv;
} MV_SPEED_FEATURES;

typedef struct INTER_MODE_SPEED_FEATURES {
  // 2-pass inter mode model estimation where the preliminary pass skips
  // transform search and uses a model to estimate rd, while the final pass
  // computes the full transform search. Two types of models are supported:
  // 0: not used
  // 1: used with online dynamic rd model
  // 2: used with static rd model
  int inter_mode_rd_model_estimation;

  // Bypass transform search based on skip rd
  int txfm_rd_gate_level;

  // Limit the inter mode tested in the RD loop
  int reduce_inter_modes;

  // This variable is used to cap the maximum number of times we skip testing a
  // mode to be evaluated. A high value means we will be faster.
  int adaptive_rd_thresh;

  // Aggressively prune inter modes when best mode is skippable.
  int prune_inter_modes_if_skippable;

  // Drop less likely to be picked reference frames in the RD search.
  // Has six levels for now: 0, 1, 2, 3, 4 and 5, where higher levels prune
  // more aggressively than lower ones. (0 means no pruning).
  int selective_ref_frame;

  // Prune reference frames for rectangular partitions.
  // 0 implies no pruning
  // 1 implies prune for extended partition
  // 2 implies prune horiz, vert and extended partition
  int prune_ref_frame_for_rect_partitions;

  int alt_ref_search_fp;

  // flag to skip NEWMV mode in drl if the motion search result is the same
  int skip_repeated_newmv;

  // Skip the current ref_mv in NEW_MV mode if we have already encountered
  // another ref_mv in the drl such that:
  //  1. The other drl has the same fullpel_mv during the SIMPLE_TRANSLATION
  //     search process as the current fullpel_mv.
  //  2. The rate needed to encode the current fullpel_mv is larger than that
  //     for the other ref_mv.
  int skip_repeated_full_newmv;

  // This speed feature checks duplicate ref MVs among NEARESTMV, NEARMV,
  // GLOBALMV and skips NEARMV or GLOBALMV (in order) if a duplicate is found
  // TODO(any): Instead of skipping repeated ref mv, use the recalculated
  // rd-cost based on mode rate and skip the mode evaluation
  int skip_repeated_ref_mv;

  // Flag used to control the ref_best_rd based gating for chroma
  int perform_best_rd_based_gating_for_chroma;

  // Reuse the inter_intra_mode search result from NEARESTMV mode to other
  // single ref modes
  int reuse_inter_intra_mode;

  // prune wedge and compound segment approximate rd evaluation based on
  // compound average modeled rd
  int prune_comp_type_by_model_rd;

  // prune wedge and compound segment approximate rd evaluation based on
  // compound average rd/ref_best_rd
  int prune_comp_type_by_comp_avg;

  // Skip some ref frames in compound motion search by single motion search
  // result. Has three levels for now: 0 referring to no skipping, and 1 - 3
  // increasing aggressiveness of skipping in order.
  // Note: The search order might affect the result. It assumes that the single
  // reference modes are searched before compound modes. It is better to search
  // same single inter mode as a group.
  int prune_comp_search_by_single_result;

  // If 1 we iterate finding a best reference for 2 ref frames together - via
  // a log search that iterates 4 times (check around mv for last for best
  // error of combined predictor then check around mv for alt). If 0 we
  // we just use the best motion vector found for each frame by itself.
  BLOCK_SIZE comp_inter_joint_search_thresh;

  // Instead of performing a full MV search, do a simple translation first
  // and only perform a full MV search on the motion vectors that performed
  // well.
  int prune_mode_search_simple_translation;

  // Only search compound modes with at least one "good" reference frame.
  // A reference frame is good if, after looking at its performance among
  // the single reference modes, it is one of the two best performers.
  int prune_compound_using_single_ref;

  // Skip extended compound mode using ref frames of above and left neighbor
  // blocks.
  // 0 : no pruning
  // 1 : prune extended compound mode (less aggressiveness)
  // 2 : prune extended compound mode (high aggressiveness)
  int prune_compound_using_neighbors;

  // Skip extended compound mode when ref frame corresponding to NEWMV does not
  // have NEWMV as single mode winner.
  // 0 : no pruning
  // 1 : prune extended compound mode (less aggressiveness)
  // 2 : prune extended compound mode (high aggressiveness)
  int prune_comp_using_best_single_mode_ref;

  // Based on previous ref_mv_idx search result, prune the following search.
  int prune_ref_mv_idx_search;

  // Disable one sided compound modes.
  int disable_onesided_comp;

  // Prune obmc search using previous frame stats.
  // INT_MAX : disable obmc search
  int prune_obmc_prob_thresh;

  // Prune warped motion search using previous frame stats.
  int prune_warped_prob_thresh;

  // Variance threshold to enable/disable Interintra wedge search
  unsigned int disable_interintra_wedge_var_thresh;

  // Variance threshold to enable/disable Interinter wedge search
  unsigned int disable_interinter_wedge_var_thresh;

  // De-couple wedge and mode search during interintra RDO.
  int fast_interintra_wedge_search;

  // Whether fast wedge sign estimate is used
  int fast_wedge_sign_estimate;

  // Enable/disable ME for interinter wedge search.
  int disable_interinter_wedge_newmv_search;

  // Decide when and how to use joint_comp.
  DIST_WTD_COMP_FLAG use_dist_wtd_comp_flag;

  // To skip cost update for mv.
  // mv_cost_upd_level indicates the aggressiveness of skipping.
  // 0: update happens at each sb level.
  // 1: update happens once for each sb row.
  // 2: update happens once for a set of rows.
  int mv_cost_upd_level;
  // Prune inter modes based on tpl stats
  // 0 : no pruning
  // 1 - 3 indicate increasing aggressiveness in order.
  int prune_inter_modes_based_on_tpl;

  // Skip NEARMV and NEAR_NEARMV modes using ref frames of above and left
  // neighbor blocks and qindex.
  int prune_nearmv_using_neighbors;

  // Model based breakout after interpolation filter search
  // 0: no breakout
  // 1: use model based rd breakout
  int model_based_post_interp_filter_breakout;

  // Reuse compound type rd decision when exact match is found
  // 0: No reuse
  // 1: Reuse the compound type decision
  int reuse_compound_type_decision;

  // Enable/disable masked compound.
  int disable_masked_comp;

  // Reuse the best prediction modes found in PARTITION_SPLIT and PARTITION_RECT
  // when encoding PARTITION_AB.
  int reuse_best_prediction_for_part_ab;

  // Enable/disable the fast compound mode search.
  int enable_fast_compound_mode_search;

  // Reuse masked compound type search results
  int reuse_mask_search_results;
} INTER_MODE_SPEED_FEATURES;

typedef struct INTERP_FILTER_SPEED_FEATURES {
  // Do limited interpolation filter search for dual filters, since best choice
  // usually includes EIGHTTAP_REGULAR.
  int use_fast_interpolation_filter_search;

  // Disable dual filter
  int disable_dual_filter;

  // Save results of av1_interpolation_filter_search for a block
  // Check mv and ref_frames before search, if they are very close with previous
  // saved results, filter search can be skipped.
  int use_interp_filter;

  // skip sharp_filter evaluation based on regular and smooth filter rd for
  // dual_filter=0 case
  int skip_sharp_interp_filter_search;

  int cb_pred_filter_search;

  // adaptive interp_filter search to allow skip of certain filter types.
  int adaptive_interp_filter_search;
} INTERP_FILTER_SPEED_FEATURES;

typedef struct INTRA_MODE_SPEED_FEATURES {
  // These bit masks allow you to enable or disable intra modes for each
  // transform size separately.
  int intra_y_mode_mask[TX_SIZES];
  int intra_uv_mode_mask[TX_SIZES];

  // flag to allow skipping intra mode for inter frame prediction
  int skip_intra_in_interframe;

  // Prune intra mode candidates based on source block histogram of gradient.
  // Applies to luma plane only.
  int intra_pruning_with_hog;

  // Prune intra mode candidates based on source block histogram of gradient.
  // Applies to chroma plane only.
  int chroma_intra_pruning_with_hog;

  // Enable/disable smooth intra modes.
  int disable_smooth_intra;

  // Enable/disable filter intra modes.
  int disable_filter_intra;

  // prune palette search
  // 0: No pruning
  // 1: Perform coarse search to prune the palette colors. For winner colors,
  // neighbors are also evaluated using a finer search.
  // 2: Perform 2 way palette search from max colors to min colors (and min
  // colors to remaining colors) and terminate the search if current number of
  // palette colors is not the winner.
  int prune_palette_search_level;
} INTRA_MODE_SPEED_FEATURES;

typedef struct TX_SPEED_FEATURES {
  // Init search depth for square and rectangular transform partitions.
  // Values:
  // 0 - search full tree, 1: search 1 level, 2: search the highest level only
  int inter_tx_size_search_init_depth_sqr;
  int inter_tx_size_search_init_depth_rect;
  int intra_tx_size_search_init_depth_sqr;
  int intra_tx_size_search_init_depth_rect;

  // If any dimension of a coding block size above 64, always search the
  // largest transform only, since the largest transform block size is 64x64.
  int tx_size_search_lgr_block;

  TX_TYPE_SEARCH tx_type_search;

  // Skip split transform block partition when the collocated bigger block
  // is selected as all zero coefficients.
  int txb_split_cap;

  // Shortcut the transform block partition and type search when the target
  // rdcost is relatively lower.
  // Values are 0 (not used) , or 1 - 2 with progressively increasing
  // aggressiveness
  int adaptive_txb_search_level;

  // Prune level for tx_size_type search for inter based on rd model
  // 0: no pruning
  // 1-2: progressively increasing aggressiveness of pruning
  int model_based_prune_tx_search_level;

  // Use hash table to store intra(keyframe only) txb transform search results
  // to avoid repeated search on the same residue signal. This is currently not
  // compatible with multi-winner mode as the hash states are reset during
  // winner mode processing.
  int use_intra_txb_hash;

  // Use hash table to store inter txb transform search results
  // to avoid repeated search on the same residue signal.
  int use_inter_txb_hash;

  // Refine TX type after fast TX search.
  int refine_fast_tx_search_results;

  // Prune transform split/no_split eval based on residual properties. A value
  // of 0 indicates no pruning, and the aggressiveness of pruning progressively
  // increases from levels 1 to 3.
  int prune_tx_size_level;
} TX_SPEED_FEATURES;

typedef struct RD_CALC_SPEED_FEATURES {
  // Fast approximation of av1_model_rd_from_var_lapndz
  int simple_model_rd_from_var;

  // Whether to compute distortion in the image domain (slower but
  // more accurate), or in the transform domain (faster but less acurate).
  // 0: use image domain
  // 1: use transform domain in tx_type search, and use image domain for
  // RD_STATS
  // 2: use transform domain
  int tx_domain_dist_level;

  // Transform domain distortion threshold level
  int tx_domain_dist_thres_level;

  // Trellis (dynamic programming) optimization of quantized values
  TRELLIS_OPT_TYPE optimize_coefficients;

  // Use hash table to store macroblock RD search results
  // to avoid repeated search on the same residue signal.
  int use_mb_rd_hash;

  // Flag used to control the extent of coeff R-D optimization
  int perform_coeff_opt;
} RD_CALC_SPEED_FEATURES;

typedef struct WINNER_MODE_SPEED_FEATURES {
  // Flag used to control the winner mode processing for better R-D optimization
  // of quantized coeffs
  int enable_winner_mode_for_coeff_opt;

  // Flag used to control the winner mode processing for transform size
  // search method
  int enable_winner_mode_for_tx_size_srch;

  // Control transform size search level
  // Eval type: Default       Mode        Winner
  // Level 0  : FULL RD     LARGEST ALL   FULL RD
  // Level 1  : FAST RD     LARGEST ALL   FULL RD
  // Level 2  : LARGEST ALL LARGEST ALL   FULL RD
  int tx_size_search_level;

  // Flag used to control the winner mode processing for use transform
  // domain distortion
  int enable_winner_mode_for_use_tx_domain_dist;

  // Flag used to enable processing of multiple winner modes
  MULTI_WINNER_MODE_TYPE multi_winner_mode_type;

  // Motion mode for winner candidates:
  // 0: speed feature OFF
  // 1 / 2 : Use configured number of winner candidates
  int motion_mode_for_winner_cand;

  // Early DC only txfm block prediction
  // 0: speed feature OFF
  // 1 / 2 : Use the configured level for different modes
  int dc_blk_pred_level;
} WINNER_MODE_SPEED_FEATURES;

typedef struct LOOP_FILTER_SPEED_FEATURES {
  // This feature controls how the loop filter level is determined.
  LPF_PICK_METHOD lpf_pick;

  // Skip some final iterations in the determination of the best loop filter
  // level.
  int use_coarse_filter_level_search;

  // Control how the CDEF strength is determined.
  CDEF_PICK_METHOD cdef_pick_method;

  // Decoder side speed feature to add penalty for use of dual-sgr filters.
  // Takes values 0 - 10, 0 indicating no penalty and each additional level
  // adding a penalty of 1%
  int dual_sgr_penalty_level;

  // prune sgr ep using binary search like mechanism
  int enable_sgr_ep_pruning;

  // Disable loop restoration for Chroma plane
  int disable_loop_restoration_chroma;

  // Prune RESTORE_WIENER evaluation based on source variance
  // 0 : no pruning
  // 1 : conservative pruning
  // 2 : aggressive pruning
  int prune_wiener_based_on_src_var;

  // Prune self-guided loop restoration based on wiener search results
  // 0 : no pruning
  // 1 : pruning based on rdcost ratio of RESTORE_WIENER and RESTORE_NONE
  // 2 : pruning based on winner restoration type among RESTORE_WIENER and
  // RESTORE_NONE
  int prune_sgr_based_on_wiener;

  // Reduce the wiener filter win size for luma
  int reduce_wiener_window_size;

  // Disable loop restoration filter
  int disable_lr_filter;
} LOOP_FILTER_SPEED_FEATURES;

typedef struct REAL_TIME_SPEED_FEATURES {
  // check intra prediction for non-RD mode.
  int check_intra_pred_nonrd;

  // skip checking intra prediction if TX is skipped
  int skip_intra_pred_if_tx_skip;

  // Perform coarse ME before calculating variance in variance-based partition
  int estimate_motion_for_var_based_partition;

  // For nonrd_use_partition: mode of extra check of leaf partition
  // 0 - don't check merge
  // 1 - always check merge
  // 2 - check merge and prune checking final split
  int nonrd_check_partition_merge_mode;

  // For nonrd_use_partition: check of leaf partition extra split
  int nonrd_check_partition_split;

  // Implements various heuristics to skip searching modes
  // The heuristics selected are based on  flags
  // defined in the MODE_SEARCH_SKIP_HEURISTICS enum
  unsigned int mode_search_skip_flags;

  // For nonrd: Reduces ref frame search.
  // 0 - low level of search prune in non last frames
  // 1 - pruned search in non last frames
  // 2 - more pruned search in non last frames
  int nonrd_prune_ref_frame_search;

  // This flag controls the use of non-RD mode decision.
  int use_nonrd_pick_mode;

  // Use ALTREF frame in non-RD mode decision.
  int use_nonrd_altref_frame;

  // Use compound reference for non-RD mode.
  int use_comp_ref_nonrd;

  // use reduced ref set for real-time mode
  int use_real_time_ref_set;

  // Skip a number of expensive mode evaluations for blocks with very low
  // temporal variance.
  int short_circuit_low_temp_var;

  // Use modeled (currently CurvFit model) RDCost for fast non-RD mode
  int use_modeled_non_rd_cost;

  // Reuse inter prediction in fast non-rd mode.
  int reuse_inter_pred_nonrd;

  // Number of best inter modes to search transform. INT_MAX - search all.
  int num_inter_modes_for_tx_search;

  // Forces TX search off for RDCost calulation.
  int force_tx_search_off;

  // Use interpolation filter search in non-RD mode decision.
  int use_nonrd_filter_search;

  // Use simplified RD model for interpolation search and Intra
  int use_simple_rd_model;

  // If set forces interpolation filter to EIGHTTAP_REGULAR
  int skip_interp_filter_search;

  // Use hybrid (rd for bsize < 16x16, otherwise nonrd) intra search for intra
  // only frames.
  int hybrid_intra_pickmode;

  // Compute variance/sse on source difference, prior to encoding superblock.
  int source_metrics_sb_nonrd;

  // Flag to indicate process for handling overshoot on slide/scene change,
  // for real-time CBR mode.
  OVERSHOOT_DETECTION_CBR overshoot_detection_cbr;

  // Check for scene/content change detection on every frame before encoding.
  int check_scene_detection;

  // Forces larger partition blocks in variance based partitioning
  int force_large_partition_blocks;

  // uses results of temporal noise estimate
  int use_temporal_noise_estimate;

  // Parameter indicating initial search window to be used in full-pixel search
  // for nonrd_pickmode. Range [0, MAX_MVSEARCH_STEPS - 1]. Lower value
  // indicates larger window. If set to 0, step_param is set based on internal
  // logic in set_mv_search_params().
  int fullpel_search_step_param;

  // Skip loopfilter (and cdef) in svc real-time mode for
  // non_reference/droppable frames.
  int skip_loopfilter_non_reference;

  // Bit mask to enable or disable intra modes for each prediction block size
  // separately, for nonrd pickmode.
  int intra_y_mode_bsize_mask_nrd[BLOCK_SIZES];

  // Skips mode checks more agressively in nonRD mode
  int nonrd_agressive_skip;
} REAL_TIME_SPEED_FEATURES;

/*!\endcond */

/*!
 * \brief Top level speed vs quality trade off data struture.
 */
typedef struct SPEED_FEATURES {
  /*!
   * Sequence/frame level speed features:
   */
  HIGH_LEVEL_SPEED_FEATURES hl_sf;

  /*!
   * Speed features related to how tpl's searches are done.
   */
  TPL_SPEED_FEATURES tpl_sf;

  /*!
   * Global motion speed features:
   */
  GLOBAL_MOTION_SPEED_FEATURES gm_sf;

  /*!
   * Partition search speed features:
   */
  PARTITION_SPEED_FEATURES part_sf;

  /*!
   * Motion search speed features:
   */
  MV_SPEED_FEATURES mv_sf;

  /*!
   * Inter mode search speed features:
   */
  INTER_MODE_SPEED_FEATURES inter_sf;

  /*!
   * Interpolation filter search speed features:
   */
  INTERP_FILTER_SPEED_FEATURES interp_sf;

  /*!
   * Intra mode search speed features:
   */
  INTRA_MODE_SPEED_FEATURES intra_sf;

  /*!
   * Transform size/type search speed features:
   */
  TX_SPEED_FEATURES tx_sf;

  /*!
   * RD calculation speed features:
   */
  RD_CALC_SPEED_FEATURES rd_sf;

  /*!
   * Two-pass mode evaluation features:
   */
  WINNER_MODE_SPEED_FEATURES winner_mode_sf;

  /*!
   * In-loop filter speed features:
   */
  LOOP_FILTER_SPEED_FEATURES lpf_sf;

  /*!
   * Real-time mode speed features:
   */
  REAL_TIME_SPEED_FEATURES rt_sf;
} SPEED_FEATURES;
/*!\cond */

struct AV1_COMP;

/*!\endcond */
/*!\brief Frame size independent speed vs quality trade off flags
 *
 *\ingroup speed_features
 *
 * \param[in]    cpi     Top - level encoder instance structure
 * \param[in]    speed   Speed setting passed in from the command  line
 *
 * \return No return value but configures the various speed trade off flags
 *         based on the passed in speed setting. (Higher speed gives lower
 *         quality)
 */
void av1_set_speed_features_framesize_independent(struct AV1_COMP *cpi,
                                                  int speed);

/*!\brief Frame size dependent speed vs quality trade off flags
 *
 *\ingroup speed_features
 *
 * \param[in]    cpi     Top - level encoder instance structure
 * \param[in]    speed   Speed setting passed in from the command  line
 *
 * \return No return value but configures the various speed trade off flags
 *         based on the passed in speed setting and frame size. (Higher speed
 *         corresponds to lower quality)
 */
void av1_set_speed_features_framesize_dependent(struct AV1_COMP *cpi,
                                                int speed);
/*!\brief Q index dependent speed vs quality trade off flags
 *
 *\ingroup speed_features
 *
 * \param[in]    cpi     Top - level encoder instance structure
 * \param[in]    speed   Speed setting passed in from the command  line
 *
 * \return No return value but configures the various speed trade off flags
 *         based on the passed in speed setting and current frame's Q index.
 *         (Higher speed corresponds to lower quality)
 */
void av1_set_speed_features_qindex_dependent(struct AV1_COMP *cpi, int speed);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_SPEED_FEATURES_H_
