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

#ifndef AOM_AV1_ENCODER_RATECTRL_H_
#define AOM_AV1_ENCODER_RATECTRL_H_

#include "aom/aom_codec.h"
#include "aom/aom_integer.h"

#include "aom_ports/mem.h"

#include "av1/common/av1_common_int.h"
#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bits Per MB at different Q (Multiplied by 512)
#define BPER_MB_NORMBITS 9

// Use this macro to turn on/off use of alt-refs in one-pass mode.
#define USE_ALTREF_FOR_ONE_PASS 1

// Threshold used to define if a KF group is static (e.g. a slide show).
// Essentially, this means that no frame in the group has more than 1% of MBs
// that are not marked as coded with 0,0 motion in the first pass.
#define STATIC_KF_GROUP_THRESH 99
#define STATIC_KF_GROUP_FLOAT_THRESH 0.99

// The maximum duration of a GF group that is static (e.g. a slide show).
#define MAX_STATIC_GF_GROUP_LENGTH 250

// Minimum and maximum height for the new pyramid structure.
// (Old structure supports height = 1, but does NOT support height = 4).
#define MIN_PYRAMID_LVL 0
#define MAX_PYRAMID_LVL 4

#define MIN_GF_INTERVAL 4
#define MAX_GF_INTERVAL 32
#define FIXED_GF_INTERVAL 8  // Used in some testing modes only
#define MAX_GF_LENGTH_LAP 16

#define MAX_NUM_GF_INTERVALS 15

#define MAX_ARF_LAYERS 6
// #define STRICT_RC

typedef struct {
  int resize_width;
  int resize_height;
  uint8_t superres_denom;
} size_params_type;

enum {
  INTER_NORMAL,
  GF_ARF_LOW,
  GF_ARF_STD,
  KF_STD,
  RATE_FACTOR_LEVELS
} UENUM1BYTE(RATE_FACTOR_LEVEL);

enum {
  KF_UPDATE,
  LF_UPDATE,
  GF_UPDATE,
  ARF_UPDATE,
  OVERLAY_UPDATE,
  INTNL_OVERLAY_UPDATE,  // Internal Overlay Frame
  INTNL_ARF_UPDATE,      // Internal Altref Frame
  FRAME_UPDATE_TYPES
} UENUM1BYTE(FRAME_UPDATE_TYPE);

typedef struct {
  // Rate targetting variables
  int base_frame_target;  // A baseline frame target before adjustment
                          // for previous under or over shoot.
  int this_frame_target;  // Actual frame target after rc adjustment.

  // gop bit budget
  int64_t gf_group_bits;

  int projected_frame_size;
  int sb64_target_rate;
  int last_q[FRAME_TYPES];  // Separate values for Intra/Inter
  int last_boosted_qindex;  // Last boosted GF/KF/ARF q
  int last_kf_qindex;       // Q index of the last key frame coded.

  int gfu_boost;
  int kf_boost;

  double rate_correction_factors[RATE_FACTOR_LEVELS];

  int frames_since_golden;
  int frames_till_gf_update_due;

  // number of determined gf group length left
  int intervals_till_gf_calculate_due;
  // stores gf group length intervals
  int gf_intervals[MAX_NUM_GF_INTERVALS];
  // the current index in gf_intervals
  int cur_gf_index;

  int min_gf_interval;
  int max_gf_interval;
  int static_scene_max_gf_interval;
  int baseline_gf_interval;
  int constrained_gf_group;
  int frames_to_key;
  int frames_since_key;
  int this_key_frame_forced;
  int next_key_frame_forced;
  int source_alt_ref_pending;
  int source_alt_ref_active;
  int is_src_frame_alt_ref;
  int sframe_due;

  int high_source_sad;
  uint64_t avg_source_sad;

  int avg_frame_bandwidth;  // Average frame size target for clip
  int min_frame_bandwidth;  // Minimum allocation used for any frame
  int max_frame_bandwidth;  // Maximum burst rate allowed for a frame.
  int prev_avg_frame_bandwidth;

  int ni_av_qi;
  int ni_tot_qi;
  int ni_frames;
  int avg_frame_qindex[FRAME_TYPES];
  double tot_q;
  double avg_q;

  int64_t buffer_level;
  int64_t bits_off_target;
  int64_t vbr_bits_off_target;
  int64_t vbr_bits_off_target_fast;

  int decimation_factor;
  int decimation_count;

  int rolling_target_bits;
  int rolling_actual_bits;

  int long_rolling_target_bits;
  int long_rolling_actual_bits;

  int rate_error_estimate;

  int64_t total_actual_bits;
  int64_t total_target_bits;
  int64_t total_target_vs_actual;

  int worst_quality;
  int best_quality;

  int64_t starting_buffer_level;
  int64_t optimal_buffer_level;
  int64_t maximum_buffer_size;

  // rate control history for last frame(1) and the frame before(2).
  // -1: undershot
  //  1: overshoot
  //  0: not initialized.
  int rc_1_frame;
  int rc_2_frame;
  int q_1_frame;
  int q_2_frame;

  float_t arf_boost_factor;
  // Q index used for ALT frame
  int arf_q;
  int active_worst_quality;
  int active_best_quality[MAX_ARF_LAYERS + 1];
  int base_layer_qp;

  // Total number of stats used only for kf_boost calculation.
  int num_stats_used_for_kf_boost;
  // Total number of stats used only for gfu_boost calculation.
  int num_stats_used_for_gfu_boost;
  // Total number of stats required by gfu_boost calculation.
  int num_stats_required_for_gfu_boost;
  int next_is_fwd_key;
  int enable_scenecut_detection;
} RATE_CONTROL;

struct AV1_COMP;
struct AV1EncoderConfig;

void av1_rc_init(const struct AV1EncoderConfig *oxcf, int pass,
                 RATE_CONTROL *rc);

int av1_estimate_bits_at_q(FRAME_TYPE frame_kind, int q, int mbs,
                           double correction_factor, aom_bit_depth_t bit_depth);

double av1_convert_qindex_to_q(int qindex, aom_bit_depth_t bit_depth);

void av1_rc_init_minq_luts(void);

int av1_rc_get_default_min_gf_interval(int width, int height, double framerate);
// Note av1_rc_get_default_max_gf_interval() requires the min_gf_interval to
// be passed in to ensure that the max_gf_interval returned is at least as bis
// as that.
int av1_rc_get_default_max_gf_interval(double framerate, int min_gf_interval);

// Generally at the high level, the following flow is expected
// to be enforced for rate control:
// First call per frame, one of:
//   av1_rc_get_first_pass_params()
//   av1_rc_get_second_pass_params()
// depending on the usage to set the rate control encode parameters desired.
//
// Then, call encode_frame_to_data_rate() to perform the
// actual encode. This function will in turn call encode_frame()
// one or more times, followed by one of:
//   av1_rc_postencode_update()
//   av1_rc_postencode_update_drop_frame()
//
// The majority of rate control parameters are only expected
// to be set in the av1_rc_get_..._params() functions and
// updated during the av1_rc_postencode_update...() functions.
// The only exceptions are av1_rc_drop_frame() and
// av1_rc_update_rate_correction_factors() functions.

// Functions to set parameters for encoding before the actual
// encode_frame_to_data_rate() function.
struct EncodeFrameParams;

// Post encode update of the rate control parameters based
// on bytes used
void av1_rc_postencode_update(struct AV1_COMP *cpi, uint64_t bytes_used);
// Post encode update of the rate control parameters for dropped frames
void av1_rc_postencode_update_drop_frame(struct AV1_COMP *cpi);

// Updates rate correction factors
// Changes only the rate correction factors in the rate control structure.
void av1_rc_update_rate_correction_factors(struct AV1_COMP *cpi, int width,
                                           int height);

// Decide if we should drop this frame: For 1-pass CBR.
// Changes only the decimation count in the rate control structure
int av1_rc_drop_frame(struct AV1_COMP *cpi);

// Computes frame size bounds.
void av1_rc_compute_frame_size_bounds(const struct AV1_COMP *cpi,
                                      int this_frame_target,
                                      int *frame_under_shoot_limit,
                                      int *frame_over_shoot_limit);

// Picks q and q bounds given the target for bits
int av1_rc_pick_q_and_bounds(const struct AV1_COMP *cpi, RATE_CONTROL *rc,
                             int width, int height, int gf_index,
                             int *bottom_index, int *top_index);

// Estimates q to achieve a target bits per frame
int av1_rc_regulate_q(const struct AV1_COMP *cpi, int target_bits_per_frame,
                      int active_best_quality, int active_worst_quality,
                      int width, int height);

// Estimates bits per mb for a given qindex and correction factor.
int av1_rc_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                       double correction_factor, aom_bit_depth_t bit_depth);

// Clamping utilities for bitrate targets for iframes and pframes.
int av1_rc_clamp_iframe_target_size(const struct AV1_COMP *const cpi,
                                    int target);
int av1_rc_clamp_pframe_target_size(const struct AV1_COMP *const cpi,
                                    int target, uint8_t frame_update_type);

// Find q_index corresponding to desired_q, within [best_qindex, worst_qindex].
// To be precise, 'q_index' is the smallest integer, for which the corresponding
// q >= desired_q.
// If no such q index is found, returns 'worst_qindex'.
int av1_find_qindex(double desired_q, aom_bit_depth_t bit_depth,
                    int best_qindex, int worst_qindex);

// Computes a q delta (in "q index" terms) to get from a starting q value
// to a target q value
int av1_compute_qdelta(const RATE_CONTROL *rc, double qstart, double qtarget,
                       aom_bit_depth_t bit_depth);

// Computes a q delta (in "q index" terms) to get from a starting q value
// to a value that should equate to the given rate ratio.
int av1_compute_qdelta_by_rate(const RATE_CONTROL *rc, FRAME_TYPE frame_type,
                               int qindex, double rate_target_ratio,
                               aom_bit_depth_t bit_depth);

int av1_frame_type_qdelta(const struct AV1_COMP *cpi, int q);

void av1_rc_update_framerate(struct AV1_COMP *cpi, int width, int height);

void av1_rc_set_gf_interval_range(const struct AV1_COMP *const cpi,
                                  RATE_CONTROL *const rc);

void av1_set_target_rate(struct AV1_COMP *cpi, int width, int height);

int av1_resize_one_pass_cbr(struct AV1_COMP *cpi);

void av1_rc_set_frame_target(struct AV1_COMP *cpi, int target, int width,
                             int height);

int av1_calc_pframe_target_size_one_pass_vbr(
    const struct AV1_COMP *const cpi, FRAME_UPDATE_TYPE frame_update_type);

int av1_calc_iframe_target_size_one_pass_vbr(const struct AV1_COMP *const cpi);

int av1_calc_pframe_target_size_one_pass_cbr(
    const struct AV1_COMP *cpi, FRAME_UPDATE_TYPE frame_update_type);

int av1_calc_iframe_target_size_one_pass_cbr(const struct AV1_COMP *cpi);

void av1_get_one_pass_rt_params(struct AV1_COMP *cpi,
                                struct EncodeFrameParams *const frame_params,
                                unsigned int frame_flags);

int av1_encodedframe_overshoot(struct AV1_COMP *cpi, int *q);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_RATECTRL_H_
