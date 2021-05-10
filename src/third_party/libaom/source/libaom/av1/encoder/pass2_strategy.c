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

/*!\defgroup gf_group_algo Golden Frame Group
 * \ingroup high_level_algo
 * Algorithms regarding determining the length of GF groups and defining GF
 * group structures.
 * @{
 */
/*! @} - end defgroup gf_group_algo */

#include <stdint.h>

#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"

#include "aom_ports/system_state.h"

#include "av1/common/av1_common_int.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/gop_structure.h"
#include "av1/encoder/pass2_strategy.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rc_utils.h"
#include "av1/encoder/temporal_filter.h"
#include "av1/encoder/tpl_model.h"
#include "av1/encoder/use_flat_gop_model_params.h"
#include "av1/encoder/encode_strategy.h"

#define DEFAULT_KF_BOOST 2300
#define DEFAULT_GF_BOOST 2000
#define GROUP_ADAPTIVE_MAXQ 1
static void init_gf_stats(GF_GROUP_STATS *gf_stats);

// Calculate an active area of the image that discounts formatting
// bars and partially discounts other 0 energy areas.
#define MIN_ACTIVE_AREA 0.5
#define MAX_ACTIVE_AREA 1.0
static double calculate_active_area(const FRAME_INFO *frame_info,
                                    const FIRSTPASS_STATS *this_frame) {
  const double active_pct =
      1.0 -
      ((this_frame->intra_skip_pct / 2) +
       ((this_frame->inactive_zone_rows * 2) / (double)frame_info->mb_rows));
  return fclamp(active_pct, MIN_ACTIVE_AREA, MAX_ACTIVE_AREA);
}

// Calculate a modified Error used in distributing bits between easier and
// harder frames.
#define ACT_AREA_CORRECTION 0.5
static double calculate_modified_err(const FRAME_INFO *frame_info,
                                     const TWO_PASS *twopass,
                                     const AV1EncoderConfig *oxcf,
                                     const FIRSTPASS_STATS *this_frame) {
  const FIRSTPASS_STATS *const stats = twopass->stats_buf_ctx->total_stats;
  if (stats == NULL) {
    return 0;
  }
  const double av_weight = stats->weight / stats->count;
  const double av_err = (stats->coded_error * av_weight) / stats->count;
  double modified_error =
      av_err * pow(this_frame->coded_error * this_frame->weight /
                       DOUBLE_DIVIDE_CHECK(av_err),
                   oxcf->rc_cfg.vbrbias / 100.0);

  // Correction for active area. Frames with a reduced active area
  // (eg due to formatting bars) have a higher error per mb for the
  // remaining active MBs. The correction here assumes that coding
  // 0.5N blocks of complexity 2X is a little easier than coding N
  // blocks of complexity X.
  modified_error *=
      pow(calculate_active_area(frame_info, this_frame), ACT_AREA_CORRECTION);

  return fclamp(modified_error, twopass->modified_error_min,
                twopass->modified_error_max);
}

// Resets the first pass file to the given position using a relative seek from
// the current position.
static void reset_fpf_position(TWO_PASS *p, const FIRSTPASS_STATS *position) {
  p->stats_in = position;
}

static int input_stats(TWO_PASS *p, FIRSTPASS_STATS *fps) {
  if (p->stats_in >= p->stats_buf_ctx->stats_in_end) return EOF;

  *fps = *p->stats_in;
  ++p->stats_in;
  return 1;
}

static int input_stats_lap(TWO_PASS *p, FIRSTPASS_STATS *fps) {
  if (p->stats_in >= p->stats_buf_ctx->stats_in_end) return EOF;

  *fps = *p->stats_in;
  /* Move old stats[0] out to accommodate for next frame stats  */
  memmove(p->frame_stats_arr[0], p->frame_stats_arr[1],
          (p->stats_buf_ctx->stats_in_end - p->stats_in - 1) *
              sizeof(FIRSTPASS_STATS));
  p->stats_buf_ctx->stats_in_end--;
  return 1;
}

// Read frame stats at an offset from the current position.
static const FIRSTPASS_STATS *read_frame_stats(const TWO_PASS *p, int offset) {
  if ((offset >= 0 && p->stats_in + offset >= p->stats_buf_ctx->stats_in_end) ||
      (offset < 0 && p->stats_in + offset < p->stats_buf_ctx->stats_in_start)) {
    return NULL;
  }

  return &p->stats_in[offset];
}

static void subtract_stats(FIRSTPASS_STATS *section,
                           const FIRSTPASS_STATS *frame) {
  section->frame -= frame->frame;
  section->weight -= frame->weight;
  section->intra_error -= frame->intra_error;
  section->frame_avg_wavelet_energy -= frame->frame_avg_wavelet_energy;
  section->coded_error -= frame->coded_error;
  section->sr_coded_error -= frame->sr_coded_error;
  section->pcnt_inter -= frame->pcnt_inter;
  section->pcnt_motion -= frame->pcnt_motion;
  section->pcnt_second_ref -= frame->pcnt_second_ref;
  section->pcnt_neutral -= frame->pcnt_neutral;
  section->intra_skip_pct -= frame->intra_skip_pct;
  section->inactive_zone_rows -= frame->inactive_zone_rows;
  section->inactive_zone_cols -= frame->inactive_zone_cols;
  section->MVr -= frame->MVr;
  section->mvr_abs -= frame->mvr_abs;
  section->MVc -= frame->MVc;
  section->mvc_abs -= frame->mvc_abs;
  section->MVrv -= frame->MVrv;
  section->MVcv -= frame->MVcv;
  section->mv_in_out_count -= frame->mv_in_out_count;
  section->new_mv_count -= frame->new_mv_count;
  section->count -= frame->count;
  section->duration -= frame->duration;
}

// This function returns the maximum target rate per frame.
static int frame_max_bits(const RATE_CONTROL *rc,
                          const AV1EncoderConfig *oxcf) {
  int64_t max_bits = ((int64_t)rc->avg_frame_bandwidth *
                      (int64_t)oxcf->rc_cfg.vbrmax_section) /
                     100;
  if (max_bits < 0)
    max_bits = 0;
  else if (max_bits > rc->max_frame_bandwidth)
    max_bits = rc->max_frame_bandwidth;

  return (int)max_bits;
}

static const double q_pow_term[(QINDEX_RANGE >> 5) + 1] = { 0.65, 0.70, 0.75,
                                                            0.80, 0.85, 0.90,
                                                            0.95, 0.95, 0.95 };
#define ERR_DIVISOR 96.0
static double calc_correction_factor(double err_per_mb, int q) {
  const double error_term = err_per_mb / ERR_DIVISOR;
  const int index = q >> 5;
  // Adjustment to power term based on qindex
  const double power_term =
      q_pow_term[index] +
      (((q_pow_term[index + 1] - q_pow_term[index]) * (q % 32)) / 32.0);
  assert(error_term >= 0.0);
  return fclamp(pow(error_term, power_term), 0.05, 5.0);
}

// Based on history adjust expectations of bits per macroblock.
static void twopass_update_bpm_factor(AV1_COMP *cpi, int rate_err_tol) {
  TWO_PASS *twopass = &cpi->twopass;
  const RATE_CONTROL *const rc = &cpi->rc;
  int err_estimate = rc->rate_error_estimate;

  // Based on recent history adjust expectations of bits per macroblock.
  double damp_fac = AOMMAX(5.0, rate_err_tol / 10.0);
  double rate_err_factor = 1.0;
  const double adj_limit = AOMMAX(0.20, (double)(100 - rate_err_tol) / 200.0);
  const double min_fac = 1.0 - adj_limit;
  const double max_fac = 1.0 + adj_limit;

  if (rc->vbr_bits_off_target && rc->total_actual_bits > 0) {
    if (cpi->lap_enabled) {
      rate_err_factor =
          (double)twopass->rolling_arf_group_actual_bits /
          DOUBLE_DIVIDE_CHECK((double)twopass->rolling_arf_group_target_bits);
    } else {
      rate_err_factor =
          1.0 - ((double)(rc->vbr_bits_off_target) /
                 AOMMAX(rc->total_actual_bits, cpi->twopass.bits_left));
    }

    rate_err_factor = AOMMAX(min_fac, AOMMIN(max_fac, rate_err_factor));

    // Adjustment is damped if this is 1 pass with look ahead processing
    // (as there are only ever a few frames of data) and for all but the first
    // GOP in normal two pass.
    if ((twopass->bpm_factor != 1.0) || cpi->lap_enabled) {
      rate_err_factor = 1.0 + ((rate_err_factor - 1.0) / damp_fac);
    }
  }

  // Is the rate control trending in the right direction. Only make
  // an adjustment if things are getting worse.
  if ((rate_err_factor < 1.0 && err_estimate > 0) ||
      (rate_err_factor > 1.0 && err_estimate < 0)) {
    twopass->bpm_factor *= rate_err_factor;
    twopass->bpm_factor = AOMMAX(min_fac, AOMMIN(max_fac, twopass->bpm_factor));
  }
}

static int qbpm_enumerator(int rate_err_tol) {
  return 1200000 + ((300000 * AOMMIN(75, AOMMAX(rate_err_tol - 25, 0))) / 75);
}

// Similar to find_qindex_by_rate() function in ratectrl.c, but includes
// calculation of a correction_factor.
static int find_qindex_by_rate_with_correction(
    int desired_bits_per_mb, aom_bit_depth_t bit_depth, double error_per_mb,
    double group_weight_factor, int rate_err_tol, int best_qindex,
    int worst_qindex) {
  assert(best_qindex <= worst_qindex);
  int low = best_qindex;
  int high = worst_qindex;

  while (low < high) {
    const int mid = (low + high) >> 1;
    const double mid_factor = calc_correction_factor(error_per_mb, mid);
    const double q = av1_convert_qindex_to_q(mid, bit_depth);
    const int enumerator = qbpm_enumerator(rate_err_tol);
    const int mid_bits_per_mb =
        (int)((enumerator * mid_factor * group_weight_factor) / q);

    if (mid_bits_per_mb > desired_bits_per_mb) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return low;
}

/*!\brief Choose a target maximum Q for a group of frames
 *
 * \ingroup rate_control
 *
 * This function is used to estimate a suitable maximum Q for a
 * group of frames. Inititally it is called to get a crude estimate
 * for the whole clip. It is then called for each ARF/GF group to get
 * a revised estimate for that group.
 *
 * \param[in]    cpi                 Top-level encoder structure
 * \param[in]    av_frame_err        The average per frame coded error score
 *                                   for frames making up this section/group.
 * \param[in]    inactive_zone       Used to mask off /ignore part of the
 *                                   frame. The most common use case is where
 *                                   a wide format video (e.g. 16:9) is
 *                                   letter-boxed into a more square format.
 *                                   Here we want to ignore the bands at the
 *                                   top and bottom.
 * \param[in]    av_target_bandwidth The target bits per frame
 *
 * \return The maximum Q for frames in the group.
 */
static int get_twopass_worst_quality(AV1_COMP *cpi, const double av_frame_err,
                                     double inactive_zone,
                                     int av_target_bandwidth) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;
  inactive_zone = fclamp(inactive_zone, 0.0, 1.0);

  if (av_target_bandwidth <= 0) {
    return rc->worst_quality;  // Highest value allowed
  } else {
    const int num_mbs = (oxcf->resize_cfg.resize_mode != RESIZE_NONE)
                            ? cpi->initial_mbs
                            : cpi->common.mi_params.MBs;
    const int active_mbs = AOMMAX(1, num_mbs - (int)(num_mbs * inactive_zone));
    const double av_err_per_mb = av_frame_err / active_mbs;
    const int target_norm_bits_per_mb =
        (int)((uint64_t)av_target_bandwidth << BPER_MB_NORMBITS) / active_mbs;
    int rate_err_tol = AOMMIN(rc_cfg->under_shoot_pct, rc_cfg->over_shoot_pct);

    // Update bpm correction factor based on previous GOP rate error.
    twopass_update_bpm_factor(cpi, rate_err_tol);

    // Try and pick a max Q that will be high enough to encode the
    // content at the given rate.
    int q = find_qindex_by_rate_with_correction(
        target_norm_bits_per_mb, cpi->common.seq_params.bit_depth,
        av_err_per_mb, cpi->twopass.bpm_factor, rate_err_tol, rc->best_quality,
        rc->worst_quality);

    // Restriction on active max q for constrained quality mode.
    if (rc_cfg->mode == AOM_CQ) q = AOMMAX(q, rc_cfg->cq_level);
    return q;
  }
}

#define SR_DIFF_PART 0.0015
#define MOTION_AMP_PART 0.003
#define INTRA_PART 0.005
#define DEFAULT_DECAY_LIMIT 0.75
#define LOW_SR_DIFF_TRHESH 0.1
#define SR_DIFF_MAX 128.0
#define NCOUNT_FRAME_II_THRESH 5.0

static double get_sr_decay_rate(const FRAME_INFO *frame_info,
                                const FIRSTPASS_STATS *frame) {
  const int num_mbs = frame_info->num_mbs;
  double sr_diff = (frame->sr_coded_error - frame->coded_error) / num_mbs;
  double sr_decay = 1.0;
  double modified_pct_inter;
  double modified_pcnt_intra;
  const double motion_amplitude_factor =
      frame->pcnt_motion * ((frame->mvc_abs + frame->mvr_abs) / 2);

  modified_pct_inter = frame->pcnt_inter;
  if ((frame->intra_error / DOUBLE_DIVIDE_CHECK(frame->coded_error)) <
      (double)NCOUNT_FRAME_II_THRESH) {
    modified_pct_inter = frame->pcnt_inter - frame->pcnt_neutral;
  }
  modified_pcnt_intra = 100 * (1.0 - modified_pct_inter);

  if ((sr_diff > LOW_SR_DIFF_TRHESH)) {
    sr_diff = AOMMIN(sr_diff, SR_DIFF_MAX);
    sr_decay = 1.0 - (SR_DIFF_PART * sr_diff) -
               (MOTION_AMP_PART * motion_amplitude_factor) -
               (INTRA_PART * modified_pcnt_intra);
  }
  return AOMMAX(sr_decay, AOMMIN(DEFAULT_DECAY_LIMIT, modified_pct_inter));
}

// This function gives an estimate of how badly we believe the prediction
// quality is decaying from frame to frame.
static double get_zero_motion_factor(const FRAME_INFO *frame_info,
                                     const FIRSTPASS_STATS *frame) {
  const double zero_motion_pct = frame->pcnt_inter - frame->pcnt_motion;
  double sr_decay = get_sr_decay_rate(frame_info, frame);
  return AOMMIN(sr_decay, zero_motion_pct);
}

#define ZM_POWER_FACTOR 0.75

static double get_prediction_decay_rate(const FRAME_INFO *frame_info,
                                        const FIRSTPASS_STATS *next_frame) {
  const double sr_decay_rate = get_sr_decay_rate(frame_info, next_frame);
  const double zero_motion_factor =
      (0.95 * pow((next_frame->pcnt_inter - next_frame->pcnt_motion),
                  ZM_POWER_FACTOR));

  return AOMMAX(zero_motion_factor,
                (sr_decay_rate + ((1.0 - sr_decay_rate) * zero_motion_factor)));
}

// Function to test for a condition where a complex transition is followed
// by a static section. For example in slide shows where there is a fade
// between slides. This is to help with more optimal kf and gf positioning.
static int detect_transition_to_still(TWO_PASS *const twopass,
                                      const int min_gf_interval,
                                      const int frame_interval,
                                      const int still_interval,
                                      const double loop_decay_rate,
                                      const double last_decay_rate) {
  // Break clause to detect very still sections after motion
  // For example a static image after a fade or other transition
  // instead of a clean scene cut.
  if (frame_interval > min_gf_interval && loop_decay_rate >= 0.999 &&
      last_decay_rate < 0.9) {
    int j;
    // Look ahead a few frames to see if static condition persists...
    for (j = 0; j < still_interval; ++j) {
      const FIRSTPASS_STATS *stats = &twopass->stats_in[j];
      if (stats >= twopass->stats_buf_ctx->stats_in_end) break;

      if (stats->pcnt_inter - stats->pcnt_motion < 0.999) break;
    }
    // Only if it does do we signal a transition to still.
    return j == still_interval;
  }
  return 0;
}

// This function detects a flash through the high relative pcnt_second_ref
// score in the frame following a flash frame. The offset passed in should
// reflect this.
static int detect_flash(const TWO_PASS *twopass, const int offset) {
  const FIRSTPASS_STATS *const next_frame = read_frame_stats(twopass, offset);

  // What we are looking for here is a situation where there is a
  // brief break in prediction (such as a flash) but subsequent frames
  // are reasonably well predicted by an earlier (pre flash) frame.
  // The recovery after a flash is indicated by a high pcnt_second_ref
  // compared to pcnt_inter.
  return next_frame != NULL &&
         next_frame->pcnt_second_ref > next_frame->pcnt_inter &&
         next_frame->pcnt_second_ref >= 0.5;
}

// Update the motion related elements to the GF arf boost calculation.
static void accumulate_frame_motion_stats(const FIRSTPASS_STATS *stats,
                                          GF_GROUP_STATS *gf_stats) {
  const double pct = stats->pcnt_motion;

  // Accumulate Motion In/Out of frame stats.
  gf_stats->this_frame_mv_in_out = stats->mv_in_out_count * pct;
  gf_stats->mv_in_out_accumulator += gf_stats->this_frame_mv_in_out;
  gf_stats->abs_mv_in_out_accumulator += fabs(gf_stats->this_frame_mv_in_out);

  // Accumulate a measure of how uniform (or conversely how random) the motion
  // field is (a ratio of abs(mv) / mv).
  if (pct > 0.05) {
    const double mvr_ratio =
        fabs(stats->mvr_abs) / DOUBLE_DIVIDE_CHECK(fabs(stats->MVr));
    const double mvc_ratio =
        fabs(stats->mvc_abs) / DOUBLE_DIVIDE_CHECK(fabs(stats->MVc));

    gf_stats->mv_ratio_accumulator +=
        pct * (mvr_ratio < stats->mvr_abs ? mvr_ratio : stats->mvr_abs);
    gf_stats->mv_ratio_accumulator +=
        pct * (mvc_ratio < stats->mvc_abs ? mvc_ratio : stats->mvc_abs);
  }
}

static void accumulate_this_frame_stats(const FIRSTPASS_STATS *stats,
                                        const double mod_frame_err,
                                        GF_GROUP_STATS *gf_stats) {
  gf_stats->gf_group_err += mod_frame_err;
#if GROUP_ADAPTIVE_MAXQ
  gf_stats->gf_group_raw_error += stats->coded_error;
#endif
  gf_stats->gf_group_skip_pct += stats->intra_skip_pct;
  gf_stats->gf_group_inactive_zone_rows += stats->inactive_zone_rows;
}

static void accumulate_next_frame_stats(const FIRSTPASS_STATS *stats,
                                        const FRAME_INFO *frame_info,
                                        const int flash_detected,
                                        const int frames_since_key,
                                        const int cur_idx,
                                        GF_GROUP_STATS *gf_stats) {
  accumulate_frame_motion_stats(stats, gf_stats);
  // sum up the metric values of current gf group
  gf_stats->avg_sr_coded_error += stats->sr_coded_error;
  gf_stats->avg_tr_coded_error += stats->tr_coded_error;
  gf_stats->avg_pcnt_second_ref += stats->pcnt_second_ref;
  gf_stats->avg_pcnt_third_ref += stats->pcnt_third_ref;
  gf_stats->avg_new_mv_count += stats->new_mv_count;
  gf_stats->avg_wavelet_energy += stats->frame_avg_wavelet_energy;
  if (fabs(stats->raw_error_stdev) > 0.000001) {
    gf_stats->non_zero_stdev_count++;
    gf_stats->avg_raw_err_stdev += stats->raw_error_stdev;
  }

  // Accumulate the effect of prediction quality decay
  if (!flash_detected) {
    gf_stats->last_loop_decay_rate = gf_stats->loop_decay_rate;
    gf_stats->loop_decay_rate = get_prediction_decay_rate(frame_info, stats);

    gf_stats->decay_accumulator =
        gf_stats->decay_accumulator * gf_stats->loop_decay_rate;

    // Monitor for static sections.
    if ((frames_since_key + cur_idx - 1) > 1) {
      gf_stats->zero_motion_accumulator =
          AOMMIN(gf_stats->zero_motion_accumulator,
                 get_zero_motion_factor(frame_info, stats));
    }
  }
}

static void average_gf_stats(const int total_frame,
                             const FIRSTPASS_STATS *last_stat,
                             GF_GROUP_STATS *gf_stats) {
  if (total_frame) {
    gf_stats->avg_sr_coded_error /= total_frame;
    gf_stats->avg_tr_coded_error /= total_frame;
    gf_stats->avg_pcnt_second_ref /= total_frame;
    if (total_frame - 1) {
      gf_stats->avg_pcnt_third_ref_nolast =
          (gf_stats->avg_pcnt_third_ref - last_stat->pcnt_third_ref) /
          (total_frame - 1);
    } else {
      gf_stats->avg_pcnt_third_ref_nolast =
          gf_stats->avg_pcnt_third_ref / total_frame;
    }
    gf_stats->avg_pcnt_third_ref /= total_frame;
    gf_stats->avg_new_mv_count /= total_frame;
    gf_stats->avg_wavelet_energy /= total_frame;
  }

  if (gf_stats->non_zero_stdev_count)
    gf_stats->avg_raw_err_stdev /= gf_stats->non_zero_stdev_count;
}

static void get_features_from_gf_stats(const GF_GROUP_STATS *gf_stats,
                                       const GF_FRAME_STATS *first_frame,
                                       const GF_FRAME_STATS *last_frame,
                                       const int num_mbs,
                                       const int constrained_gf_group,
                                       const int kf_zeromotion_pct,
                                       const int num_frames, float *features) {
  *features++ = (float)gf_stats->abs_mv_in_out_accumulator;
  *features++ = (float)(gf_stats->avg_new_mv_count / num_mbs);
  *features++ = (float)gf_stats->avg_pcnt_second_ref;
  *features++ = (float)gf_stats->avg_pcnt_third_ref;
  *features++ = (float)gf_stats->avg_pcnt_third_ref_nolast;
  *features++ = (float)(gf_stats->avg_sr_coded_error / num_mbs);
  *features++ = (float)(gf_stats->avg_tr_coded_error / num_mbs);
  *features++ = (float)(gf_stats->avg_wavelet_energy / num_mbs);
  *features++ = (float)(constrained_gf_group);
  *features++ = (float)gf_stats->decay_accumulator;
  *features++ = (float)(first_frame->frame_coded_error / num_mbs);
  *features++ = (float)(first_frame->frame_sr_coded_error / num_mbs);
  *features++ = (float)(first_frame->frame_tr_coded_error / num_mbs);
  *features++ = (float)(first_frame->frame_err / num_mbs);
  *features++ = (float)(kf_zeromotion_pct);
  *features++ = (float)(last_frame->frame_coded_error / num_mbs);
  *features++ = (float)(last_frame->frame_sr_coded_error / num_mbs);
  *features++ = (float)(last_frame->frame_tr_coded_error / num_mbs);
  *features++ = (float)num_frames;
  *features++ = (float)gf_stats->mv_ratio_accumulator;
  *features++ = (float)gf_stats->non_zero_stdev_count;
}

#define BOOST_FACTOR 12.5
static double baseline_err_per_mb(const FRAME_INFO *frame_info) {
  unsigned int screen_area = frame_info->frame_height * frame_info->frame_width;

  // Use a different error per mb factor for calculating boost for
  //  different formats.
  if (screen_area <= 640 * 360) {
    return 500.0;
  } else {
    return 1000.0;
  }
}

static double calc_frame_boost(const RATE_CONTROL *rc,
                               const FRAME_INFO *frame_info,
                               const FIRSTPASS_STATS *this_frame,
                               double this_frame_mv_in_out, double max_boost) {
  double frame_boost;
  const double lq = av1_convert_qindex_to_q(rc->avg_frame_qindex[INTER_FRAME],
                                            frame_info->bit_depth);
  const double boost_q_correction = AOMMIN((0.5 + (lq * 0.015)), 1.5);
  const double active_area = calculate_active_area(frame_info, this_frame);
  int num_mbs = frame_info->num_mbs;

  // Correct for any inactive region in the image
  num_mbs = (int)AOMMAX(1, num_mbs * active_area);

  // Underlying boost factor is based on inter error ratio.
  frame_boost = AOMMAX(baseline_err_per_mb(frame_info) * num_mbs,
                       this_frame->intra_error * active_area) /
                DOUBLE_DIVIDE_CHECK(this_frame->coded_error);
  frame_boost = frame_boost * BOOST_FACTOR * boost_q_correction;

  // Increase boost for frames where new data coming into frame (e.g. zoom out).
  // Slightly reduce boost if there is a net balance of motion out of the frame
  // (zoom in). The range for this_frame_mv_in_out is -1.0 to +1.0.
  if (this_frame_mv_in_out > 0.0)
    frame_boost += frame_boost * (this_frame_mv_in_out * 2.0);
  // In the extreme case the boost is halved.
  else
    frame_boost += frame_boost * (this_frame_mv_in_out / 2.0);

  return AOMMIN(frame_boost, max_boost * boost_q_correction);
}

static double calc_kf_frame_boost(const RATE_CONTROL *rc,
                                  const FRAME_INFO *frame_info,
                                  const FIRSTPASS_STATS *this_frame,
                                  double *sr_accumulator, double max_boost) {
  double frame_boost;
  const double lq = av1_convert_qindex_to_q(rc->avg_frame_qindex[INTER_FRAME],
                                            frame_info->bit_depth);
  const double boost_q_correction = AOMMIN((0.50 + (lq * 0.015)), 2.00);
  const double active_area = calculate_active_area(frame_info, this_frame);
  int num_mbs = frame_info->num_mbs;

  // Correct for any inactive region in the image
  num_mbs = (int)AOMMAX(1, num_mbs * active_area);

  // Underlying boost factor is based on inter error ratio.
  frame_boost = AOMMAX(baseline_err_per_mb(frame_info) * num_mbs,
                       this_frame->intra_error * active_area) /
                DOUBLE_DIVIDE_CHECK(
                    (this_frame->coded_error + *sr_accumulator) * active_area);

  // Update the accumulator for second ref error difference.
  // This is intended to give an indication of how much the coded error is
  // increasing over time.
  *sr_accumulator += (this_frame->sr_coded_error - this_frame->coded_error);
  *sr_accumulator = AOMMAX(0.0, *sr_accumulator);

  // Q correction and scaling
  // The 40.0 value here is an experimentally derived baseline minimum.
  // This value is in line with the minimum per frame boost in the alt_ref
  // boost calculation.
  frame_boost = ((frame_boost + 40.0) * boost_q_correction);

  return AOMMIN(frame_boost, max_boost * boost_q_correction);
}

static int get_projected_gfu_boost(const RATE_CONTROL *rc, int gfu_boost,
                                   int frames_to_project,
                                   int num_stats_used_for_gfu_boost) {
  /*
   * If frames_to_project is equal to num_stats_used_for_gfu_boost,
   * it means that gfu_boost was calculated over frames_to_project to
   * begin with(ie; all stats required were available), hence return
   * the original boost.
   */
  if (num_stats_used_for_gfu_boost >= frames_to_project) return gfu_boost;

  double min_boost_factor = sqrt(rc->baseline_gf_interval);
  // Get the current tpl factor (number of frames = frames_to_project).
  double tpl_factor = av1_get_gfu_boost_projection_factor(
      min_boost_factor, MAX_GFUBOOST_FACTOR, frames_to_project);
  // Get the tpl factor when number of frames = num_stats_used_for_prior_boost.
  double tpl_factor_num_stats = av1_get_gfu_boost_projection_factor(
      min_boost_factor, MAX_GFUBOOST_FACTOR, num_stats_used_for_gfu_boost);
  int projected_gfu_boost =
      (int)rint((tpl_factor * gfu_boost) / tpl_factor_num_stats);
  return projected_gfu_boost;
}

#define GF_MAX_BOOST 90.0
#define MIN_DECAY_FACTOR 0.01
int av1_calc_arf_boost(const TWO_PASS *twopass, const RATE_CONTROL *rc,
                       FRAME_INFO *frame_info, int offset, int f_frames,
                       int b_frames, int *num_fpstats_used,
                       int *num_fpstats_required) {
  int i;
  GF_GROUP_STATS gf_stats;
  init_gf_stats(&gf_stats);
  double boost_score = (double)NORMAL_BOOST;
  int arf_boost;
  int flash_detected = 0;
  if (num_fpstats_used) *num_fpstats_used = 0;

  // Search forward from the proposed arf/next gf position.
  for (i = 0; i < f_frames; ++i) {
    const FIRSTPASS_STATS *this_frame = read_frame_stats(twopass, i + offset);
    if (this_frame == NULL) break;

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(this_frame, &gf_stats);

    // We want to discount the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, i + offset) ||
                     detect_flash(twopass, i + offset + 1);

    // Accumulate the effect of prediction quality decay.
    if (!flash_detected) {
      gf_stats.decay_accumulator *=
          get_prediction_decay_rate(frame_info, this_frame);
      gf_stats.decay_accumulator = gf_stats.decay_accumulator < MIN_DECAY_FACTOR
                                       ? MIN_DECAY_FACTOR
                                       : gf_stats.decay_accumulator;
    }

    boost_score +=
        gf_stats.decay_accumulator *
        calc_frame_boost(rc, frame_info, this_frame,
                         gf_stats.this_frame_mv_in_out, GF_MAX_BOOST);
    if (num_fpstats_used) (*num_fpstats_used)++;
  }

  arf_boost = (int)boost_score;

  // Reset for backward looking loop.
  boost_score = 0.0;
  init_gf_stats(&gf_stats);
  // Search backward towards last gf position.
  for (i = -1; i >= -b_frames; --i) {
    const FIRSTPASS_STATS *this_frame = read_frame_stats(twopass, i + offset);
    if (this_frame == NULL) break;

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(this_frame, &gf_stats);

    // We want to discount the the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, i + offset) ||
                     detect_flash(twopass, i + offset + 1);

    // Cumulative effect of prediction quality decay.
    if (!flash_detected) {
      gf_stats.decay_accumulator *=
          get_prediction_decay_rate(frame_info, this_frame);
      gf_stats.decay_accumulator = gf_stats.decay_accumulator < MIN_DECAY_FACTOR
                                       ? MIN_DECAY_FACTOR
                                       : gf_stats.decay_accumulator;
    }

    boost_score +=
        gf_stats.decay_accumulator *
        calc_frame_boost(rc, frame_info, this_frame,
                         gf_stats.this_frame_mv_in_out, GF_MAX_BOOST);
    if (num_fpstats_used) (*num_fpstats_used)++;
  }
  arf_boost += (int)boost_score;

  if (num_fpstats_required) {
    *num_fpstats_required = f_frames + b_frames;
    if (num_fpstats_used) {
      arf_boost = get_projected_gfu_boost(rc, arf_boost, *num_fpstats_required,
                                          *num_fpstats_used);
    }
  }

  if (arf_boost < ((b_frames + f_frames) * 50))
    arf_boost = ((b_frames + f_frames) * 50);

  return arf_boost;
}

// Calculate a section intra ratio used in setting max loop filter.
static int calculate_section_intra_ratio(const FIRSTPASS_STATS *begin,
                                         const FIRSTPASS_STATS *end,
                                         int section_length) {
  const FIRSTPASS_STATS *s = begin;
  double intra_error = 0.0;
  double coded_error = 0.0;
  int i = 0;

  while (s < end && i < section_length) {
    intra_error += s->intra_error;
    coded_error += s->coded_error;
    ++s;
    ++i;
  }

  return (int)(intra_error / DOUBLE_DIVIDE_CHECK(coded_error));
}

/*!\brief Calculates the bit target for this GF/ARF group
 *
 * \ingroup rate_control
 *
 * Calculates the total bits to allocate in this GF/ARF group.
 *
 * \param[in]    cpi              Top-level encoder structure
 * \param[in]    gf_group_err     Cumulative coded error score for the
 *                                frames making up this group.
 *
 * \return The target total number of bits for this GF/ARF group.
 */
static int64_t calculate_total_gf_group_bits(AV1_COMP *cpi,
                                             double gf_group_err) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const TWO_PASS *const twopass = &cpi->twopass;
  const int max_bits = frame_max_bits(rc, &cpi->oxcf);
  int64_t total_group_bits;

  // Calculate the bits to be allocated to the group as a whole.
  if ((twopass->kf_group_bits > 0) && (twopass->kf_group_error_left > 0)) {
    total_group_bits = (int64_t)(twopass->kf_group_bits *
                                 (gf_group_err / twopass->kf_group_error_left));
  } else {
    total_group_bits = 0;
  }

  // Clamp odd edge cases.
  total_group_bits = (total_group_bits < 0)
                         ? 0
                         : (total_group_bits > twopass->kf_group_bits)
                               ? twopass->kf_group_bits
                               : total_group_bits;

  // Clip based on user supplied data rate variability limit.
  if (total_group_bits > (int64_t)max_bits * rc->baseline_gf_interval)
    total_group_bits = (int64_t)max_bits * rc->baseline_gf_interval;

  return total_group_bits;
}

// Calculate the number of bits to assign to boosted frames in a group.
static int calculate_boost_bits(int frame_count, int boost,
                                int64_t total_group_bits) {
  int allocation_chunks;

  // return 0 for invalid inputs (could arise e.g. through rounding errors)
  if (!boost || (total_group_bits <= 0)) return 0;

  if (frame_count <= 0) return (int)(AOMMIN(total_group_bits, INT_MAX));

  allocation_chunks = (frame_count * 100) + boost;

  // Prevent overflow.
  if (boost > 1023) {
    int divisor = boost >> 10;
    boost /= divisor;
    allocation_chunks /= divisor;
  }

  // Calculate the number of extra bits for use in the boosted frame or frames.
  return AOMMAX((int)(((int64_t)boost * total_group_bits) / allocation_chunks),
                0);
}

// Calculate the boost factor based on the number of bits assigned, i.e. the
// inverse of calculate_boost_bits().
static int calculate_boost_factor(int frame_count, int bits,
                                  int64_t total_group_bits) {
  aom_clear_system_state();
  return (int)(100.0 * frame_count * bits / (total_group_bits - bits));
}

// Reduce the number of bits assigned to keyframe or arf if necessary, to
// prevent bitrate spikes that may break level constraints.
// frame_type: 0: keyframe; 1: arf.
static int adjust_boost_bits_for_target_level(const AV1_COMP *const cpi,
                                              RATE_CONTROL *const rc,
                                              int bits_assigned,
                                              int64_t group_bits,
                                              int frame_type) {
  const AV1_COMMON *const cm = &cpi->common;
  const SequenceHeader *const seq_params = &cm->seq_params;
  const int temporal_layer_id = cm->temporal_layer_id;
  const int spatial_layer_id = cm->spatial_layer_id;
  for (int index = 0; index < seq_params->operating_points_cnt_minus_1 + 1;
       ++index) {
    if (!is_in_operating_point(seq_params->operating_point_idc[index],
                               temporal_layer_id, spatial_layer_id)) {
      continue;
    }

    const AV1_LEVEL target_level =
        cpi->level_params.target_seq_level_idx[index];
    if (target_level >= SEQ_LEVELS) continue;

    assert(is_valid_seq_level_idx(target_level));

    const double level_bitrate_limit = av1_get_max_bitrate_for_level(
        target_level, seq_params->tier[0], seq_params->profile);
    const int target_bits_per_frame =
        (int)(level_bitrate_limit / cpi->framerate);
    if (frame_type == 0) {
      // Maximum bits for keyframe is 8 times the target_bits_per_frame.
      const int level_enforced_max_kf_bits = target_bits_per_frame * 8;
      if (bits_assigned > level_enforced_max_kf_bits) {
        const int frames = rc->frames_to_key - 1;
        rc->kf_boost = calculate_boost_factor(
            frames, level_enforced_max_kf_bits, group_bits);
        bits_assigned = calculate_boost_bits(frames, rc->kf_boost, group_bits);
      }
    } else if (frame_type == 1) {
      // Maximum bits for arf is 4 times the target_bits_per_frame.
      const int level_enforced_max_arf_bits = target_bits_per_frame * 4;
      if (bits_assigned > level_enforced_max_arf_bits) {
        rc->gfu_boost = calculate_boost_factor(
            rc->baseline_gf_interval, level_enforced_max_arf_bits, group_bits);
        bits_assigned = calculate_boost_bits(rc->baseline_gf_interval,
                                             rc->gfu_boost, group_bits);
      }
    } else {
      assert(0);
    }
  }

  return bits_assigned;
}

// Allocate bits to each frame in a GF / ARF group
double layer_fraction[MAX_ARF_LAYERS + 1] = { 1.0,  0.70, 0.55, 0.60,
                                              0.60, 1.0,  1.0 };
static void allocate_gf_group_bits(GF_GROUP *gf_group, RATE_CONTROL *const rc,
                                   int64_t gf_group_bits, int gf_arf_bits,
                                   int key_frame, int use_arf) {
  int64_t total_group_bits = gf_group_bits;
  int base_frame_bits;
  const int gf_group_size = gf_group->size;
  int layer_frames[MAX_ARF_LAYERS + 1] = { 0 };

  // For key frames the frame target rate is already set and it
  // is also the golden frame.
  // === [frame_index == 0] ===
  int frame_index = !!key_frame;

  // Subtract the extra bits set aside for ARF frames from the Group Total
  if (use_arf) total_group_bits -= gf_arf_bits;

  int num_frames =
      AOMMAX(1, rc->baseline_gf_interval - (rc->frames_since_key == 0));
  base_frame_bits = (int)(total_group_bits / num_frames);

  // Check the number of frames in each layer in case we have a
  // non standard group length.
  int max_arf_layer = gf_group->max_layer_depth - 1;
  for (int idx = frame_index; idx < gf_group_size; ++idx) {
    if ((gf_group->update_type[idx] == ARF_UPDATE) ||
        (gf_group->update_type[idx] == INTNL_ARF_UPDATE)) {
      layer_frames[gf_group->layer_depth[idx]]++;
    }
  }

  // Allocate extra bits to each ARF layer
  int i;
  int layer_extra_bits[MAX_ARF_LAYERS + 1] = { 0 };
  for (i = 1; i <= max_arf_layer; ++i) {
    double fraction = (i == max_arf_layer) ? 1.0 : layer_fraction[i];
    layer_extra_bits[i] =
        (int)((gf_arf_bits * fraction) / AOMMAX(1, layer_frames[i]));
    gf_arf_bits -= (int)(gf_arf_bits * fraction);
  }

  // Now combine ARF layer and baseline bits to give total bits for each frame.
  int arf_extra_bits;
  for (int idx = frame_index; idx < gf_group_size; ++idx) {
    switch (gf_group->update_type[idx]) {
      case ARF_UPDATE:
      case INTNL_ARF_UPDATE:
        arf_extra_bits = layer_extra_bits[gf_group->layer_depth[idx]];
        gf_group->bit_allocation[idx] = base_frame_bits + arf_extra_bits;
        break;
      case INTNL_OVERLAY_UPDATE:
      case OVERLAY_UPDATE: gf_group->bit_allocation[idx] = 0; break;
      default: gf_group->bit_allocation[idx] = base_frame_bits; break;
    }
  }

  // Set the frame following the current GOP to 0 bit allocation. For ARF
  // groups, this next frame will be overlay frame, which is the first frame
  // in the next GOP. For GF group, next GOP will overwrite the rate allocation.
  // Setting this frame to use 0 bit (of out the current GOP budget) will
  // simplify logics in reference frame management.
  gf_group->bit_allocation[gf_group_size] = 0;
}

// Returns true if KF group and GF group both are almost completely static.
static INLINE int is_almost_static(double gf_zero_motion, int kf_zero_motion,
                                   int is_lap_enabled) {
  if (is_lap_enabled) {
    /*
     * when LAP enabled kf_zero_motion is not reliable, so use strict
     * constraint on gf_zero_motion.
     */
    return (gf_zero_motion >= 0.999);
  } else {
    return (gf_zero_motion >= 0.995) &&
           (kf_zero_motion >= STATIC_KF_GROUP_THRESH);
  }
}

#define ARF_ABS_ZOOM_THRESH 4.4
static INLINE int detect_gf_cut(AV1_COMP *cpi, int frame_index, int cur_start,
                                int flash_detected, int active_max_gf_interval,
                                int active_min_gf_interval,
                                GF_GROUP_STATS *gf_stats) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  InitialDimensions *const initial_dimensions = &cpi->initial_dimensions;
  // Motion breakout threshold for loop below depends on image size.
  const double mv_ratio_accumulator_thresh =
      (initial_dimensions->height + initial_dimensions->width) / 4.0;

  if (!flash_detected) {
    // Break clause to detect very still sections after motion. For example,
    // a static image after a fade or other transition.
    if (detect_transition_to_still(
            twopass, rc->min_gf_interval, frame_index - cur_start, 5,
            gf_stats->loop_decay_rate, gf_stats->last_loop_decay_rate)) {
      return 1;
    }
  }

  // Some conditions to breakout after min interval.
  if (frame_index - cur_start >= active_min_gf_interval &&
      // If possible don't break very close to a kf
      (rc->frames_to_key - frame_index >= rc->min_gf_interval) &&
      ((frame_index - cur_start) & 0x01) && !flash_detected &&
      (gf_stats->mv_ratio_accumulator > mv_ratio_accumulator_thresh ||
       gf_stats->abs_mv_in_out_accumulator > ARF_ABS_ZOOM_THRESH)) {
    return 1;
  }

  // If almost totally static, we will not use the the max GF length later,
  // so we can continue for more frames.
  if (((frame_index - cur_start) >= active_max_gf_interval + 1) &&
      !is_almost_static(gf_stats->zero_motion_accumulator,
                        twopass->kf_zeromotion_pct, cpi->lap_enabled)) {
    return 1;
  }
  return 0;
}

#define MIN_FWD_KF_INTERVAL 8
#define MIN_SHRINK_LEN 6  // the minimum length of gf if we are shrinking
#define SMOOTH_FILT_LEN 7
#define HALF_FILT_LEN (SMOOTH_FILT_LEN / 2)
#define WINDOW_SIZE 7
#define HALF_WIN (WINDOW_SIZE / 2)
// A 7-tap gaussian smooth filter
const double smooth_filt[SMOOTH_FILT_LEN] = { 0.006, 0.061, 0.242, 0.383,
                                              0.242, 0.061, 0.006 };

// Smooth filter intra_error and coded_error in firstpass stats.
// If ignore[i]==1, the ith element should not be used in the filtering.
static void smooth_filter_stats(const FIRSTPASS_STATS *stats, const int *ignore,
                                int start_idx, int last_idx,
                                double *filt_intra_err,
                                double *filt_coded_err) {
  int i, j;
  for (i = start_idx; i <= last_idx; i++) {
    double total_wt = 0;
    for (j = -HALF_FILT_LEN; j <= HALF_FILT_LEN; j++) {
      int idx = AOMMIN(AOMMAX(i + j, start_idx), last_idx);
      if (ignore[idx]) continue;

      filt_intra_err[i] +=
          smooth_filt[j + HALF_FILT_LEN] * stats[idx].intra_error;
      total_wt += smooth_filt[j + HALF_FILT_LEN];
    }
    if (total_wt > 0.01) {
      filt_intra_err[i] /= total_wt;
    } else {
      filt_intra_err[i] = stats[i].intra_error;
    }
  }
  for (i = start_idx; i <= last_idx; i++) {
    double total_wt = 0;
    for (j = -HALF_FILT_LEN; j <= HALF_FILT_LEN; j++) {
      int idx = AOMMIN(AOMMAX(i + j, start_idx), last_idx);
      // Coded error involves idx and idx - 1.
      if (ignore[idx] || (idx > 0 && ignore[idx - 1])) continue;

      filt_coded_err[i] +=
          smooth_filt[j + HALF_FILT_LEN] * stats[idx].coded_error;
      total_wt += smooth_filt[j + HALF_FILT_LEN];
    }
    if (total_wt > 0.01) {
      filt_coded_err[i] /= total_wt;
    } else {
      filt_coded_err[i] = stats[i].coded_error;
    }
  }
}

// Calculate gradient
static void get_gradient(const double *values, int start, int last,
                         double *grad) {
  if (start == last) {
    grad[start] = 0;
    return;
  }
  for (int i = start; i <= last; i++) {
    int prev = AOMMAX(i - 1, start);
    int next = AOMMIN(i + 1, last);
    grad[i] = (values[next] - values[prev]) / (next - prev);
  }
}

static int find_next_scenecut(const FIRSTPASS_STATS *const stats_start,
                              int first, int last, int *ignore) {
  // Identify unstable areas caused by scenecuts.
  // Find the max and 2nd max coded error, and the average of the rest frames.
  // If there is only one frame that yields a huge coded error, it is likely a
  // scenecut.
  double this_ratio, max_prev_ratio, max_next_ratio, max_prev_coded,
      max_next_coded;

  if (last - first == 0) return -1;

  for (int i = first; i <= last; i++) {
    if (ignore[i] || (i > 0 && ignore[i - 1])) continue;
    double temp_intra = AOMMAX(stats_start[i].intra_error, 0.01);
    this_ratio = stats_start[i].coded_error / temp_intra;
    // find the avg ratio in the preceding neighborhood
    max_prev_ratio = 0;
    max_prev_coded = 0;
    for (int j = AOMMAX(first, i - HALF_WIN); j < i; j++) {
      if (ignore[j] || (j > 0 && ignore[j - 1])) continue;
      temp_intra = AOMMAX(stats_start[j].intra_error, 0.01);
      double temp_ratio = stats_start[j].coded_error / temp_intra;
      if (temp_ratio > max_prev_ratio) {
        max_prev_ratio = temp_ratio;
      }
      if (stats_start[j].coded_error > max_prev_coded) {
        max_prev_coded = stats_start[j].coded_error;
      }
    }
    // find the avg ratio in the following neighborhood
    max_next_ratio = 0;
    max_next_coded = 0;
    for (int j = i + 1; j <= AOMMIN(i + HALF_WIN, last); j++) {
      if (ignore[j] || (j > 0 && ignore[j - 1])) continue;
      temp_intra = AOMMAX(stats_start[j].intra_error, 0.01);
      double temp_ratio = stats_start[j].coded_error / temp_intra;
      if (temp_ratio > max_next_ratio) {
        max_next_ratio = temp_ratio;
      }
      if (stats_start[j].coded_error > max_next_coded) {
        max_next_coded = stats_start[j].coded_error;
      }
    }

    if (max_prev_ratio < 0.001 && max_next_ratio < 0.001) {
      // the ratios are very small, only check a small fixed threshold
      if (this_ratio < 0.02) continue;
    } else {
      // check if this frame has a larger ratio than the neighborhood
      double max_sr = stats_start[i].sr_coded_error;
      if (i < last) max_sr = AOMMAX(max_sr, stats_start[i + 1].sr_coded_error);
      double max_sr_fr_ratio =
          max_sr / AOMMAX(stats_start[i].coded_error, 0.01);

      if (max_sr_fr_ratio > 1.2) continue;
      if (this_ratio < 2 * AOMMAX(max_prev_ratio, max_next_ratio) &&
          stats_start[i].coded_error <
              2 * AOMMAX(max_prev_coded, max_next_coded)) {
        continue;
      }
    }
    return i;
  }
  return -1;
}

static void mark_flashes(const FIRSTPASS_STATS *stats, int start_idx,
                         int last_idx, int *is_flash) {
  int i;
  for (i = start_idx; i < last_idx; i++) {
    if (stats[i + 1].pcnt_second_ref > stats[i + 1].pcnt_inter &&
        stats[i + 1].pcnt_second_ref >= 0.5) {
      // this is a new flash frame
      is_flash[i] = 1;
      continue;
    }
  }
}

// Remove the region with index next_region.
// parameter merge: 0: merge with previous; 1: merge with next; 2:
// merge with both, take type from previous if possible
// After removing, next_region will be the index of the next region.
static void remove_region(int merge, REGIONS *regions, int *num_regions,
                          int *next_region) {
  int k = *next_region;
  assert(k < *num_regions);
  if (*num_regions == 1) {
    *num_regions = 0;
    return;
  }
  if (k == 0) {
    merge = 1;
  } else if (k == *num_regions - 1) {
    merge = 0;
  }
  int num_merge = (merge == 2) ? 2 : 1;
  switch (merge) {
    case 0:
      regions[k - 1].last = regions[k].last;
      *next_region = k;
      break;
    case 1:
      regions[k + 1].start = regions[k].start;
      *next_region = k + 1;
      break;
    case 2:
      regions[k - 1].last = regions[k + 1].last;
      *next_region = k;
      break;
    default: assert(0);
  }
  *num_regions -= num_merge;
  for (k = *next_region - (merge == 1); k < *num_regions; k++) {
    regions[k] = regions[k + num_merge];
  }
}

// Insert a region in the cur_region_idx. The start and last should both be in
// the current region. After insertion, the cur_region_idx will point to the
// last region that was splitted from the original region.
static void insert_region(int start, int last, REGION_TYPES type,
                          REGIONS *regions, int *num_regions,
                          int *cur_region_idx) {
  int k = *cur_region_idx;
  REGION_TYPES this_region_type = regions[k].type;
  int this_region_last = regions[k].last;
  int num_add = (start != regions[k].start) + (last != regions[k].last);
  // move the following regions further to the back
  for (int r = *num_regions - 1; r > k; r--) {
    regions[r + num_add] = regions[r];
  }
  *num_regions += num_add;
  if (start > regions[k].start) {
    regions[k].last = start - 1;
    k++;
    regions[k].start = start;
  }
  regions[k].type = type;
  if (last < this_region_last) {
    regions[k].last = last;
    k++;
    regions[k].start = last + 1;
    regions[k].last = this_region_last;
    regions[k].type = this_region_type;
  } else {
    regions[k].last = this_region_last;
  }
  *cur_region_idx = k;
}

// Estimate the noise variance of each frame from the first pass stats
static void estimate_region_noise(const FIRSTPASS_STATS *stats,
                                  const int *is_flash, REGIONS *region) {
  double C1, C2, C3, noise;
  int count = 0;
  region->avg_noise_var = -1;
  for (int i = region->start + 2; i <= region->last; i++) {
    if (is_flash[i] || is_flash[i - 1] || is_flash[i - 2]) continue;

    C1 = stats[i - 1].intra_error *
         (stats[i].intra_error - stats[i].coded_error);
    C2 = stats[i - 2].intra_error *
         (stats[i - 1].intra_error - stats[i - 1].coded_error);
    C3 = stats[i - 2].intra_error *
         (stats[i].intra_error - stats[i].sr_coded_error);
    if (C1 <= 0 || C2 <= 0 || C3 <= 0) continue;
    C1 = sqrt(C1);
    C2 = sqrt(C2);
    C3 = sqrt(C3);

    noise = stats[i - 1].intra_error - C1 * C2 / C3;
    noise = AOMMAX(noise, 0.01);
    region->avg_noise_var = (region->avg_noise_var == -1)
                                ? noise
                                : AOMMIN(noise, region->avg_noise_var);
    count++;
  }
  if (count == 0) {
    region->avg_noise_var = 0;
  }
}

// Analyze the corrrelation coefficient of each frame with its previous frame in
// a region. Also get the average of stats inside a region.
// Before calling this function, the region's noise variance is needed.
static void analyze_region(const FIRSTPASS_STATS *stats, int region_idx,
                           REGIONS *regions, double *coeff) {
  double cor_coeff;

  int i, k = region_idx;
  regions[k].avg_cor_coeff = 0;
  regions[k].avg_sr_fr_ratio = 0;
  regions[k].avg_intra_err = 0;
  regions[k].avg_coded_err = 0;

  int check_first_sr = (k != 0);

  for (i = regions[k].start; i <= regions[k].last; i++) {
    double C = sqrt(AOMMAX(stats[i - 1].intra_error *
                               (stats[i].intra_error - stats[i].coded_error),
                           0.001));
    cor_coeff =
        C / AOMMAX(stats[i - 1].intra_error - regions[k].avg_noise_var, 0.001);

    if (i > regions[k].start || check_first_sr) {
      double num_frames =
          (double)(regions[k].last - regions[k].start + check_first_sr);
      double max_coded_error =
          AOMMAX(stats[i].coded_error, stats[i - 1].coded_error);
      double this_ratio =
          stats[i].sr_coded_error / AOMMAX(max_coded_error, 0.001);
      regions[k].avg_sr_fr_ratio += this_ratio / num_frames;
    }

    regions[k].avg_intra_err +=
        stats[i].intra_error / (double)(regions[k].last - regions[k].start + 1);
    regions[k].avg_coded_err +=
        stats[i].coded_error / (double)(regions[k].last - regions[k].start + 1);

    coeff[i] =
        cor_coeff *
        sqrt(
            AOMMAX(stats[i - 1].intra_error - regions[k].avg_noise_var, 0.001) /
            AOMMAX(stats[i].intra_error - regions[k].avg_noise_var, 0.001));
    // clip correlation coefficient.
    coeff[i] = AOMMIN(AOMMAX(coeff[i], 0), 1);

    regions[k].avg_cor_coeff +=
        coeff[i] / (double)(regions[k].last - regions[k].start + 1);
  }
}

// Calculate the regions stats of every region. Uses the stable regions to
// estimate noise variance of other regions. Then call analyze_region for each.
static void get_region_stats(const FIRSTPASS_STATS *stats, const int *is_flash,
                             REGIONS *regions, double *coeff, int num_regions) {
  int k, count_stable = 0;
  // Analyze stable regions.
  for (k = 0; k < num_regions; k++) {
    if (regions[k].type == STABLE_REGION) {
      estimate_region_noise(stats, is_flash, regions + k);
      analyze_region(stats, k, regions, coeff);
      count_stable++;
    }
  }

  if (count_stable == 0) {
    // no stable region, just use the lowest noise variance estimated.
    double lowest_noise = -1;
    for (k = 0; k < num_regions; k++) {
      if (regions[k].type == SCENECUT_REGION) continue;
      estimate_region_noise(stats, is_flash, regions + k);
      if (regions[k].avg_noise_var < 0.01) continue;
      if (lowest_noise < 0 || lowest_noise > regions[k].avg_noise_var) {
        lowest_noise = regions[k].avg_noise_var;
      }
    }
    lowest_noise = AOMMAX(lowest_noise, 0);
    for (k = 0; k < num_regions; k++) {
      regions[k].avg_noise_var = lowest_noise;
      analyze_region(stats, k, regions, coeff);
    }
    return;
  }

  // Analyze other regions
  for (k = 0; k < num_regions; k++) {
    if (regions[k].type != STABLE_REGION) {
      // use the average of the nearest previous and next stable regions
      int count = 0;
      regions[k].avg_noise_var = 0;
      for (int r = k - 1; r >= 0; r--) {
        if (regions[r].type == STABLE_REGION) {
          count++;
          regions[k].avg_noise_var += regions[r].avg_noise_var;
          break;
        }
      }
      for (int r = k + 1; r < num_regions; r++) {
        if (regions[r].type == STABLE_REGION) {
          count++;
          regions[k].avg_noise_var += regions[r].avg_noise_var;
          break;
        }
      }
      if (count) {
        regions[k].avg_noise_var /= (double)count;
      }
      analyze_region(stats, k, regions, coeff);
    }
  }
}

// Find tentative stable regions
static int find_stable_regions(const FIRSTPASS_STATS *stats,
                               const double *grad_coded, const int *ignore,
                               int this_start, int this_last,
                               REGIONS *regions) {
  int i, j, k = 0;
  regions[k].start = this_start;
  for (i = this_start; i <= this_last; i++) {
    // Check mean and variance of stats in a window
    double mean_intra = 0.001, var_intra = 0.001;
    double mean_coded = 0.001, var_coded = 0.001;
    int count = 0;
    for (j = -HALF_WIN; j <= HALF_WIN; j++) {
      int idx = AOMMIN(AOMMAX(i + j, this_start), this_last);
      if (ignore[idx] || (idx > 0 && ignore[idx - 1])) continue;
      mean_intra += stats[idx].intra_error;
      var_intra += stats[idx].intra_error * stats[idx].intra_error;
      mean_coded += stats[idx].coded_error;
      var_coded += stats[idx].coded_error * stats[idx].coded_error;
      count++;
    }

    REGION_TYPES cur_type;
    if (count > 0) {
      mean_intra /= (double)count;
      var_intra /= (double)count;
      mean_coded /= (double)count;
      var_coded /= (double)count;
      int is_intra_stable = (var_intra / (mean_intra * mean_intra) < 1.03);
      int is_coded_stable = (var_coded / (mean_coded * mean_coded) < 1.04 &&
                             fabs(grad_coded[i]) / mean_coded < 0.05) ||
                            mean_coded / mean_intra < 0.05;
      int is_coded_small = mean_coded < 0.5 * mean_intra;
      cur_type = (is_intra_stable && is_coded_stable && is_coded_small)
                     ? STABLE_REGION
                     : HIGH_VAR_REGION;
    } else {
      cur_type = HIGH_VAR_REGION;
    }

    // mark a new region if type changes
    if (i == regions[k].start) {
      // first frame in the region
      regions[k].type = cur_type;
    } else if (cur_type != regions[k].type) {
      // Append a new region
      regions[k].last = i - 1;
      regions[k + 1].start = i;
      regions[k + 1].type = cur_type;
      k++;
    }
  }
  regions[k].last = this_last;
  return k + 1;
}

// Clean up regions that should be removed or merged.
static void cleanup_regions(REGIONS *regions, int *num_regions) {
  int k = 0;
  while (k < *num_regions) {
    if ((k > 0 && regions[k - 1].type == regions[k].type &&
         regions[k].type != SCENECUT_REGION) ||
        regions[k].last < regions[k].start) {
      remove_region(0, regions, num_regions, &k);
    } else {
      k++;
    }
  }
}

// Remove regions that are of type and shorter than length.
// Merge it with its neighboring regions.
static void remove_short_regions(REGIONS *regions, int *num_regions,
                                 REGION_TYPES type, int length) {
  int k = 0;
  while (k < *num_regions && (*num_regions) > 1) {
    if ((regions[k].last - regions[k].start + 1 < length &&
         regions[k].type == type)) {
      // merge current region with the previous and next regions
      remove_region(2, regions, num_regions, &k);
    } else {
      k++;
    }
  }
  cleanup_regions(regions, num_regions);
}

static void adjust_unstable_region_bounds(const FIRSTPASS_STATS *stats,
                                          const int *is_flash,
                                          const double *grad, REGIONS *regions,
                                          double *coeff, int *num_regions) {
  int i, j, k;
  // Remove regions that are too short. Likely noise.
  remove_short_regions(regions, num_regions, STABLE_REGION, HALF_WIN);
  remove_short_regions(regions, num_regions, HIGH_VAR_REGION, HALF_WIN);

  get_region_stats(stats, is_flash, regions, coeff, *num_regions);

  // Adjust region boundaries. The thresholds are empirically obtained, but
  // overall the performance is not very sensitive to small changes to them.
  for (k = 0; k < *num_regions; k++) {
    if (regions[k].type == STABLE_REGION) continue;
    if (k > 0) {
      // Adjust previous boundary.
      // First find the average intra/coded error in the previous
      // neighborhood.
      double avg_intra_err = 0, avg_coded_err = 0, avg_coeff = 0;
      int starti = AOMMAX(regions[k - 1].last - WINDOW_SIZE + 1,
                          regions[k - 1].start + 1);
      int lasti = regions[k - 1].last;
      int counti = 0;
      for (i = starti; i <= lasti; i++) {
        avg_intra_err += stats[i].intra_error;
        avg_coded_err += stats[i].coded_error;
        avg_coeff += coeff[i];
        counti++;
      }
      if (counti > 0) {
        avg_intra_err = AOMMAX(avg_intra_err / (double)counti, 0.001);
        avg_coded_err /= AOMMAX(avg_coded_err / (double)counti, 0.001);
        avg_coeff /= AOMMIN(avg_intra_err / (double)counti, 0.99999);
        int count_coded = 0, count_grad = 0;
        for (j = lasti + 1; j <= regions[k].last; j++) {
          int intra_close =
              fabs(stats[j].intra_error - avg_intra_err) / avg_intra_err < 0.1;
          int coded_close =
              fabs(stats[j].coded_error - avg_coded_err) / avg_coded_err < 0.15;
          int grad_small = fabs(grad[j]) / avg_coded_err < 0.05;
          int coded_small = stats[j].coded_error / avg_intra_err < 0.03;
          int coeff_close =
              (1 - coeff[j]) / (1 - avg_coeff) < 1.5 || coeff[j] > 0.995;
          if (!coeff_close || (!coded_close && !coded_small)) count_coded--;
          if (!grad_small && !coded_small) count_grad--;

          if (intra_close && count_coded >= 0 && count_grad >= 0) {
            // this frame probably belongs to the previous stable region
            regions[k - 1].last = j;
            regions[k].start = j + 1;
          } else {
            break;
          }
        }
      }
    }  // if k > 0
    if (k < *num_regions - 1) {
      // Adjust next boundary.
      // First find the average intra/coded error in the next neighborhood.
      double avg_intra_err = 0, avg_coded_err = 0, avg_coeff = 0;
      int starti = regions[k + 1].start;
      int lasti = AOMMIN(regions[k + 1].last - 1,
                         regions[k + 1].start + WINDOW_SIZE - 1);
      int counti = 0;
      for (i = starti; i <= lasti; i++) {
        avg_intra_err += stats[i].intra_error;
        avg_coded_err += stats[i + 1].coded_error;
        avg_coeff += coeff[i];
        counti++;
      }
      if (counti > 0) {
        avg_intra_err = AOMMAX(avg_intra_err / (double)counti, 0.001);
        avg_coded_err /= AOMMAX(avg_coded_err / (double)counti, 0.001);
        avg_coeff /= AOMMIN(avg_intra_err / (double)counti, 0.99999);
        // At the boundary, coded error is large, but still the frame is stable
        int count_coded = 1, count_grad = 1;
        for (j = starti - 1; j >= regions[k].start; j--) {
          int intra_close =
              fabs(stats[j].intra_error - avg_intra_err) / avg_intra_err < 0.1;
          int coded_close =
              fabs(stats[j + 1].coded_error - avg_coded_err) / avg_coded_err <
              0.15;
          int grad_small = fabs(grad[j + 1]) / avg_coded_err < 0.05;
          int coded_small = stats[j + 1].coded_error / avg_intra_err < 0.03;
          int coeff_close =
              (1 - coeff[j + 1]) / (1 - avg_coeff) < 1.5 || coeff[j] > 0.995;
          if (!coeff_close || (!coded_close && !coded_small)) count_coded--;
          if (!grad_small && !coded_small) count_grad--;
          if (intra_close && count_coded >= 0 && count_grad >= 0) {
            // this frame probably belongs to the next stable region
            regions[k + 1].start = j;
            regions[k].last = j - 1;
          } else {
            break;
          }
        }
      }
    }  // if k < *num_regions - 1
  }    // end of loop over all regions

  cleanup_regions(regions, num_regions);
  remove_short_regions(regions, num_regions, HIGH_VAR_REGION, HALF_WIN);
  get_region_stats(stats, is_flash, regions, coeff, *num_regions);

  // If a stable regions has higher error than neighboring high var regions,
  // or if the stable region has a lower average correlation,
  // then it should be merged with them
  k = 0;
  while (k < *num_regions && (*num_regions) > 1) {
    if (regions[k].type == STABLE_REGION &&
        ((k > 0 &&  // previous regions
          (regions[k].avg_coded_err > regions[k - 1].avg_coded_err ||
           regions[k].avg_cor_coeff < regions[k - 1].avg_cor_coeff)) &&
         (k < *num_regions - 1 &&  // next region
          (regions[k].avg_coded_err > regions[k + 1].avg_coded_err ||
           regions[k].avg_cor_coeff < regions[k + 1].avg_cor_coeff)))) {
      // merge current region with the previous and next regions
      remove_region(2, regions, num_regions, &k);
      analyze_region(stats, k - 1, regions, coeff);
    } else if (regions[k].type == HIGH_VAR_REGION &&
               ((k > 0 &&  // previous regions
                 (regions[k].avg_coded_err < regions[k - 1].avg_coded_err ||
                  regions[k].avg_cor_coeff > regions[k - 1].avg_cor_coeff)) &&
                (k < *num_regions - 1 &&  // next region
                 (regions[k].avg_coded_err < regions[k + 1].avg_coded_err ||
                  regions[k].avg_cor_coeff > regions[k + 1].avg_cor_coeff)))) {
      // merge current region with the previous and next regions
      remove_region(2, regions, num_regions, &k);
      analyze_region(stats, k - 1, regions, coeff);
    } else {
      k++;
    }
  }

  remove_short_regions(regions, num_regions, STABLE_REGION, WINDOW_SIZE);
  remove_short_regions(regions, num_regions, HIGH_VAR_REGION, HALF_WIN);
}

// Identify blending regions.
static void find_blending_regions(const FIRSTPASS_STATS *stats,
                                  const int *is_flash, REGIONS *regions,
                                  int *num_regions, double *coeff) {
  int i, k = 0;
  // Blending regions will have large content change, therefore will have a
  // large consistent change in intra error.
  int count_stable = 0;
  while (k < *num_regions) {
    if (regions[k].type == STABLE_REGION) {
      k++;
      count_stable++;
      continue;
    }
    int dir = 0;
    int start = 0, last;
    for (i = regions[k].start; i <= regions[k].last; i++) {
      // First mark the regions that has consistent large change of intra error.
      if (is_flash[i] || (i > 0 && is_flash[i - 1])) continue;
      double grad = stats[i].intra_error - stats[i - 1].intra_error;
      int large_change = fabs(grad) / AOMMAX(stats[i].intra_error, 0.01) > 0.05;
      int this_dir = 0;
      if (large_change) {
        this_dir = (grad > 0) ? 1 : -1;
      }
      // the current trend continues
      if (dir == this_dir) continue;
      if (dir != 0) {
        // Mark the end of a new large change group and add it
        last = i - 1;
        insert_region(start, last, BLENDING_REGION, regions, num_regions, &k);
      }
      dir = this_dir;
      start = i;
    }
    if (dir != 0) {
      last = regions[k].last;
      insert_region(start, last, BLENDING_REGION, regions, num_regions, &k);
    }
    k++;
  }

  // If the blending region has very low correlation, mark it as high variance
  // since we probably cannot benefit from it anyways.
  get_region_stats(stats, is_flash, regions, coeff, *num_regions);
  for (k = 0; k < *num_regions; k++) {
    if (regions[k].type != BLENDING_REGION) continue;
    if (regions[k].last == regions[k].start || regions[k].avg_cor_coeff < 0.6 ||
        count_stable == 0)
      regions[k].type = HIGH_VAR_REGION;
  }
  get_region_stats(stats, is_flash, regions, coeff, *num_regions);

  // It is possible for blending to result in a "dip" in intra error (first
  // decrease then increase). Therefore we need to find the dip and combine the
  // two regions.
  k = 1;
  while (k < *num_regions) {
    if (k < *num_regions - 1 && regions[k].type == HIGH_VAR_REGION) {
      // Check if this short high variance regions is actually in the middle of
      // a blending region.
      if (regions[k - 1].type == BLENDING_REGION &&
          regions[k + 1].type == BLENDING_REGION &&
          regions[k].last - regions[k].start < 3) {
        int prev_dir = (stats[regions[k - 1].last].intra_error -
                        stats[regions[k - 1].last - 1].intra_error) > 0
                           ? 1
                           : -1;
        int next_dir = (stats[regions[k + 1].last].intra_error -
                        stats[regions[k + 1].last - 1].intra_error) > 0
                           ? 1
                           : -1;
        if (prev_dir < 0 && next_dir > 0) {
          // This is possibly a mid region of blending. Check the ratios
          double ratio_thres = AOMMIN(regions[k - 1].avg_sr_fr_ratio,
                                      regions[k + 1].avg_sr_fr_ratio) *
                               0.95;
          if (regions[k].avg_sr_fr_ratio > ratio_thres) {
            regions[k].type = BLENDING_REGION;
            remove_region(2, regions, num_regions, &k);
            analyze_region(stats, k - 1, regions, coeff);
            continue;
          }
        }
      }
    }
    // Check if we have a pair of consecutive blending regions.
    if (regions[k - 1].type == BLENDING_REGION &&
        regions[k].type == BLENDING_REGION) {
      int prev_dir = (stats[regions[k - 1].last].intra_error -
                      stats[regions[k - 1].last - 1].intra_error) > 0
                         ? 1
                         : -1;
      int next_dir = (stats[regions[k].last].intra_error -
                      stats[regions[k].last - 1].intra_error) > 0
                         ? 1
                         : -1;

      // if both are too short, no need to check
      int total_length = regions[k].last - regions[k - 1].start + 1;
      if (total_length < 4) {
        regions[k - 1].type = HIGH_VAR_REGION;
        k++;
        continue;
      }

      int to_merge = 0;
      if (prev_dir < 0 && next_dir > 0) {
        // In this case we check the last frame in the previous region.
        double prev_length =
            (double)(regions[k - 1].last - regions[k - 1].start + 1);
        double last_ratio, ratio_thres;
        if (prev_length < 2.01) {
          // if the previous region is very short
          double max_coded_error =
              AOMMAX(stats[regions[k - 1].last].coded_error,
                     stats[regions[k - 1].last - 1].coded_error);
          last_ratio = stats[regions[k - 1].last].sr_coded_error /
                       AOMMAX(max_coded_error, 0.001);
          ratio_thres = regions[k].avg_sr_fr_ratio * 0.95;
        } else {
          double max_coded_error =
              AOMMAX(stats[regions[k - 1].last].coded_error,
                     stats[regions[k - 1].last - 1].coded_error);
          last_ratio = stats[regions[k - 1].last].sr_coded_error /
                       AOMMAX(max_coded_error, 0.001);
          double prev_ratio =
              (regions[k - 1].avg_sr_fr_ratio * prev_length - last_ratio) /
              (prev_length - 1.0);
          ratio_thres = AOMMIN(prev_ratio, regions[k].avg_sr_fr_ratio) * 0.95;
        }
        if (last_ratio > ratio_thres) {
          to_merge = 1;
        }
      }

      if (to_merge) {
        remove_region(0, regions, num_regions, &k);
        analyze_region(stats, k - 1, regions, coeff);
        continue;
      } else {
        // These are possibly two separate blending regions. Mark the boundary
        // frame as HIGH_VAR_REGION to separate the two.
        int prev_k = k - 1;
        insert_region(regions[prev_k].last, regions[prev_k].last,
                      HIGH_VAR_REGION, regions, num_regions, &prev_k);
        analyze_region(stats, prev_k, regions, coeff);
        k = prev_k + 1;
        analyze_region(stats, k, regions, coeff);
      }
    }
    k++;
  }
  cleanup_regions(regions, num_regions);
}

// Clean up decision for blendings. Remove blending regions that are too short.
// Also if a very short high var region is between a blending and a stable
// region, just merge it with one of them.
static void cleanup_blendings(REGIONS *regions, int *num_regions) {
  int k = 0;
  while (k<*num_regions && * num_regions> 1) {
    int is_short_blending = regions[k].type == BLENDING_REGION &&
                            regions[k].last - regions[k].start + 1 < 5;
    int is_short_hv = regions[k].type == HIGH_VAR_REGION &&
                      regions[k].last - regions[k].start + 1 < 5;
    int has_stable_neighbor =
        ((k > 0 && regions[k - 1].type == STABLE_REGION) ||
         (k < *num_regions - 1 && regions[k + 1].type == STABLE_REGION));
    int has_blend_neighbor =
        ((k > 0 && regions[k - 1].type == BLENDING_REGION) ||
         (k < *num_regions - 1 && regions[k + 1].type == BLENDING_REGION));
    int total_neighbors = (k > 0) + (k < *num_regions - 1);

    if (is_short_blending ||
        (is_short_hv &&
         has_stable_neighbor + has_blend_neighbor >= total_neighbors)) {
      // Remove this region.Try to determine whether to combine it with the
      // previous or next region.
      int merge;
      double prev_diff =
          (k > 0)
              ? fabs(regions[k].avg_cor_coeff - regions[k - 1].avg_cor_coeff)
              : 1;
      double next_diff =
          (k < *num_regions - 1)
              ? fabs(regions[k].avg_cor_coeff - regions[k + 1].avg_cor_coeff)
              : 1;
      // merge == 0 means to merge with previous, 1 means to merge with next
      merge = prev_diff > next_diff;
      remove_region(merge, regions, num_regions, &k);
    } else {
      k++;
    }
  }
  cleanup_regions(regions, num_regions);
}

// Identify stable and unstable regions from first pass stats.
// Stats_start points to the first frame to analyze.
// Offset is the offset from the current frame to the frame stats_start is
// pointing to.
static void identify_regions(const FIRSTPASS_STATS *const stats_start,
                             int total_frames, int offset, REGIONS *regions,
                             int *total_regions, double *cor_coeff) {
  int k;
  if (total_frames <= 1) return;

  double *coeff = cor_coeff + offset;

  // store the initial decisions
  REGIONS temp_regions[MAX_FIRSTPASS_ANALYSIS_FRAMES];
  av1_zero_array(temp_regions, MAX_FIRSTPASS_ANALYSIS_FRAMES);
  int is_flash[MAX_FIRSTPASS_ANALYSIS_FRAMES] = { 0 };
  // buffers for filtered stats
  double filt_intra_err[MAX_FIRSTPASS_ANALYSIS_FRAMES] = { 0 };
  double filt_coded_err[MAX_FIRSTPASS_ANALYSIS_FRAMES] = { 0 };
  double grad_coded[MAX_FIRSTPASS_ANALYSIS_FRAMES] = { 0 };

  int cur_region = 0, this_start = 0, this_last;

  // find possible flash frames
  mark_flashes(stats_start, 0, total_frames - 1, is_flash);

  // first get the obvious scenecuts
  int next_scenecut = -1;

  do {
    next_scenecut =
        find_next_scenecut(stats_start, this_start, total_frames - 1, is_flash);
    this_last = (next_scenecut >= 0) ? (next_scenecut - 1) : total_frames - 1;
    // low-pass filter the needed stats
    smooth_filter_stats(stats_start, is_flash, this_start, this_last,
                        filt_intra_err, filt_coded_err);
    get_gradient(filt_coded_err, this_start, this_last, grad_coded);

    // find tentative stable regions and unstable regions
    int num_regions = find_stable_regions(stats_start, grad_coded, is_flash,
                                          this_start, this_last, temp_regions);
    adjust_unstable_region_bounds(stats_start, is_flash, grad_coded,
                                  temp_regions, coeff, &num_regions);

    get_region_stats(stats_start, is_flash, temp_regions, coeff, num_regions);

    // Try to identify blending regions in the unstable regions
    find_blending_regions(stats_start, is_flash, temp_regions, &num_regions,
                          coeff);
    cleanup_blendings(temp_regions, &num_regions);

    // The flash points should all be considered high variance points
    k = 0;
    while (k < num_regions) {
      if (temp_regions[k].type != STABLE_REGION) {
        k++;
        continue;
      }
      int start = temp_regions[k].start;
      int last = temp_regions[k].last;
      for (int i = start; i <= last; i++) {
        if (is_flash[i]) {
          insert_region(i, i, HIGH_VAR_REGION, temp_regions, &num_regions, &k);
        }
      }
      k++;
    }
    cleanup_regions(temp_regions, &num_regions);

    // copy the regions in the scenecut group
    for (k = 0; k < num_regions; k++) {
      regions[k + cur_region] = temp_regions[k];
    }
    cur_region += num_regions;

    // add the scenecut region
    if (next_scenecut > -1) {
      // add the scenecut region, and find the next scenecut
      regions[cur_region].type = SCENECUT_REGION;
      regions[cur_region].start = next_scenecut;
      regions[cur_region].last = next_scenecut;
      cur_region++;
      this_start = next_scenecut + 1;
    }
  } while (next_scenecut >= 0);

  *total_regions = cur_region;
  get_region_stats(stats_start, is_flash, regions, coeff, *total_regions);

  for (k = 0; k < *total_regions; k++) {
    // If scenecuts are very minor, mark them as high variance.
    if (regions[k].type != SCENECUT_REGION || regions[k].avg_cor_coeff < 0.8) {
      continue;
    }
    regions[k].type = HIGH_VAR_REGION;
  }
  cleanup_regions(regions, total_regions);
  get_region_stats(stats_start, is_flash, regions, coeff, *total_regions);

  for (k = 0; k < *total_regions; k++) {
    regions[k].start += offset;
    regions[k].last += offset;
  }
}

static int find_regions_index(const REGIONS *regions, int num_regions,
                              int frame_idx) {
  for (int k = 0; k < num_regions; k++) {
    if (regions[k].start <= frame_idx && regions[k].last >= frame_idx) {
      return k;
    }
  }
  return -1;
}

/*!\brief Determine the length of future GF groups.
 *
 * \ingroup gf_group_algo
 * This function decides the gf group length of future frames in batch
 *
 * \param[in]    cpi              Top-level encoder structure
 * \param[in]    max_gop_length   Maximum length of the GF group
 * \param[in]    max_intervals    Maximum number of intervals to decide
 *
 * \return Nothing is returned. Instead, cpi->rc.gf_intervals is
 * changed to store the decided GF group lengths.
 */
static void calculate_gf_length(AV1_COMP *cpi, int max_gop_length,
                                int max_intervals) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  FIRSTPASS_STATS next_frame;
  const FIRSTPASS_STATS *const start_pos = twopass->stats_in;
  FRAME_INFO *frame_info = &cpi->frame_info;
  int i;

  int flash_detected;

  aom_clear_system_state();
  av1_zero(next_frame);

  if (has_no_stats_stage(cpi)) {
    for (i = 0; i < MAX_NUM_GF_INTERVALS; i++) {
      rc->gf_intervals[i] = AOMMIN(rc->max_gf_interval, max_gop_length);
    }
    rc->cur_gf_index = 0;
    rc->intervals_till_gf_calculate_due = MAX_NUM_GF_INTERVALS;
    return;
  }

  // TODO(urvang): Try logic to vary min and max interval based on q.
  const int active_min_gf_interval = rc->min_gf_interval;
  const int active_max_gf_interval =
      AOMMIN(rc->max_gf_interval, max_gop_length);
  const int min_shrink_int = AOMMAX(MIN_SHRINK_LEN, active_min_gf_interval);

  i = (rc->frames_since_key == 0);
  max_intervals = cpi->lap_enabled ? 1 : max_intervals;
  int count_cuts = 1;
  // If cpi->gf_state.arf_gf_boost_lst is 0, we are starting with a KF or GF.
  int cur_start = -1 + !cpi->gf_state.arf_gf_boost_lst, cur_last;
  int cut_pos[MAX_NUM_GF_INTERVALS + 1] = { -1 };
  int cut_here;
  GF_GROUP_STATS gf_stats;
  init_gf_stats(&gf_stats);
  while (count_cuts < max_intervals + 1) {
    // reaches next key frame, break here
    if (i >= rc->frames_to_key + rc->next_is_fwd_key) {
      cut_here = 2;
    } else if (i - cur_start >= rc->static_scene_max_gf_interval) {
      // reached maximum len, but nothing special yet (almost static)
      // let's look at the next interval
      cut_here = 1;
    } else if (EOF == input_stats(twopass, &next_frame)) {
      // reaches last frame, break
      cut_here = 2;
    } else {
      // Test for the case where there is a brief flash but the prediction
      // quality back to an earlier frame is then restored.
      flash_detected = detect_flash(twopass, 0);
      // TODO(bohanli): remove redundant accumulations here, or unify
      // this and the ones in define_gf_group
      accumulate_next_frame_stats(&next_frame, frame_info, flash_detected,
                                  rc->frames_since_key, i, &gf_stats);

      cut_here = detect_gf_cut(cpi, i, cur_start, flash_detected,
                               active_max_gf_interval, active_min_gf_interval,
                               &gf_stats);
    }
    if (cut_here) {
      cur_last = i - 1;  // the current last frame in the gf group
      int ori_last = cur_last;
      // The region frame idx does not start from the same frame as cur_start
      // and cur_last. Need to offset them.
      int offset = rc->frames_since_key - rc->regions_offset;
      REGIONS *regions = rc->regions;
      int num_regions = rc->num_regions;
      if (cpi->oxcf.kf_cfg.fwd_kf_enabled && rc->next_is_fwd_key) {
        const int frames_left = rc->frames_to_key - i;
        const int min_int = AOMMIN(MIN_FWD_KF_INTERVAL, active_min_gf_interval);
        if (frames_left < min_int && frames_left > 0) {
          cur_last = rc->frames_to_key - min_int - 1;
        }
      }

      int scenecut_idx = -1;
      // only try shrinking if interval smaller than active_max_gf_interval
      if (cur_last - cur_start <= active_max_gf_interval &&
          cur_last > cur_start) {
        // find the region indices of where the first and last frame belong.
        int k_start =
            find_regions_index(regions, num_regions, cur_start + offset);
        int k_last =
            find_regions_index(regions, num_regions, cur_last + offset);
        if (cur_start + offset == 0) k_start = 0;

        // See if we have a scenecut in between
        for (int r = k_start + 1; r <= k_last; r++) {
          if (regions[r].type == SCENECUT_REGION &&
              regions[r].last - offset - cur_start > active_min_gf_interval) {
            scenecut_idx = r;
            break;
          }
        }

        // if the found scenecut is very close to the end, ignore it.
        if (regions[num_regions - 1].last - regions[scenecut_idx].last < 4) {
          scenecut_idx = -1;
        }

        if (scenecut_idx != -1) {
          // If we have a scenecut, then stop at it.
          // TODO(bohanli): add logic here to stop before the scenecut and for
          // the next gop start from the scenecut with GF
          int is_minor_sc = (regions[scenecut_idx].avg_cor_coeff > 0.6);
          cur_last = regions[scenecut_idx].last - offset - !is_minor_sc;
        } else {
          int is_last_analysed = (k_last == num_regions - 1) &&
                                 (cur_last + offset == regions[k_last].last);
          int not_enough_regions =
              k_last - k_start <=
              1 + (regions[k_start].type == SCENECUT_REGION);
          // if we are very close to the end, then do not shrink since it may
          // introduce intervals that are too short
          if (!(is_last_analysed && not_enough_regions)) {
            int found = 0;
            // first try to end at a stable area
            for (int j = cur_last; j >= cur_start + min_shrink_int; j--) {
              if (regions[find_regions_index(regions, num_regions, j + offset)]
                      .type == STABLE_REGION) {
                cur_last = j;
                found = 1;
                break;
              }
            }
            if (!found) {
              // Could not find stable point,
              // try to find an OK point (high correlation, not blending)
              for (int j = cur_last; j >= cur_start + min_shrink_int; j--) {
                REGIONS *cur_region =
                    regions +
                    find_regions_index(regions, num_regions, j + offset);
                double avg_coeff = cur_region->avg_cor_coeff;
                if (rc->cor_coeff[j + offset] > avg_coeff &&
                    cur_region->type != BLENDING_REGION) {
                  cur_last = j;
                  found = 1;
                  break;
                }
              }
            }
            if (!found) {
              // Could not find a better point,
              // try not to cut in blending areas
              for (int j = cur_last; j >= cur_start + min_shrink_int; j--) {
                REGIONS *cur_region =
                    regions +
                    find_regions_index(regions, num_regions, j + offset);
                if (cur_region->type != BLENDING_REGION) {
                  cur_last = j;
                  break;
                }
              }
            }
            // if cannot find anything, just cut at the original place.
          }
        }
      }
      cut_pos[count_cuts] = cur_last;
      count_cuts++;

      // reset pointers to the shrinked location
      twopass->stats_in = start_pos + cur_last;
      cur_start = cur_last;
      if (regions[find_regions_index(regions, num_regions,
                                     cur_start + 1 + offset)]
              .type == SCENECUT_REGION) {
        cur_start++;
      }
      i = cur_last;

      if (cut_here > 1 && cur_last == ori_last) break;

      // reset accumulators
      init_gf_stats(&gf_stats);
    }
    ++i;
  }

  // save intervals
  rc->intervals_till_gf_calculate_due = count_cuts - 1;
  for (int n = 1; n < count_cuts; n++) {
    rc->gf_intervals[n - 1] = cut_pos[n] - cut_pos[n - 1];
  }
  rc->cur_gf_index = 0;
  twopass->stats_in = start_pos;
}

static void correct_frames_to_key(AV1_COMP *cpi) {
  int lookahead_size =
      (int)av1_lookahead_depth(cpi->lookahead, cpi->compressor_stage);
  if (lookahead_size <
      av1_lookahead_pop_sz(cpi->lookahead, cpi->compressor_stage)) {
    assert(IMPLIES(cpi->oxcf.pass != 0 && cpi->frames_left > 0,
                   lookahead_size == cpi->frames_left));
    cpi->rc.frames_to_key = AOMMIN(cpi->rc.frames_to_key, lookahead_size);
  } else if (cpi->frames_left > 0) {
    // Correct frames to key based on limit
    cpi->rc.frames_to_key = AOMMIN(cpi->rc.frames_to_key, cpi->frames_left);
  }
}

/*!\brief Define a GF group in one pass mode when no look ahead stats are
 * available.
 *
 * \ingroup gf_group_algo
 * This function defines the structure of a GF group, along with various
 * parameters regarding bit-allocation and quality setup in the special
 * case of one pass encoding where no lookahead stats are avialable.
 *
 * \param[in]    cpi             Top-level encoder structure
 *
 * \return Nothing is returned. Instead, cpi->gf_group is changed.
 */
static void define_gf_group_pass0(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  GF_GROUP *const gf_group = &cpi->gf_group;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const GFConfig *const gf_cfg = &oxcf->gf_cfg;
  int target;

  if (oxcf->q_cfg.aq_mode == CYCLIC_REFRESH_AQ) {
    av1_cyclic_refresh_set_golden_update(cpi);
  } else {
    rc->baseline_gf_interval = rc->gf_intervals[rc->cur_gf_index];
    rc->intervals_till_gf_calculate_due--;
    rc->cur_gf_index++;
  }

  // correct frames_to_key when lookahead queue is flushing
  correct_frames_to_key(cpi);

  if (rc->baseline_gf_interval > rc->frames_to_key)
    rc->baseline_gf_interval = rc->frames_to_key;

  rc->gfu_boost = DEFAULT_GF_BOOST;
  rc->constrained_gf_group =
      (rc->baseline_gf_interval >= rc->frames_to_key) ? 1 : 0;

  gf_group->max_layer_depth_allowed = oxcf->gf_cfg.gf_max_pyr_height;

  // Rare case when the look-ahead is less than the target GOP length, can't
  // generate ARF frame.
  if (rc->baseline_gf_interval > gf_cfg->lag_in_frames ||
      !is_altref_enabled(gf_cfg->lag_in_frames, gf_cfg->enable_auto_arf) ||
      rc->baseline_gf_interval < rc->min_gf_interval)
    gf_group->max_layer_depth_allowed = 0;

  // Set up the structure of this Group-Of-Pictures (same as GF_GROUP)
  av1_gop_setup_structure(cpi);

  // Allocate bits to each of the frames in the GF group.
  // TODO(sarahparker) Extend this to work with pyramid structure.
  for (int cur_index = 0; cur_index < gf_group->size; ++cur_index) {
    const FRAME_UPDATE_TYPE cur_update_type = gf_group->update_type[cur_index];
    if (oxcf->rc_cfg.mode == AOM_CBR) {
      if (cur_update_type == KF_UPDATE) {
        target = av1_calc_iframe_target_size_one_pass_cbr(cpi);
      } else {
        target = av1_calc_pframe_target_size_one_pass_cbr(cpi, cur_update_type);
      }
    } else {
      if (cur_update_type == KF_UPDATE) {
        target = av1_calc_iframe_target_size_one_pass_vbr(cpi);
      } else {
        target = av1_calc_pframe_target_size_one_pass_vbr(cpi, cur_update_type);
      }
    }
    gf_group->bit_allocation[cur_index] = target;
  }
}

static INLINE void set_baseline_gf_interval(AV1_COMP *cpi, int arf_position,
                                            int active_max_gf_interval,
                                            int use_alt_ref,
                                            int is_final_pass) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  // Set the interval until the next gf.
  // If forward keyframes are enabled, ensure the final gf group obeys the
  // MIN_FWD_KF_INTERVAL.
  const int is_last_kf =
      (twopass->stats_in - arf_position + rc->frames_to_key) >=
      twopass->stats_buf_ctx->stats_in_end;

  if (cpi->oxcf.kf_cfg.fwd_kf_enabled && use_alt_ref && !is_last_kf &&
      cpi->rc.next_is_fwd_key) {
    if (arf_position == rc->frames_to_key + 1) {
      rc->baseline_gf_interval = arf_position;
      // if the last gf group will be smaller than MIN_FWD_KF_INTERVAL
    } else if (rc->frames_to_key + 1 - arf_position <
               AOMMAX(MIN_FWD_KF_INTERVAL, rc->min_gf_interval)) {
      // if possible, merge the last two gf groups
      if (rc->frames_to_key + 1 <= active_max_gf_interval) {
        rc->baseline_gf_interval = rc->frames_to_key + 1;
        if (is_final_pass) rc->intervals_till_gf_calculate_due = 0;
        // if merging the last two gf groups creates a group that is too long,
        // split them and force the last gf group to be the MIN_FWD_KF_INTERVAL
      } else {
        rc->baseline_gf_interval = rc->frames_to_key + 1 - MIN_FWD_KF_INTERVAL;
        if (is_final_pass) rc->intervals_till_gf_calculate_due = 0;
      }
    } else {
      rc->baseline_gf_interval = arf_position;
    }
  } else {
    rc->baseline_gf_interval = arf_position;
  }
}

// initialize GF_GROUP_STATS
static void init_gf_stats(GF_GROUP_STATS *gf_stats) {
  gf_stats->gf_group_err = 0.0;
  gf_stats->gf_group_raw_error = 0.0;
  gf_stats->gf_group_skip_pct = 0.0;
  gf_stats->gf_group_inactive_zone_rows = 0.0;

  gf_stats->mv_ratio_accumulator = 0.0;
  gf_stats->decay_accumulator = 1.0;
  gf_stats->zero_motion_accumulator = 1.0;
  gf_stats->loop_decay_rate = 1.0;
  gf_stats->last_loop_decay_rate = 1.0;
  gf_stats->this_frame_mv_in_out = 0.0;
  gf_stats->mv_in_out_accumulator = 0.0;
  gf_stats->abs_mv_in_out_accumulator = 0.0;

  gf_stats->avg_sr_coded_error = 0.0;
  gf_stats->avg_tr_coded_error = 0.0;
  gf_stats->avg_pcnt_second_ref = 0.0;
  gf_stats->avg_pcnt_third_ref = 0.0;
  gf_stats->avg_pcnt_third_ref_nolast = 0.0;
  gf_stats->avg_new_mv_count = 0.0;
  gf_stats->avg_wavelet_energy = 0.0;
  gf_stats->avg_raw_err_stdev = 0.0;
  gf_stats->non_zero_stdev_count = 0;
}

// Analyse and define a gf/arf group.
#define MAX_GF_BOOST 5400
/*!\brief Define a GF group.
 *
 * \ingroup gf_group_algo
 * This function defines the structure of a GF group, along with various
 * parameters regarding bit-allocation and quality setup.
 *
 * \param[in]    cpi             Top-level encoder structure
 * \param[in]    this_frame      First pass statistics structure
 * \param[in]    frame_params    Structure with frame parameters
 * \param[in]    max_gop_length  Maximum length of the GF group
 * \param[in]    is_final_pass   Whether this is the final pass for the
 *                               GF group, or a trial (non-zero)
 *
 * \return Nothing is returned. Instead, cpi->gf_group is changed.
 */
static void define_gf_group(AV1_COMP *cpi, FIRSTPASS_STATS *this_frame,
                            EncodeFrameParams *frame_params, int max_gop_length,
                            int is_final_pass) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->twopass;
  FIRSTPASS_STATS next_frame;
  const FIRSTPASS_STATS *const start_pos = twopass->stats_in;
  GF_GROUP *gf_group = &cpi->gf_group;
  FRAME_INFO *frame_info = &cpi->frame_info;
  const GFConfig *const gf_cfg = &oxcf->gf_cfg;
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;
  int i;
  int flash_detected;
  int64_t gf_group_bits;
  const int is_intra_only = rc->frames_since_key == 0;

  cpi->internal_altref_allowed = (gf_cfg->gf_max_pyr_height > 1);

  // Reset the GF group data structures unless this is a key
  // frame in which case it will already have been done.
  if (!is_intra_only) {
    av1_zero(cpi->gf_group);
  }

  aom_clear_system_state();
  av1_zero(next_frame);

  if (has_no_stats_stage(cpi)) {
    define_gf_group_pass0(cpi);
    return;
  }

  // correct frames_to_key when lookahead queue is emptying
  if (cpi->lap_enabled) {
    correct_frames_to_key(cpi);
  }

  GF_GROUP_STATS gf_stats;
  init_gf_stats(&gf_stats);
  GF_FRAME_STATS first_frame_stats, last_frame_stats;

  const int can_disable_arf = !gf_cfg->gf_min_pyr_height;

  // Load stats for the current frame.
  double mod_frame_err =
      calculate_modified_err(frame_info, twopass, oxcf, this_frame);

  // Note the error of the frame at the start of the group. This will be
  // the GF frame error if we code a normal gf.
  first_frame_stats.frame_err = mod_frame_err;
  first_frame_stats.frame_coded_error = this_frame->coded_error;
  first_frame_stats.frame_sr_coded_error = this_frame->sr_coded_error;
  first_frame_stats.frame_tr_coded_error = this_frame->tr_coded_error;

  // If this is a key frame or the overlay from a previous arf then
  // the error score / cost of this frame has already been accounted for.

  // TODO(urvang): Try logic to vary min and max interval based on q.
  const int active_min_gf_interval = rc->min_gf_interval;
  const int active_max_gf_interval =
      AOMMIN(rc->max_gf_interval, max_gop_length);

  i = is_intra_only;
  // get the determined gf group length from rc->gf_intervals
  while (i < rc->gf_intervals[rc->cur_gf_index]) {
    // read in the next frame
    if (EOF == input_stats(twopass, &next_frame)) break;
    // Accumulate error score of frames in this gf group.
    mod_frame_err =
        calculate_modified_err(frame_info, twopass, oxcf, &next_frame);
    // accumulate stats for this frame
    accumulate_this_frame_stats(&next_frame, mod_frame_err, &gf_stats);

    if (i == 0) {
      first_frame_stats.frame_err = mod_frame_err;
      first_frame_stats.frame_coded_error = next_frame.coded_error;
      first_frame_stats.frame_sr_coded_error = next_frame.sr_coded_error;
      first_frame_stats.frame_tr_coded_error = next_frame.tr_coded_error;
    }

    ++i;
  }

  reset_fpf_position(twopass, start_pos);

  i = is_intra_only;
  input_stats(twopass, &next_frame);
  while (i < rc->gf_intervals[rc->cur_gf_index]) {
    // read in the next frame
    if (EOF == input_stats(twopass, &next_frame)) break;

    // Test for the case where there is a brief flash but the prediction
    // quality back to an earlier frame is then restored.
    flash_detected = detect_flash(twopass, 0);

    // accumulate stats for next frame
    accumulate_next_frame_stats(&next_frame, frame_info, flash_detected,
                                rc->frames_since_key, i, &gf_stats);

    ++i;
  }

  i = rc->gf_intervals[rc->cur_gf_index];

  // save the errs for the last frame
  last_frame_stats.frame_coded_error = next_frame.coded_error;
  last_frame_stats.frame_sr_coded_error = next_frame.sr_coded_error;
  last_frame_stats.frame_tr_coded_error = next_frame.tr_coded_error;

  if (is_final_pass) {
    rc->intervals_till_gf_calculate_due--;
    rc->cur_gf_index++;
  }

  // Was the group length constrained by the requirement for a new KF?
  rc->constrained_gf_group = (i >= rc->frames_to_key) ? 1 : 0;

  const int num_mbs = (oxcf->resize_cfg.resize_mode != RESIZE_NONE)
                          ? cpi->initial_mbs
                          : cm->mi_params.MBs;
  assert(num_mbs > 0);

  average_gf_stats(i, &next_frame, &gf_stats);

  // Disable internal ARFs for "still" gf groups.
  //   zero_motion_accumulator: minimum percentage of (0,0) motion;
  //   avg_sr_coded_error:      average of the SSE per pixel of each frame;
  //   avg_raw_err_stdev:       average of the standard deviation of (0,0)
  //                            motion error per block of each frame.
  const int can_disable_internal_arfs = gf_cfg->gf_min_pyr_height <= 1;
  if (can_disable_internal_arfs &&
      gf_stats.zero_motion_accumulator > MIN_ZERO_MOTION &&
      gf_stats.avg_sr_coded_error / num_mbs < MAX_SR_CODED_ERROR &&
      gf_stats.avg_raw_err_stdev < MAX_RAW_ERR_VAR) {
    cpi->internal_altref_allowed = 0;
  }

  int use_alt_ref;
  if (can_disable_arf) {
    use_alt_ref =
        !is_almost_static(gf_stats.zero_motion_accumulator,
                          twopass->kf_zeromotion_pct, cpi->lap_enabled) &&
        rc->use_arf_in_this_kf_group && (i < gf_cfg->lag_in_frames) &&
        (i >= MIN_GF_INTERVAL);

    // TODO(urvang): Improve and use model for VBR, CQ etc as well.
    if (use_alt_ref && rc_cfg->mode == AOM_Q && rc_cfg->cq_level <= 200) {
      aom_clear_system_state();
      float features[21];
      get_features_from_gf_stats(
          &gf_stats, &first_frame_stats, &last_frame_stats, num_mbs,
          rc->constrained_gf_group, twopass->kf_zeromotion_pct, i, features);
      // Infer using ML model.
      float score;
      av1_nn_predict(features, &av1_use_flat_gop_nn_config, 1, &score);
      use_alt_ref = (score <= 0.0);
    }
  } else {
    use_alt_ref =
        rc->use_arf_in_this_kf_group && (i < gf_cfg->lag_in_frames) && (i > 2);
  }

#define REDUCE_GF_LENGTH_THRESH 4
#define REDUCE_GF_LENGTH_TO_KEY_THRESH 9
#define REDUCE_GF_LENGTH_BY 1
  int alt_offset = 0;
  // The length reduction strategy is tweaked for certain cases, and doesn't
  // work well for certain other cases.
  const int allow_gf_length_reduction =
      ((rc_cfg->mode == AOM_Q && rc_cfg->cq_level <= 128) ||
       !cpi->internal_altref_allowed) &&
      !is_lossless_requested(rc_cfg);

  if (allow_gf_length_reduction && use_alt_ref) {
    // adjust length of this gf group if one of the following condition met
    // 1: only one overlay frame left and this gf is too long
    // 2: next gf group is too short to have arf compared to the current gf

    // maximum length of next gf group
    const int next_gf_len = rc->frames_to_key - i;
    const int single_overlay_left =
        next_gf_len == 0 && i > REDUCE_GF_LENGTH_THRESH;
    // the next gf is probably going to have a ARF but it will be shorter than
    // this gf
    const int unbalanced_gf =
        i > REDUCE_GF_LENGTH_TO_KEY_THRESH &&
        next_gf_len + 1 < REDUCE_GF_LENGTH_TO_KEY_THRESH &&
        next_gf_len + 1 >= rc->min_gf_interval;

    if (single_overlay_left || unbalanced_gf) {
      const int roll_back = REDUCE_GF_LENGTH_BY;
      // Reduce length only if active_min_gf_interval will be respected later.
      if (i - roll_back >= active_min_gf_interval + 1) {
        alt_offset = -roll_back;
        i -= roll_back;
        if (is_final_pass) rc->intervals_till_gf_calculate_due = 0;
      }
    }
  }

  // Should we use the alternate reference frame.
  int ext_len = i - is_intra_only;
  if (use_alt_ref) {
    gf_group->max_layer_depth_allowed = gf_cfg->gf_max_pyr_height;
    set_baseline_gf_interval(cpi, i, active_max_gf_interval, use_alt_ref,
                             is_final_pass);

    const int forward_frames = (rc->frames_to_key - i >= ext_len)
                                   ? ext_len
                                   : AOMMAX(0, rc->frames_to_key - i);

    // Calculate the boost for alt ref.
    rc->gfu_boost = av1_calc_arf_boost(
        twopass, rc, frame_info, alt_offset, forward_frames, ext_len,
        cpi->lap_enabled ? &rc->num_stats_used_for_gfu_boost : NULL,
        cpi->lap_enabled ? &rc->num_stats_required_for_gfu_boost : NULL);
  } else {
    reset_fpf_position(twopass, start_pos);
    gf_group->max_layer_depth_allowed = 0;
    set_baseline_gf_interval(cpi, i, active_max_gf_interval, use_alt_ref,
                             is_final_pass);

    rc->gfu_boost = AOMMIN(
        MAX_GF_BOOST,
        av1_calc_arf_boost(
            twopass, rc, frame_info, alt_offset, ext_len, 0,
            cpi->lap_enabled ? &rc->num_stats_used_for_gfu_boost : NULL,
            cpi->lap_enabled ? &rc->num_stats_required_for_gfu_boost : NULL));
  }

#define LAST_ALR_BOOST_FACTOR 0.2f
  rc->arf_boost_factor = 1.0;
  if (use_alt_ref && !is_lossless_requested(rc_cfg)) {
    // Reduce the boost of altref in the last gf group
    if (rc->frames_to_key - ext_len == REDUCE_GF_LENGTH_BY ||
        rc->frames_to_key - ext_len == 0) {
      rc->arf_boost_factor = LAST_ALR_BOOST_FACTOR;
    }
  }

  rc->frames_till_gf_update_due = rc->baseline_gf_interval;

  // Reset the file position.
  reset_fpf_position(twopass, start_pos);

  if (cpi->lap_enabled) {
    // Since we don't have enough stats to know the actual error of the
    // gf group, we assume error of each frame to be equal to 1 and set
    // the error of the group as baseline_gf_interval.
    gf_stats.gf_group_err = rc->baseline_gf_interval;
  }
  // Calculate the bits to be allocated to the gf/arf group as a whole
  gf_group_bits = calculate_total_gf_group_bits(cpi, gf_stats.gf_group_err);
  rc->gf_group_bits = gf_group_bits;

#if GROUP_ADAPTIVE_MAXQ
  // Calculate an estimate of the maxq needed for the group.
  // We are more agressive about correcting for sections
  // where there could be significant overshoot than for easier
  // sections where we do not wish to risk creating an overshoot
  // of the allocated bit budget.
  if ((rc_cfg->mode != AOM_Q) && (rc->baseline_gf_interval > 1) &&
      is_final_pass) {
    const int vbr_group_bits_per_frame =
        (int)(gf_group_bits / rc->baseline_gf_interval);
    const double group_av_err =
        gf_stats.gf_group_raw_error / rc->baseline_gf_interval;
    const double group_av_skip_pct =
        gf_stats.gf_group_skip_pct / rc->baseline_gf_interval;
    const double group_av_inactive_zone =
        ((gf_stats.gf_group_inactive_zone_rows * 2) /
         (rc->baseline_gf_interval * (double)cm->mi_params.mb_rows));

    int tmp_q;
    tmp_q = get_twopass_worst_quality(
        cpi, group_av_err, (group_av_skip_pct + group_av_inactive_zone),
        vbr_group_bits_per_frame);
    rc->active_worst_quality = AOMMAX(tmp_q, rc->active_worst_quality >> 1);
  }
#endif

  // Adjust KF group bits and error remaining.
  if (is_final_pass)
    twopass->kf_group_error_left -= (int64_t)gf_stats.gf_group_err;

  // Set up the structure of this Group-Of-Pictures (same as GF_GROUP)
  av1_gop_setup_structure(cpi);

  // Reset the file position.
  reset_fpf_position(twopass, start_pos);

  // Calculate a section intra ratio used in setting max loop filter.
  if (rc->frames_since_key != 0) {
    twopass->section_intra_rating = calculate_section_intra_ratio(
        start_pos, twopass->stats_buf_ctx->stats_in_end,
        rc->baseline_gf_interval);
  }

  av1_gop_bit_allocation(cpi, rc, gf_group, rc->frames_since_key == 0,
                         use_alt_ref, gf_group_bits);

  frame_params->frame_type =
      rc->frames_since_key == 0 ? KEY_FRAME : INTER_FRAME;
  frame_params->show_frame =
      !(gf_group->update_type[gf_group->index] == ARF_UPDATE ||
        gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE);

  // TODO(jingning): Generalize this condition.
  if (is_final_pass) {
    cpi->gf_state.arf_gf_boost_lst = use_alt_ref;

    // Reset rolling actual and target bits counters for ARF groups.
    twopass->rolling_arf_group_target_bits = 1;
    twopass->rolling_arf_group_actual_bits = 1;
  }
}

// #define FIXED_ARF_BITS
#ifdef FIXED_ARF_BITS
#define ARF_BITS_FRACTION 0.75
#endif
void av1_gop_bit_allocation(const AV1_COMP *cpi, RATE_CONTROL *const rc,
                            GF_GROUP *gf_group, int is_key_frame, int use_arf,
                            int64_t gf_group_bits) {
  // Calculate the extra bits to be used for boosted frame(s)
#ifdef FIXED_ARF_BITS
  int gf_arf_bits = (int)(ARF_BITS_FRACTION * gf_group_bits);
#else
  int gf_arf_bits = calculate_boost_bits(
      rc->baseline_gf_interval - (rc->frames_since_key == 0), rc->gfu_boost,
      gf_group_bits);
#endif

  gf_arf_bits = adjust_boost_bits_for_target_level(cpi, rc, gf_arf_bits,
                                                   gf_group_bits, 1);

  // Allocate bits to each of the frames in the GF group.
  allocate_gf_group_bits(gf_group, rc, gf_group_bits, gf_arf_bits, is_key_frame,
                         use_arf);
}

// Minimum % intra coding observed in first pass (1.0 = 100%)
#define MIN_INTRA_LEVEL 0.25
// Minimum ratio between the % of intra coding and inter coding in the first
// pass after discounting neutral blocks (discounting neutral blocks in this
// way helps catch scene cuts in clips with very flat areas or letter box
// format clips with image padding.
#define INTRA_VS_INTER_THRESH 2.0
// Hard threshold where the first pass chooses intra for almost all blocks.
// In such a case even if the frame is not a scene cut coding a key frame
// may be a good option.
#define VERY_LOW_INTER_THRESH 0.05
// Maximum threshold for the relative ratio of intra error score vs best
// inter error score.
#define KF_II_ERR_THRESHOLD 1.9
// In real scene cuts there is almost always a sharp change in the intra
// or inter error score.
#define ERR_CHANGE_THRESHOLD 0.4
// For real scene cuts we expect an improvment in the intra inter error
// ratio in the next frame.
#define II_IMPROVEMENT_THRESHOLD 3.5
#define KF_II_MAX 128.0
// Intra / Inter threshold very low
#define VERY_LOW_II 1.5
// Clean slide transitions we expect a sharp single frame spike in error.
#define ERROR_SPIKE 5.0

// Slide show transition detection.
// Tests for case where there is very low error either side of the current frame
// but much higher just for this frame. This can help detect key frames in
// slide shows even where the slides are pictures of different sizes.
// Also requires that intra and inter errors are very similar to help eliminate
// harmful false positives.
// It will not help if the transition is a fade or other multi-frame effect.
static int slide_transition(const FIRSTPASS_STATS *this_frame,
                            const FIRSTPASS_STATS *last_frame,
                            const FIRSTPASS_STATS *next_frame) {
  return (this_frame->intra_error < (this_frame->coded_error * VERY_LOW_II)) &&
         (this_frame->coded_error > (last_frame->coded_error * ERROR_SPIKE)) &&
         (this_frame->coded_error > (next_frame->coded_error * ERROR_SPIKE));
}

// Threshold for use of the lagging second reference frame. High second ref
// usage may point to a transient event like a flash or occlusion rather than
// a real scene cut.
// We adapt the threshold based on number of frames in this key-frame group so
// far.
static double get_second_ref_usage_thresh(int frame_count_so_far) {
  const int adapt_upto = 32;
  const double min_second_ref_usage_thresh = 0.085;
  const double second_ref_usage_thresh_max_delta = 0.035;
  if (frame_count_so_far >= adapt_upto) {
    return min_second_ref_usage_thresh + second_ref_usage_thresh_max_delta;
  }
  return min_second_ref_usage_thresh +
         ((double)frame_count_so_far / (adapt_upto - 1)) *
             second_ref_usage_thresh_max_delta;
}

static int test_candidate_kf(TWO_PASS *twopass,
                             const FIRSTPASS_STATS *last_frame,
                             const FIRSTPASS_STATS *this_frame,
                             const FIRSTPASS_STATS *next_frame,
                             int frame_count_so_far, enum aom_rc_mode rc_mode,
                             int scenecut_mode) {
  int is_viable_kf = 0;
  double pcnt_intra = 1.0 - this_frame->pcnt_inter;
  double modified_pcnt_inter =
      this_frame->pcnt_inter - this_frame->pcnt_neutral;
  const double second_ref_usage_thresh =
      get_second_ref_usage_thresh(frame_count_so_far);
  int total_frames_to_test = SCENE_CUT_KEY_TEST_INTERVAL;
  int count_for_tolerable_prediction = 3;
  int num_future_frames = 0;
  FIRSTPASS_STATS curr_frame;

  if (scenecut_mode == ENABLE_SCENECUT_MODE_1) {
    curr_frame = *this_frame;
    const FIRSTPASS_STATS *const start_position = twopass->stats_in;
    for (num_future_frames = 0; num_future_frames < SCENE_CUT_KEY_TEST_INTERVAL;
         num_future_frames++)
      if (EOF == input_stats(twopass, &curr_frame)) break;
    reset_fpf_position(twopass, start_position);
    if (num_future_frames < 3) {
      return 0;
    } else {
      total_frames_to_test = 3;
      count_for_tolerable_prediction = 1;
    }
  }

  // Does the frame satisfy the primary criteria of a key frame?
  // See above for an explanation of the test criteria.
  // If so, then examine how well it predicts subsequent frames.
  if (IMPLIES(rc_mode == AOM_Q, frame_count_so_far >= 3) &&
      (this_frame->pcnt_second_ref < second_ref_usage_thresh) &&
      (next_frame->pcnt_second_ref < second_ref_usage_thresh) &&
      ((this_frame->pcnt_inter < VERY_LOW_INTER_THRESH) ||
       slide_transition(this_frame, last_frame, next_frame) ||
       ((pcnt_intra > MIN_INTRA_LEVEL) &&
        (pcnt_intra > (INTRA_VS_INTER_THRESH * modified_pcnt_inter)) &&
        ((this_frame->intra_error /
          DOUBLE_DIVIDE_CHECK(this_frame->coded_error)) <
         KF_II_ERR_THRESHOLD) &&
        ((fabs(last_frame->coded_error - this_frame->coded_error) /
              DOUBLE_DIVIDE_CHECK(this_frame->coded_error) >
          ERR_CHANGE_THRESHOLD) ||
         (fabs(last_frame->intra_error - this_frame->intra_error) /
              DOUBLE_DIVIDE_CHECK(this_frame->intra_error) >
          ERR_CHANGE_THRESHOLD) ||
         ((next_frame->intra_error /
           DOUBLE_DIVIDE_CHECK(next_frame->coded_error)) >
          II_IMPROVEMENT_THRESHOLD))))) {
    int i;
    const FIRSTPASS_STATS *start_pos = twopass->stats_in;
    double boost_score = 0.0;
    double old_boost_score = 0.0;
    double decay_accumulator = 1.0;

    // Examine how well the key frame predicts subsequent frames.
    for (i = 0; i < total_frames_to_test; ++i) {
      // Get the next frame details
      FIRSTPASS_STATS local_next_frame;
      if (EOF == input_stats(twopass, &local_next_frame)) break;
      double next_iiratio = (BOOST_FACTOR * local_next_frame.intra_error /
                             DOUBLE_DIVIDE_CHECK(local_next_frame.coded_error));

      if (next_iiratio > KF_II_MAX) next_iiratio = KF_II_MAX;

      // Cumulative effect of decay in prediction quality.
      if (local_next_frame.pcnt_inter > 0.85)
        decay_accumulator *= local_next_frame.pcnt_inter;
      else
        decay_accumulator *= (0.85 + local_next_frame.pcnt_inter) / 2.0;

      // Keep a running total.
      boost_score += (decay_accumulator * next_iiratio);

      // Test various breakout clauses.
      if ((local_next_frame.pcnt_inter < 0.05) || (next_iiratio < 1.5) ||
          (((local_next_frame.pcnt_inter - local_next_frame.pcnt_neutral) <
            0.20) &&
           (next_iiratio < 3.0)) ||
          ((boost_score - old_boost_score) < 3.0) ||
          (local_next_frame.intra_error < 200)) {
        break;
      }

      old_boost_score = boost_score;
    }

    // If there is tolerable prediction for at least the next 3 frames then
    // break out else discard this potential key frame and move on
    if (boost_score > 30.0 && (i > count_for_tolerable_prediction)) {
      is_viable_kf = 1;
    } else {
      is_viable_kf = 0;
    }

    // Reset the file position
    reset_fpf_position(twopass, start_pos);
  }
  return is_viable_kf;
}

#define FRAMES_TO_CHECK_DECAY 8
#define KF_MIN_FRAME_BOOST 80.0
#define KF_MAX_FRAME_BOOST 128.0
#define MIN_KF_BOOST 600  // Minimum boost for non-static KF interval
#define MAX_KF_BOOST 3200
#define MIN_STATIC_KF_BOOST 5400  // Minimum boost for static KF interval

static int detect_app_forced_key(AV1_COMP *cpi) {
  if (cpi->oxcf.kf_cfg.fwd_kf_enabled) cpi->rc.next_is_fwd_key = 1;
  int num_frames_to_app_forced_key = is_forced_keyframe_pending(
      cpi->lookahead, cpi->lookahead->max_sz, cpi->compressor_stage);
  if (num_frames_to_app_forced_key != -1) cpi->rc.next_is_fwd_key = 0;
  return num_frames_to_app_forced_key;
}

static int get_projected_kf_boost(AV1_COMP *cpi) {
  /*
   * If num_stats_used_for_kf_boost >= frames_to_key, then
   * all stats needed for prior boost calculation are available.
   * Hence projecting the prior boost is not needed in this cases.
   */
  if (cpi->rc.num_stats_used_for_kf_boost >= cpi->rc.frames_to_key)
    return cpi->rc.kf_boost;

  // Get the current tpl factor (number of frames = frames_to_key).
  double tpl_factor = av1_get_kf_boost_projection_factor(cpi->rc.frames_to_key);
  // Get the tpl factor when number of frames = num_stats_used_for_kf_boost.
  double tpl_factor_num_stats =
      av1_get_kf_boost_projection_factor(cpi->rc.num_stats_used_for_kf_boost);
  int projected_kf_boost =
      (int)rint((tpl_factor * cpi->rc.kf_boost) / tpl_factor_num_stats);
  return projected_kf_boost;
}

/*!\brief Determine the location of the next key frame
 *
 * \ingroup gf_group_algo
 * This function decides the placement of the next key frame when a
 * scenecut is detected or the maximum key frame distance is reached.
 *
 * \param[in]    cpi              Top-level encoder structure
 * \param[in]    this_frame       Pointer to first pass stats
 * \param[out]   kf_group_err     The total error in the KF group
 * \param[in]    num_frames_to_detect_scenecut Maximum lookahead frames.
 *
 * \return       Number of frames to the next key.
 */
static int define_kf_interval(AV1_COMP *cpi, FIRSTPASS_STATS *this_frame,
                              double *kf_group_err,
                              int num_frames_to_detect_scenecut) {
  TWO_PASS *const twopass = &cpi->twopass;
  RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const KeyFrameCfg *const kf_cfg = &oxcf->kf_cfg;
  double recent_loop_decay[FRAMES_TO_CHECK_DECAY];
  FIRSTPASS_STATS last_frame;
  double decay_accumulator = 1.0;
  int i = 0, j;
  int frames_to_key = 1;
  int frames_since_key = rc->frames_since_key + 1;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  int num_stats_used_for_kf_boost = 1;
  int scenecut_detected = 0;

  int num_frames_to_next_key = detect_app_forced_key(cpi);

  if (num_frames_to_detect_scenecut == 0) {
    if (num_frames_to_next_key != -1)
      return num_frames_to_next_key;
    else
      return rc->frames_to_key;
  }

  if (num_frames_to_next_key != -1)
    num_frames_to_detect_scenecut =
        AOMMIN(num_frames_to_detect_scenecut, num_frames_to_next_key);

  // Initialize the decay rates for the recent frames to check
  for (j = 0; j < FRAMES_TO_CHECK_DECAY; ++j) recent_loop_decay[j] = 1.0;

  i = 0;
  while (twopass->stats_in < twopass->stats_buf_ctx->stats_in_end &&
         frames_to_key < num_frames_to_detect_scenecut) {
    // Accumulate total number of stats available till next key frame
    num_stats_used_for_kf_boost++;

    // Accumulate kf group error.
    if (kf_group_err != NULL)
      *kf_group_err +=
          calculate_modified_err(frame_info, twopass, oxcf, this_frame);

    // Load the next frame's stats.
    last_frame = *this_frame;
    input_stats(twopass, this_frame);

    // Provided that we are not at the end of the file...
    if ((cpi->rc.enable_scenecut_detection > 0) && kf_cfg->auto_key &&
        twopass->stats_in < twopass->stats_buf_ctx->stats_in_end) {
      double loop_decay_rate;

      // Check for a scene cut.
      if (frames_since_key >= kf_cfg->key_freq_min &&
          test_candidate_kf(twopass, &last_frame, this_frame, twopass->stats_in,
                            frames_since_key, oxcf->rc_cfg.mode,
                            cpi->rc.enable_scenecut_detection)) {
        scenecut_detected = 1;
        break;
      }

      // How fast is the prediction quality decaying?
      loop_decay_rate =
          get_prediction_decay_rate(frame_info, twopass->stats_in);

      // We want to know something about the recent past... rather than
      // as used elsewhere where we are concerned with decay in prediction
      // quality since the last GF or KF.
      recent_loop_decay[i % FRAMES_TO_CHECK_DECAY] = loop_decay_rate;
      decay_accumulator = 1.0;
      for (j = 0; j < FRAMES_TO_CHECK_DECAY; ++j)
        decay_accumulator *= recent_loop_decay[j];

      // Special check for transition or high motion followed by a
      // static scene.
      if (frames_since_key >= kf_cfg->key_freq_min &&
          detect_transition_to_still(twopass, rc->min_gf_interval, i,
                                     kf_cfg->key_freq_max - i, loop_decay_rate,
                                     decay_accumulator)) {
        scenecut_detected = 1;
        // In the case of transition followed by a static scene, the key frame
        // could be a good predictor for the following frames, therefore we
        // do not use an arf.
        rc->use_arf_in_this_kf_group = 0;
        break;
      }

      // Step on to the next frame.
      ++frames_to_key;
      ++frames_since_key;

      // If we don't have a real key frame within the next two
      // key_freq_max intervals then break out of the loop.
      if (frames_to_key >= 2 * kf_cfg->key_freq_max) break;
    } else {
      ++frames_to_key;
      ++frames_since_key;
    }
    ++i;
  }

  if (kf_group_err != NULL)
    rc->num_stats_used_for_kf_boost = num_stats_used_for_kf_boost;

  if (cpi->lap_enabled && !scenecut_detected)
    frames_to_key = num_frames_to_next_key;

  if (!kf_cfg->fwd_kf_enabled || scenecut_detected ||
      twopass->stats_in >= twopass->stats_buf_ctx->stats_in_end)
    rc->next_is_fwd_key = 0;

  return frames_to_key;
}

static double get_kf_group_avg_error(TWO_PASS *twopass,
                                     const FIRSTPASS_STATS *first_frame,
                                     const FIRSTPASS_STATS *start_position,
                                     int frames_to_key) {
  FIRSTPASS_STATS cur_frame = *first_frame;
  int num_frames, i;
  double kf_group_avg_error = 0.0;

  reset_fpf_position(twopass, start_position);

  for (i = 0; i < frames_to_key; ++i) {
    kf_group_avg_error += cur_frame.coded_error;
    if (EOF == input_stats(twopass, &cur_frame)) break;
  }
  num_frames = i + 1;
  num_frames = AOMMIN(num_frames, frames_to_key);
  kf_group_avg_error = kf_group_avg_error / num_frames;

  return (kf_group_avg_error);
}

static int64_t get_kf_group_bits(AV1_COMP *cpi, double kf_group_err,
                                 double kf_group_avg_error) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  int64_t kf_group_bits;
  if (cpi->lap_enabled) {
    kf_group_bits = (int64_t)rc->frames_to_key * rc->avg_frame_bandwidth;
    if (cpi->oxcf.rc_cfg.vbr_corpus_complexity_lap) {
      const int num_mbs = (cpi->oxcf.resize_cfg.resize_mode != RESIZE_NONE)
                              ? cpi->initial_mbs
                              : cpi->common.mi_params.MBs;

      double vbr_corpus_complexity_lap =
          cpi->oxcf.rc_cfg.vbr_corpus_complexity_lap / 10.0;
      /* Get the average corpus complexity of the frame */
      vbr_corpus_complexity_lap = vbr_corpus_complexity_lap * num_mbs;
      kf_group_bits = (int64_t)(
          kf_group_bits * (kf_group_avg_error / vbr_corpus_complexity_lap));
    }
  } else {
    kf_group_bits = (int64_t)(twopass->bits_left *
                              (kf_group_err / twopass->modified_error_left));
  }

  return kf_group_bits;
}

static int calc_avg_stats(AV1_COMP *cpi, FIRSTPASS_STATS *avg_frame_stat) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  FIRSTPASS_STATS cur_frame;
  av1_zero(cur_frame);
  int num_frames = 0;
  // Accumulate total stat using available number of stats.
  for (num_frames = 0; num_frames < (rc->frames_to_key - 1); ++num_frames) {
    if (EOF == input_stats(twopass, &cur_frame)) break;
    av1_accumulate_stats(avg_frame_stat, &cur_frame);
  }

  if (num_frames < 2) {
    return num_frames;
  }
  // Average the total stat
  avg_frame_stat->weight = avg_frame_stat->weight / num_frames;
  avg_frame_stat->intra_error = avg_frame_stat->intra_error / num_frames;
  avg_frame_stat->frame_avg_wavelet_energy =
      avg_frame_stat->frame_avg_wavelet_energy / num_frames;
  avg_frame_stat->coded_error = avg_frame_stat->coded_error / num_frames;
  avg_frame_stat->sr_coded_error = avg_frame_stat->sr_coded_error / num_frames;
  avg_frame_stat->pcnt_inter = avg_frame_stat->pcnt_inter / num_frames;
  avg_frame_stat->pcnt_motion = avg_frame_stat->pcnt_motion / num_frames;
  avg_frame_stat->pcnt_second_ref =
      avg_frame_stat->pcnt_second_ref / num_frames;
  avg_frame_stat->pcnt_neutral = avg_frame_stat->pcnt_neutral / num_frames;
  avg_frame_stat->intra_skip_pct = avg_frame_stat->intra_skip_pct / num_frames;
  avg_frame_stat->inactive_zone_rows =
      avg_frame_stat->inactive_zone_rows / num_frames;
  avg_frame_stat->inactive_zone_cols =
      avg_frame_stat->inactive_zone_cols / num_frames;
  avg_frame_stat->MVr = avg_frame_stat->MVr / num_frames;
  avg_frame_stat->mvr_abs = avg_frame_stat->mvr_abs / num_frames;
  avg_frame_stat->MVc = avg_frame_stat->MVc / num_frames;
  avg_frame_stat->mvc_abs = avg_frame_stat->mvc_abs / num_frames;
  avg_frame_stat->MVrv = avg_frame_stat->MVrv / num_frames;
  avg_frame_stat->MVcv = avg_frame_stat->MVcv / num_frames;
  avg_frame_stat->mv_in_out_count =
      avg_frame_stat->mv_in_out_count / num_frames;
  avg_frame_stat->new_mv_count = avg_frame_stat->new_mv_count / num_frames;
  avg_frame_stat->count = avg_frame_stat->count / num_frames;
  avg_frame_stat->duration = avg_frame_stat->duration / num_frames;

  return num_frames;
}

static double get_kf_boost_score(AV1_COMP *cpi, double kf_raw_err,
                                 double *zero_motion_accumulator,
                                 double *sr_accumulator, int use_avg_stat) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  FIRSTPASS_STATS frame_stat;
  av1_zero(frame_stat);
  int i = 0, num_stat_used = 0;
  double boost_score = 0.0;
  const double kf_max_boost =
      cpi->oxcf.rc_cfg.mode == AOM_Q
          ? AOMMIN(AOMMAX(rc->frames_to_key * 2.0, KF_MIN_FRAME_BOOST),
                   KF_MAX_FRAME_BOOST)
          : KF_MAX_FRAME_BOOST;

  // Calculate the average using available number of stats.
  if (use_avg_stat) num_stat_used = calc_avg_stats(cpi, &frame_stat);

  for (i = num_stat_used; i < (rc->frames_to_key - 1); ++i) {
    if (!use_avg_stat && EOF == input_stats(twopass, &frame_stat)) break;

    // Monitor for static sections.
    // For the first frame in kf group, the second ref indicator is invalid.
    if (i > 0) {
      *zero_motion_accumulator =
          AOMMIN(*zero_motion_accumulator,
                 get_zero_motion_factor(frame_info, &frame_stat));
    } else {
      *zero_motion_accumulator = frame_stat.pcnt_inter - frame_stat.pcnt_motion;
    }

    // Not all frames in the group are necessarily used in calculating boost.
    if ((*sr_accumulator < (kf_raw_err * 1.50)) &&
        (i <= rc->max_gf_interval * 2)) {
      double frame_boost;
      double zm_factor;

      // Factor 0.75-1.25 based on how much of frame is static.
      zm_factor = (0.75 + (*zero_motion_accumulator / 2.0));

      if (i < 2) *sr_accumulator = 0.0;
      frame_boost = calc_kf_frame_boost(rc, frame_info, &frame_stat,
                                        sr_accumulator, kf_max_boost);
      boost_score += frame_boost * zm_factor;
    }
  }
  return boost_score;
}

/*!\brief Interval(in seconds) to clip key-frame distance to in LAP.
 */
#define MAX_KF_BITS_INTERVAL_SINGLE_PASS 5

/*!\brief Determine the next key frame group
 *
 * \ingroup gf_group_algo
 * This function decides the placement of the next key frame, and
 * calculates the bit allocation of the KF group and the keyframe itself.
 *
 * \param[in]    cpi              Top-level encoder structure
 * \param[in]    this_frame       Pointer to first pass stats
 *
 * \return Nothing is returned.
 */
static void find_next_key_frame(AV1_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &cpi->gf_group;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  AV1_COMMON *const cm = &cpi->common;
  CurrentFrame *const current_frame = &cm->current_frame;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const KeyFrameCfg *const kf_cfg = &oxcf->kf_cfg;
  const FIRSTPASS_STATS first_frame = *this_frame;
  FIRSTPASS_STATS next_frame;
  av1_zero(next_frame);

  rc->frames_since_key = 0;
  // Use arfs if possible.
  rc->use_arf_in_this_kf_group = is_altref_enabled(
      oxcf->gf_cfg.lag_in_frames, oxcf->gf_cfg.enable_auto_arf);

  // Reset the GF group data structures.
  av1_zero(*gf_group);

  // KF is always a GF so clear frames till next gf counter.
  rc->frames_till_gf_update_due = 0;

  rc->frames_to_key = 1;

  if (has_no_stats_stage(cpi)) {
    int num_frames_to_app_forced_key = detect_app_forced_key(cpi);
    rc->this_key_frame_forced =
        current_frame->frame_number != 0 && rc->frames_to_key == 0;
    if (num_frames_to_app_forced_key != -1)
      rc->frames_to_key = num_frames_to_app_forced_key;
    else
      rc->frames_to_key = AOMMAX(1, kf_cfg->key_freq_max);
    correct_frames_to_key(cpi);
    rc->kf_boost = DEFAULT_KF_BOOST;
    gf_group->update_type[0] = KF_UPDATE;
    return;
  }
  int i;
  const FIRSTPASS_STATS *const start_position = twopass->stats_in;
  int kf_bits = 0;
  double zero_motion_accumulator = 1.0;
  double boost_score = 0.0;
  double kf_raw_err = 0.0;
  double kf_mod_err = 0.0;
  double kf_group_err = 0.0;
  double sr_accumulator = 0.0;
  double kf_group_avg_error = 0.0;
  int frames_to_key, frames_to_key_clipped = INT_MAX;
  int64_t kf_group_bits_clipped = INT64_MAX;

  // Is this a forced key frame by interval.
  rc->this_key_frame_forced = rc->next_key_frame_forced;

  twopass->kf_group_bits = 0;        // Total bits available to kf group
  twopass->kf_group_error_left = 0;  // Group modified error score.

  kf_raw_err = this_frame->intra_error;
  kf_mod_err = calculate_modified_err(frame_info, twopass, oxcf, this_frame);

  frames_to_key =
      define_kf_interval(cpi, this_frame, &kf_group_err, kf_cfg->key_freq_max);

  if (frames_to_key != -1)
    rc->frames_to_key = AOMMIN(kf_cfg->key_freq_max, frames_to_key);
  else
    rc->frames_to_key = kf_cfg->key_freq_max;

  if (cpi->lap_enabled) correct_frames_to_key(cpi);

  // If there is a max kf interval set by the user we must obey it.
  // We already breakout of the loop above at 2x max.
  // This code centers the extra kf if the actual natural interval
  // is between 1x and 2x.
  if (kf_cfg->auto_key && rc->frames_to_key > kf_cfg->key_freq_max) {
    FIRSTPASS_STATS tmp_frame = first_frame;

    rc->frames_to_key /= 2;

    // Reset to the start of the group.
    reset_fpf_position(twopass, start_position);

    kf_group_err = 0.0;

    // Rescan to get the correct error data for the forced kf group.
    for (i = 0; i < rc->frames_to_key; ++i) {
      kf_group_err +=
          calculate_modified_err(frame_info, twopass, oxcf, &tmp_frame);
      if (EOF == input_stats(twopass, &tmp_frame)) break;
    }
    rc->next_key_frame_forced = 1;
  } else if ((twopass->stats_in == twopass->stats_buf_ctx->stats_in_end &&
              is_stat_consumption_stage_twopass(cpi)) ||
             rc->frames_to_key >= kf_cfg->key_freq_max) {
    rc->next_key_frame_forced = 1;
  } else {
    rc->next_key_frame_forced = 0;
  }

  if (kf_cfg->fwd_kf_enabled) rc->next_is_fwd_key |= rc->next_key_frame_forced;

  // Special case for the last key frame of the file.
  if (twopass->stats_in >= twopass->stats_buf_ctx->stats_in_end) {
    // Accumulate kf group error.
    kf_group_err +=
        calculate_modified_err(frame_info, twopass, oxcf, this_frame);
    rc->next_is_fwd_key = 0;
  }

  // Calculate the number of bits that should be assigned to the kf group.
  if ((twopass->bits_left > 0 && twopass->modified_error_left > 0.0) ||
      (cpi->lap_enabled && oxcf->rc_cfg.mode != AOM_Q)) {
    // Maximum number of bits for a single normal frame (not key frame).
    const int max_bits = frame_max_bits(rc, oxcf);

    // Maximum number of bits allocated to the key frame group.
    int64_t max_grp_bits;

    if (oxcf->rc_cfg.vbr_corpus_complexity_lap) {
      kf_group_avg_error = get_kf_group_avg_error(
          twopass, &first_frame, start_position, rc->frames_to_key);
    }

    // Default allocation based on bits left and relative
    // complexity of the section.
    twopass->kf_group_bits =
        get_kf_group_bits(cpi, kf_group_err, kf_group_avg_error);
    // Clip based on maximum per frame rate defined by the user.
    max_grp_bits = (int64_t)max_bits * (int64_t)rc->frames_to_key;
    if (twopass->kf_group_bits > max_grp_bits)
      twopass->kf_group_bits = max_grp_bits;
  } else {
    twopass->kf_group_bits = 0;
  }
  twopass->kf_group_bits = AOMMAX(0, twopass->kf_group_bits);

  if (cpi->lap_enabled) {
    // In the case of single pass based on LAP, frames to  key may have an
    // inaccurate value, and hence should be clipped to an appropriate
    // interval.
    frames_to_key_clipped =
        (int)(MAX_KF_BITS_INTERVAL_SINGLE_PASS * cpi->framerate);

    // This variable calculates the bits allocated to kf_group with a clipped
    // frames_to_key.
    if (rc->frames_to_key > frames_to_key_clipped) {
      kf_group_bits_clipped =
          (int64_t)((double)twopass->kf_group_bits * frames_to_key_clipped /
                    rc->frames_to_key);
    }
  }

  // Reset the first pass file position.
  reset_fpf_position(twopass, start_position);

  // Scan through the kf group collating various stats used to determine
  // how many bits to spend on it.
  boost_score = get_kf_boost_score(cpi, kf_raw_err, &zero_motion_accumulator,
                                   &sr_accumulator, 0);
  reset_fpf_position(twopass, start_position);
  // Store the zero motion percentage
  twopass->kf_zeromotion_pct = (int)(zero_motion_accumulator * 100.0);

  // Calculate a section intra ratio used in setting max loop filter.
  twopass->section_intra_rating = calculate_section_intra_ratio(
      start_position, twopass->stats_buf_ctx->stats_in_end, rc->frames_to_key);

  rc->kf_boost = (int)boost_score;

  if (cpi->lap_enabled) {
    if (oxcf->rc_cfg.mode == AOM_Q) {
      rc->kf_boost = get_projected_kf_boost(cpi);
    } else {
      // TODO(any): Explore using average frame stats for AOM_Q as well.
      boost_score = get_kf_boost_score(
          cpi, kf_raw_err, &zero_motion_accumulator, &sr_accumulator, 1);
      reset_fpf_position(twopass, start_position);
      rc->kf_boost += (int)boost_score;
    }
  }

  // Special case for static / slide show content but don't apply
  // if the kf group is very short.
  if ((zero_motion_accumulator > STATIC_KF_GROUP_FLOAT_THRESH) &&
      (rc->frames_to_key > 8)) {
    rc->kf_boost = AOMMAX(rc->kf_boost, MIN_STATIC_KF_BOOST);
  } else {
    // Apply various clamps for min and max boost
    rc->kf_boost = AOMMAX(rc->kf_boost, (rc->frames_to_key * 3));
    rc->kf_boost = AOMMAX(rc->kf_boost, MIN_KF_BOOST);
#ifdef STRICT_RC
    rc->kf_boost = AOMMIN(rc->kf_boost, MAX_KF_BOOST);
#endif
  }

  // Work out how many bits to allocate for the key frame itself.
  // In case of LAP enabled for VBR, if the frames_to_key value is
  // very high, we calculate the bits based on a clipped value of
  // frames_to_key.
  kf_bits = calculate_boost_bits(
      AOMMIN(rc->frames_to_key, frames_to_key_clipped) - 1, rc->kf_boost,
      AOMMIN(twopass->kf_group_bits, kf_group_bits_clipped));
  // printf("kf boost = %d kf_bits = %d kf_zeromotion_pct = %d\n", rc->kf_boost,
  //        kf_bits, twopass->kf_zeromotion_pct);
  kf_bits = adjust_boost_bits_for_target_level(cpi, rc, kf_bits,
                                               twopass->kf_group_bits, 0);

  twopass->kf_group_bits -= kf_bits;

  // Save the bits to spend on the key frame.
  gf_group->bit_allocation[0] = kf_bits;
  gf_group->update_type[0] = KF_UPDATE;

  // Note the total error score of the kf group minus the key frame itself.
  if (cpi->lap_enabled)
    // As we don't have enough stats to know the actual error of the group,
    // we assume the complexity of each frame to be equal to 1, and set the
    // error as the number of frames in the group(minus the keyframe).
    twopass->kf_group_error_left = (int)(rc->frames_to_key - 1);
  else
    twopass->kf_group_error_left = (int)(kf_group_err - kf_mod_err);

  // Adjust the count of total modified error left.
  // The count of bits left is adjusted elsewhere based on real coded frame
  // sizes.
  twopass->modified_error_left -= kf_group_err;
}

static int is_skippable_frame(const AV1_COMP *cpi) {
  if (has_no_stats_stage(cpi)) return 0;
  // If the current frame does not have non-zero motion vector detected in the
  // first  pass, and so do its previous and forward frames, then this frame
  // can be skipped for partition check, and the partition size is assigned
  // according to the variance
  const TWO_PASS *const twopass = &cpi->twopass;

  return (!frame_is_intra_only(&cpi->common) &&
          twopass->stats_in - 2 > twopass->stats_buf_ctx->stats_in_start &&
          twopass->stats_in < twopass->stats_buf_ctx->stats_in_end &&
          (twopass->stats_in - 1)->pcnt_inter -
                  (twopass->stats_in - 1)->pcnt_motion ==
              1 &&
          (twopass->stats_in - 2)->pcnt_inter -
                  (twopass->stats_in - 2)->pcnt_motion ==
              1 &&
          twopass->stats_in->pcnt_inter - twopass->stats_in->pcnt_motion == 1);
}

#define ARF_STATS_OUTPUT 0
#if ARF_STATS_OUTPUT
unsigned int arf_count = 0;
#endif

static int get_section_target_bandwidth(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  CurrentFrame *const current_frame = &cm->current_frame;
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  int section_target_bandwidth;
  const int frames_left = (int)(twopass->stats_buf_ctx->total_stats->count -
                                current_frame->frame_number);
  if (cpi->lap_enabled)
    section_target_bandwidth = (int)rc->avg_frame_bandwidth;
  else
    section_target_bandwidth = (int)(twopass->bits_left / frames_left);
  return section_target_bandwidth;
}

static void process_first_pass_stats(AV1_COMP *cpi,
                                     FIRSTPASS_STATS *this_frame) {
  AV1_COMMON *const cm = &cpi->common;
  CurrentFrame *const current_frame = &cm->current_frame;
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;

  if (cpi->oxcf.rc_cfg.mode != AOM_Q && current_frame->frame_number == 0 &&
      cpi->gf_group.index == 0 && cpi->twopass.stats_buf_ctx->total_stats &&
      cpi->twopass.stats_buf_ctx->total_left_stats) {
    if (cpi->lap_enabled) {
      /*
       * Accumulate total_stats using available limited number of stats,
       * and assign it to total_left_stats.
       */
      *cpi->twopass.stats_buf_ctx->total_left_stats =
          *cpi->twopass.stats_buf_ctx->total_stats;
    }
    // Special case code for first frame.
    const int section_target_bandwidth = get_section_target_bandwidth(cpi);
    const double section_length =
        twopass->stats_buf_ctx->total_left_stats->count;
    const double section_error =
        twopass->stats_buf_ctx->total_left_stats->coded_error / section_length;
    const double section_intra_skip =
        twopass->stats_buf_ctx->total_left_stats->intra_skip_pct /
        section_length;
    const double section_inactive_zone =
        (twopass->stats_buf_ctx->total_left_stats->inactive_zone_rows * 2) /
        ((double)cm->mi_params.mb_rows * section_length);
    const int tmp_q = get_twopass_worst_quality(
        cpi, section_error, section_intra_skip + section_inactive_zone,
        section_target_bandwidth);

    rc->active_worst_quality = tmp_q;
    rc->ni_av_qi = tmp_q;
    rc->last_q[INTER_FRAME] = tmp_q;
    rc->avg_q = av1_convert_qindex_to_q(tmp_q, cm->seq_params.bit_depth);
    rc->avg_frame_qindex[INTER_FRAME] = tmp_q;
    rc->last_q[KEY_FRAME] = (tmp_q + cpi->oxcf.rc_cfg.best_allowed_q) / 2;
    rc->avg_frame_qindex[KEY_FRAME] = rc->last_q[KEY_FRAME];
  }

  int err = 0;
  if (cpi->lap_enabled) {
    err = input_stats_lap(twopass, this_frame);
  } else {
    err = input_stats(twopass, this_frame);
  }
  if (err == EOF) return;

  {
    const int num_mbs = (cpi->oxcf.resize_cfg.resize_mode != RESIZE_NONE)
                            ? cpi->initial_mbs
                            : cm->mi_params.MBs;
    // The multiplication by 256 reverses a scaling factor of (>> 8)
    // applied when combining MB error values for the frame.
    twopass->mb_av_energy = log((this_frame->intra_error / num_mbs) + 1.0);
    twopass->frame_avg_haar_energy =
        log((this_frame->frame_avg_wavelet_energy / num_mbs) + 1.0);
  }

  // Set the frame content type flag.
  if (this_frame->intra_skip_pct >= FC_ANIMATION_THRESH)
    twopass->fr_content_type = FC_GRAPHICS_ANIMATION;
  else
    twopass->fr_content_type = FC_NORMAL;
}

static void setup_target_rate(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  GF_GROUP *const gf_group = &cpi->gf_group;

  int target_rate = gf_group->bit_allocation[gf_group->index];

  if (has_no_stats_stage(cpi)) {
    av1_rc_set_frame_target(cpi, target_rate, cpi->common.width,
                            cpi->common.height);
  }

  rc->base_frame_target = target_rate;
}

void av1_get_second_pass_params(AV1_COMP *cpi,
                                EncodeFrameParams *const frame_params,
                                const EncodeFrameInput *const frame_input,
                                unsigned int frame_flags) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &cpi->gf_group;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  const FIRSTPASS_STATS *const start_pos = twopass->stats_in;

  if (is_stat_consumption_stage(cpi) && !twopass->stats_in) return;

  const int update_type = gf_group->update_type[gf_group->index];
  frame_params->frame_type = gf_group->frame_type[gf_group->index];

  if (gf_group->index < gf_group->size && !(frame_flags & FRAMEFLAGS_KEY)) {
    assert(gf_group->index < gf_group->size);

    setup_target_rate(cpi);

    // If this is an arf frame then we dont want to read the stats file or
    // advance the input pointer as we already have what we need.
    if (update_type == ARF_UPDATE || update_type == INTNL_ARF_UPDATE) {
      // Do the firstpass stats indicate that this frame is skippable for the
      // partition search?
      if (cpi->sf.part_sf.allow_partition_search_skip && oxcf->pass == 2) {
        cpi->partition_search_skippable_frame = is_skippable_frame(cpi);
      }
      return;
    }
  }

  aom_clear_system_state();

  if (oxcf->rc_cfg.mode == AOM_Q)
    rc->active_worst_quality = oxcf->rc_cfg.cq_level;
  FIRSTPASS_STATS this_frame;
  av1_zero(this_frame);
  // call above fn
  if (is_stat_consumption_stage(cpi)) {
    if (gf_group->index < gf_group->size || rc->frames_to_key == 0)
      process_first_pass_stats(cpi, &this_frame);
  } else {
    rc->active_worst_quality = oxcf->rc_cfg.cq_level;
  }

  // Keyframe and section processing.
  FIRSTPASS_STATS this_frame_copy;
  this_frame_copy = this_frame;
  int is_overlay_forward_kf =
      rc->frames_to_key == 0 &&
      gf_group->update_type[gf_group->index] == OVERLAY_UPDATE;
  if (rc->frames_to_key <= 0 && !is_overlay_forward_kf) {
    assert(rc->frames_to_key >= -1);
    // Define next KF group and assign bits to it.
    int kf_offset = rc->frames_to_key;
    if (rc->frames_to_key < 0) {
      this_frame = *(twopass->stats_in - 1);
    } else {
      frame_params->frame_type = KEY_FRAME;
    }
    find_next_key_frame(cpi, &this_frame);
    rc->frames_since_key -= kf_offset;
    rc->frames_to_key += kf_offset;
    this_frame = this_frame_copy;
  } else {
    const int altref_enabled = is_altref_enabled(oxcf->gf_cfg.lag_in_frames,
                                                 oxcf->gf_cfg.enable_auto_arf);
    const int sframe_dist = oxcf->kf_cfg.sframe_dist;
    const int sframe_mode = oxcf->kf_cfg.sframe_mode;
    CurrentFrame *const current_frame = &cpi->common.current_frame;
    if (sframe_dist != 0) {
      if (altref_enabled) {
        if (sframe_mode == 1) {
          // sframe_mode == 1: insert sframe if it matches altref frame.
          if (current_frame->frame_number % sframe_dist == 0 &&
              current_frame->frame_number != 0 && update_type == ARF_UPDATE) {
            frame_params->frame_type = S_FRAME;
          }
        } else {
          // sframe_mode != 1: if sframe will be inserted at the next available
          // altref frame
          if (current_frame->frame_number % sframe_dist == 0 &&
              current_frame->frame_number != 0) {
            rc->sframe_due = 1;
          }
          if (rc->sframe_due && update_type == ARF_UPDATE) {
            frame_params->frame_type = S_FRAME;
            rc->sframe_due = 0;
          }
        }
      } else {
        if (current_frame->frame_number % sframe_dist == 0 &&
            current_frame->frame_number != 0) {
          frame_params->frame_type = S_FRAME;
        }
      }
    }
  }

  // Define a new GF/ARF group. (Should always enter here for key frames).
  if (gf_group->index == gf_group->size) {
    assert(cpi->common.current_frame.frame_number == 0 ||
           gf_group->index == gf_group->size);
    const FIRSTPASS_STATS *const start_position = twopass->stats_in;

    if (cpi->lap_enabled && cpi->rc.enable_scenecut_detection) {
      int num_frames_to_detect_scenecut, frames_to_key;
      num_frames_to_detect_scenecut = MAX_GF_LENGTH_LAP + 1;
      frames_to_key = define_kf_interval(cpi, &this_frame, NULL,
                                         num_frames_to_detect_scenecut);
      if (frames_to_key != -1)
        rc->frames_to_key = AOMMIN(rc->frames_to_key, frames_to_key);
    }

    reset_fpf_position(twopass, start_position);

    int max_gop_length =
        (oxcf->gf_cfg.lag_in_frames >= 32 &&
         is_stat_consumption_stage_twopass(cpi))
            ? AOMMIN(MAX_GF_INTERVAL, oxcf->gf_cfg.lag_in_frames -
                                          oxcf->algo_cfg.arnr_max_frames / 2)
            : MAX_GF_LENGTH_LAP;

    // Identify regions if needed.
    if (rc->frames_since_key == 0 || rc->frames_since_key == 1 ||
        (rc->frames_till_regions_update - rc->frames_since_key <
             rc->frames_to_key &&
         rc->frames_till_regions_update - rc->frames_since_key <
             max_gop_length + 1)) {
      int is_first_stat =
          twopass->stats_in == twopass->stats_buf_ctx->stats_in_start;
      const FIRSTPASS_STATS *stats_start = twopass->stats_in + is_first_stat;
      // offset of stats_start from the current frame
      int offset = is_first_stat || (rc->frames_since_key == 0);
      // offset of the region indices from the previous key frame
      rc->regions_offset = rc->frames_since_key;
      // how many frames we can analyze from this frame
      int rest_frames = AOMMIN(rc->frames_to_key + rc->next_is_fwd_key,
                               MAX_FIRSTPASS_ANALYSIS_FRAMES);
      rest_frames =
          AOMMIN(rest_frames,
                 (int)(twopass->stats_buf_ctx->stats_in_end - stats_start + 1) +
                     offset);

      rc->frames_till_regions_update = rest_frames;

      identify_regions(stats_start, rest_frames - offset, offset, rc->regions,
                       &rc->num_regions, rc->cor_coeff);
    }

    int cur_region_idx =
        find_regions_index(rc->regions, rc->num_regions,
                           rc->frames_since_key - rc->regions_offset);
    if ((cur_region_idx >= 0 &&
         rc->regions[cur_region_idx].type == SCENECUT_REGION) ||
        rc->frames_since_key == 0) {
      // If we start from a scenecut, then the last GOP's arf boost is not
      // needed for this GOP.
      cpi->gf_state.arf_gf_boost_lst = 0;
    }

    // TODO(jingning): Resoleve the redundant calls here.
    if (rc->intervals_till_gf_calculate_due == 0 || 1) {
      calculate_gf_length(cpi, max_gop_length, MAX_NUM_GF_INTERVALS);
    }

    if (max_gop_length > 16 && oxcf->algo_cfg.enable_tpl_model &&
        !cpi->sf.tpl_sf.disable_gop_length_decision) {
      int this_idx = rc->frames_since_key + rc->gf_intervals[rc->cur_gf_index] -
                     rc->regions_offset - 1;
      int this_region =
          find_regions_index(rc->regions, rc->num_regions, this_idx);
      int next_region =
          find_regions_index(rc->regions, rc->num_regions, this_idx + 1);
      int is_last_scenecut =
          (rc->gf_intervals[rc->cur_gf_index] >= rc->frames_to_key ||
           rc->regions[this_region].type == SCENECUT_REGION ||
           rc->regions[next_region].type == SCENECUT_REGION);
      int ori_gf_int = rc->gf_intervals[rc->cur_gf_index];

      if (rc->gf_intervals[rc->cur_gf_index] > 16) {
        // The calculate_gf_length function is previously used with
        // max_gop_length = 32 with look-ahead gf intervals.
        define_gf_group(cpi, &this_frame, frame_params, max_gop_length, 0);
        this_frame = this_frame_copy;
        int is_temporal_filter_enabled =
            (rc->frames_since_key > 0 && gf_group->arf_index > -1);
        if (is_temporal_filter_enabled) {
          int arf_src_index = gf_group->arf_src_offset[gf_group->arf_index];
          FRAME_UPDATE_TYPE arf_update_type =
              gf_group->update_type[gf_group->arf_index];
          int is_forward_keyframe = 0;
          av1_temporal_filter(cpi, arf_src_index, arf_update_type,
                              is_forward_keyframe, NULL);
          aom_extend_frame_borders(&cpi->alt_ref_buffer,
                                   av1_num_planes(&cpi->common));
        }
        if (!av1_tpl_setup_stats(cpi, 1, frame_params, frame_input)) {
          // Tpl decides that a shorter gf interval is better.
          // TODO(jingning): Remove redundant computations here.
          max_gop_length = 16;
          calculate_gf_length(cpi, max_gop_length, 1);
          if (is_last_scenecut &&
              (ori_gf_int - rc->gf_intervals[rc->cur_gf_index] < 4)) {
            rc->gf_intervals[rc->cur_gf_index] = ori_gf_int;
          }
        } else {
          // Tpl stats is reused only when the ARF frame is temporally filtered
          if (is_temporal_filter_enabled)
            cpi->tpl_data.skip_tpl_setup_stats = 1;
        }
      }
    }
    define_gf_group(cpi, &this_frame, frame_params, max_gop_length, 0);

    if (gf_group->update_type[gf_group->index] != ARF_UPDATE &&
        rc->frames_since_key > 0)
      process_first_pass_stats(cpi, &this_frame);

    define_gf_group(cpi, &this_frame, frame_params, max_gop_length, 1);

    rc->frames_till_gf_update_due = rc->baseline_gf_interval;
    assert(gf_group->index == 0);
#if ARF_STATS_OUTPUT
    {
      FILE *fpfile;
      fpfile = fopen("arf.stt", "a");
      ++arf_count;
      fprintf(fpfile, "%10d %10d %10d %10d %10d\n",
              cpi->common.current_frame.frame_number,
              rc->frames_till_gf_update_due, rc->kf_boost, arf_count,
              rc->gfu_boost);

      fclose(fpfile);
    }
#endif
  }
  assert(gf_group->index < gf_group->size);

  if (gf_group->update_type[gf_group->index] == ARF_UPDATE ||
      gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE) {
    reset_fpf_position(twopass, start_pos);
  } else {
    // Update the total stats remaining structure.
    if (twopass->stats_buf_ctx->total_left_stats)
      subtract_stats(twopass->stats_buf_ctx->total_left_stats,
                     &this_frame_copy);
  }

  frame_params->frame_type = gf_group->frame_type[gf_group->index];

  // Do the firstpass stats indicate that this frame is skippable for the
  // partition search?
  if (cpi->sf.part_sf.allow_partition_search_skip && oxcf->pass == 2) {
    cpi->partition_search_skippable_frame = is_skippable_frame(cpi);
  }

  setup_target_rate(cpi);
}

void av1_init_second_pass(AV1_COMP *cpi) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->twopass;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  double frame_rate;
  FIRSTPASS_STATS *stats;

  if (!twopass->stats_buf_ctx->stats_in_end) return;

  stats = twopass->stats_buf_ctx->total_stats;

  *stats = *twopass->stats_buf_ctx->stats_in_end;
  *twopass->stats_buf_ctx->total_left_stats = *stats;

  frame_rate = 10000000.0 * stats->count / stats->duration;
  // Each frame can have a different duration, as the frame rate in the source
  // isn't guaranteed to be constant. The frame rate prior to the first frame
  // encoded in the second pass is a guess. However, the sum duration is not.
  // It is calculated based on the actual durations of all frames from the
  // first pass.
  av1_new_framerate(cpi, frame_rate);
  twopass->bits_left =
      (int64_t)(stats->duration * oxcf->rc_cfg.target_bandwidth / 10000000.0);

  // This variable monitors how far behind the second ref update is lagging.
  twopass->sr_update_lag = 1;

  // Scan the first pass file and calculate a modified total error based upon
  // the bias/power function used to allocate bits.
  {
    const double avg_error =
        stats->coded_error / DOUBLE_DIVIDE_CHECK(stats->count);
    const FIRSTPASS_STATS *s = twopass->stats_in;
    double modified_error_total = 0.0;
    twopass->modified_error_min =
        (avg_error * oxcf->rc_cfg.vbrmin_section) / 100;
    twopass->modified_error_max =
        (avg_error * oxcf->rc_cfg.vbrmax_section) / 100;
    while (s < twopass->stats_buf_ctx->stats_in_end) {
      modified_error_total +=
          calculate_modified_err(frame_info, twopass, oxcf, s);
      ++s;
    }
    twopass->modified_error_left = modified_error_total;
  }

  // Reset the vbr bits off target counters
  cpi->rc.vbr_bits_off_target = 0;
  cpi->rc.vbr_bits_off_target_fast = 0;

  cpi->rc.rate_error_estimate = 0;

  // Static sequence monitor variables.
  twopass->kf_zeromotion_pct = 100;
  twopass->last_kfgroup_zeromotion_pct = 100;

  // Initialize bits per macro_block estimate correction factor.
  twopass->bpm_factor = 1.0;
  // Initialize actual and target bits counters for ARF groups so that
  // at the start we have a neutral bpm adjustment.
  twopass->rolling_arf_group_target_bits = 1;
  twopass->rolling_arf_group_actual_bits = 1;
}

void av1_init_single_pass_lap(AV1_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->twopass;

  if (!twopass->stats_buf_ctx->stats_in_end) return;

  // This variable monitors how far behind the second ref update is lagging.
  twopass->sr_update_lag = 1;

  twopass->bits_left = 0;
  twopass->modified_error_min = 0.0;
  twopass->modified_error_max = 0.0;
  twopass->modified_error_left = 0.0;

  // Reset the vbr bits off target counters
  cpi->rc.vbr_bits_off_target = 0;
  cpi->rc.vbr_bits_off_target_fast = 0;

  cpi->rc.rate_error_estimate = 0;

  // Static sequence monitor variables.
  twopass->kf_zeromotion_pct = 100;
  twopass->last_kfgroup_zeromotion_pct = 100;

  // Initialize bits per macro_block estimate correction factor.
  twopass->bpm_factor = 1.0;
  // Initialize actual and target bits counters for ARF groups so that
  // at the start we have a neutral bpm adjustment.
  twopass->rolling_arf_group_target_bits = 1;
  twopass->rolling_arf_group_actual_bits = 1;
}

#define MINQ_ADJ_LIMIT 48
#define MINQ_ADJ_LIMIT_CQ 20
#define HIGH_UNDERSHOOT_RATIO 2
void av1_twopass_postencode_update(AV1_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->twopass;
  RATE_CONTROL *const rc = &cpi->rc;
  const RateControlCfg *const rc_cfg = &cpi->oxcf.rc_cfg;

  // VBR correction is done through rc->vbr_bits_off_target. Based on the
  // sign of this value, a limited % adjustment is made to the target rate
  // of subsequent frames, to try and push it back towards 0. This method
  // is designed to prevent extreme behaviour at the end of a clip
  // or group of frames.
  rc->vbr_bits_off_target += rc->base_frame_target - rc->projected_frame_size;
  twopass->bits_left = AOMMAX(twopass->bits_left - rc->base_frame_target, 0);

  // Target vs actual bits for this arf group.
  twopass->rolling_arf_group_target_bits += rc->base_frame_target;
  twopass->rolling_arf_group_actual_bits += rc->projected_frame_size;

  // Calculate the pct rc error.
  if (rc->total_actual_bits) {
    rc->rate_error_estimate =
        (int)((rc->vbr_bits_off_target * 100) / rc->total_actual_bits);
    rc->rate_error_estimate = clamp(rc->rate_error_estimate, -100, 100);
  } else {
    rc->rate_error_estimate = 0;
  }

  // Update the active best quality pyramid.
  if (!rc->is_src_frame_alt_ref) {
    const int pyramid_level = cpi->gf_group.layer_depth[cpi->gf_group.index];
    int i;
    for (i = pyramid_level; i <= MAX_ARF_LAYERS; ++i) {
      rc->active_best_quality[i] = cpi->common.quant_params.base_qindex;
#if CONFIG_TUNE_VMAF
      if (cpi->vmaf_info.original_qindex != -1 &&
          (cpi->oxcf.tune_cfg.tuning >= AOM_TUNE_VMAF_WITH_PREPROCESSING &&
           cpi->oxcf.tune_cfg.tuning <= AOM_TUNE_VMAF_NEG_MAX_GAIN)) {
        rc->active_best_quality[i] = cpi->vmaf_info.original_qindex;
      }
#endif
    }
  }

#if 0
  {
    AV1_COMMON *cm = &cpi->common;
    FILE *fpfile;
    fpfile = fopen("details.stt", "a");
    fprintf(fpfile,
            "%10d %10d %10d %10" PRId64 " %10" PRId64
            " %10d %10d %10d %10.4lf %10.4lf %10.4lf %10.4lf\n",
            cm->current_frame.frame_number, rc->base_frame_target,
            rc->projected_frame_size, rc->total_actual_bits,
            rc->vbr_bits_off_target, rc->rate_error_estimate,
            twopass->rolling_arf_group_target_bits,
            twopass->rolling_arf_group_actual_bits,
            (double)twopass->rolling_arf_group_actual_bits /
                (double)twopass->rolling_arf_group_target_bits,
            twopass->bpm_factor,
            av1_convert_qindex_to_q(cpi->common.quant_params.base_qindex,
                                    cm->seq_params.bit_depth),
            av1_convert_qindex_to_q(rc->active_worst_quality,
                                    cm->seq_params.bit_depth));
    fclose(fpfile);
  }
#endif

  if (cpi->common.current_frame.frame_type != KEY_FRAME) {
    twopass->kf_group_bits -= rc->base_frame_target;
    twopass->last_kfgroup_zeromotion_pct = twopass->kf_zeromotion_pct;
  }
  twopass->kf_group_bits = AOMMAX(twopass->kf_group_bits, 0);

  // If the rate control is drifting consider adjustment to min or maxq.
  if ((rc_cfg->mode != AOM_Q) && !cpi->rc.is_src_frame_alt_ref) {
    const int maxq_adj_limit = rc->worst_quality - rc->active_worst_quality;
    const int minq_adj_limit =
        (rc_cfg->mode == AOM_CQ ? MINQ_ADJ_LIMIT_CQ : MINQ_ADJ_LIMIT);

    // Undershoot.
    if (rc->rate_error_estimate > rc_cfg->under_shoot_pct) {
      --twopass->extend_maxq;
      if (rc->rolling_target_bits >= rc->rolling_actual_bits)
        ++twopass->extend_minq;
      // Overshoot.
    } else if (rc->rate_error_estimate < -rc_cfg->over_shoot_pct) {
      --twopass->extend_minq;
      if (rc->rolling_target_bits < rc->rolling_actual_bits)
        ++twopass->extend_maxq;
    } else {
      // Adjustment for extreme local overshoot.
      if (rc->projected_frame_size > (2 * rc->base_frame_target) &&
          rc->projected_frame_size > (2 * rc->avg_frame_bandwidth))
        ++twopass->extend_maxq;

      // Unwind undershoot or overshoot adjustment.
      if (rc->rolling_target_bits < rc->rolling_actual_bits)
        --twopass->extend_minq;
      else if (rc->rolling_target_bits > rc->rolling_actual_bits)
        --twopass->extend_maxq;
    }

    twopass->extend_minq = clamp(twopass->extend_minq, 0, minq_adj_limit);
    twopass->extend_maxq = clamp(twopass->extend_maxq, 0, maxq_adj_limit);

    // If there is a big and undexpected undershoot then feed the extra
    // bits back in quickly. One situation where this may happen is if a
    // frame is unexpectedly almost perfectly predicted by the ARF or GF
    // but not very well predcited by the previous frame.
    if (!frame_is_kf_gf_arf(cpi) && !cpi->rc.is_src_frame_alt_ref) {
      int fast_extra_thresh = rc->base_frame_target / HIGH_UNDERSHOOT_RATIO;
      if (rc->projected_frame_size < fast_extra_thresh) {
        rc->vbr_bits_off_target_fast +=
            fast_extra_thresh - rc->projected_frame_size;
        rc->vbr_bits_off_target_fast =
            AOMMIN(rc->vbr_bits_off_target_fast, (4 * rc->avg_frame_bandwidth));

        // Fast adaptation of minQ if necessary to use up the extra bits.
        if (rc->avg_frame_bandwidth) {
          twopass->extend_minq_fast =
              (int)(rc->vbr_bits_off_target_fast * 8 / rc->avg_frame_bandwidth);
        }
        twopass->extend_minq_fast = AOMMIN(
            twopass->extend_minq_fast, minq_adj_limit - twopass->extend_minq);
      } else if (rc->vbr_bits_off_target_fast) {
        twopass->extend_minq_fast = AOMMIN(
            twopass->extend_minq_fast, minq_adj_limit - twopass->extend_minq);
      } else {
        twopass->extend_minq_fast = 0;
      }
    }
  }
}
