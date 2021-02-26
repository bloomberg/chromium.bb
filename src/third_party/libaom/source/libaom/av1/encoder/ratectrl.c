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

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/alloccommon.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/common/common.h"
#include "av1/common/entropymode.h"
#include "av1/common/quant_common.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/encodemv.h"
#include "av1/encoder/encode_strategy.h"
#include "av1/encoder/gop_structure.h"
#include "av1/encoder/random.h"
#include "av1/encoder/ratectrl.h"

#define USE_UNRESTRICTED_Q_IN_CQ_MODE 0

// Max rate target for 1080P and below encodes under normal circumstances
// (1920 * 1080 / (16 * 16)) * MAX_MB_RATE bits per MB
#define MAX_MB_RATE 250
#define MAXRATE_1080P 2025000

#define MIN_BPB_FACTOR 0.005
#define MAX_BPB_FACTOR 50

#define SUPERRES_QADJ_PER_DENOM_KEYFRAME_SOLO 0
#define SUPERRES_QADJ_PER_DENOM_KEYFRAME 2
#define SUPERRES_QADJ_PER_DENOM_ARFFRAME 0

#define FRAME_OVERHEAD_BITS 200
#define ASSIGN_MINQ_TABLE(bit_depth, name)                   \
  do {                                                       \
    switch (bit_depth) {                                     \
      case AOM_BITS_8: name = name##_8; break;               \
      case AOM_BITS_10: name = name##_10; break;             \
      case AOM_BITS_12: name = name##_12; break;             \
      default:                                               \
        assert(0 &&                                          \
               "bit_depth should be AOM_BITS_8, AOM_BITS_10" \
               " or AOM_BITS_12");                           \
        name = NULL;                                         \
    }                                                        \
  } while (0)

// Tables relating active max Q to active min Q
static int kf_low_motion_minq_8[QINDEX_RANGE];
static int kf_high_motion_minq_8[QINDEX_RANGE];
static int arfgf_low_motion_minq_8[QINDEX_RANGE];
static int arfgf_high_motion_minq_8[QINDEX_RANGE];
static int inter_minq_8[QINDEX_RANGE];
static int rtc_minq_8[QINDEX_RANGE];

static int kf_low_motion_minq_10[QINDEX_RANGE];
static int kf_high_motion_minq_10[QINDEX_RANGE];
static int arfgf_low_motion_minq_10[QINDEX_RANGE];
static int arfgf_high_motion_minq_10[QINDEX_RANGE];
static int inter_minq_10[QINDEX_RANGE];
static int rtc_minq_10[QINDEX_RANGE];
static int kf_low_motion_minq_12[QINDEX_RANGE];
static int kf_high_motion_minq_12[QINDEX_RANGE];
static int arfgf_low_motion_minq_12[QINDEX_RANGE];
static int arfgf_high_motion_minq_12[QINDEX_RANGE];
static int inter_minq_12[QINDEX_RANGE];
static int rtc_minq_12[QINDEX_RANGE];

static int gf_high = 2400;
static int gf_low = 300;
#ifdef STRICT_RC
static int kf_high = 3200;
#else
static int kf_high = 5000;
#endif
static int kf_low = 400;

// How many times less pixels there are to encode given the current scaling.
// Temporary replacement for rcf_mult and rate_thresh_mult.
static double resize_rate_factor(const FrameDimensionCfg *const frm_dim_cfg,
                                 int width, int height) {
  return (double)(frm_dim_cfg->width * frm_dim_cfg->height) / (width * height);
}

// Functions to compute the active minq lookup table entries based on a
// formulaic approach to facilitate easier adjustment of the Q tables.
// The formulae were derived from computing a 3rd order polynomial best
// fit to the original data (after plotting real maxq vs minq (not q index))
static int get_minq_index(double maxq, double x3, double x2, double x1,
                          aom_bit_depth_t bit_depth) {
  const double minqtarget = AOMMIN(((x3 * maxq + x2) * maxq + x1) * maxq, maxq);

  // Special case handling to deal with the step from q2.0
  // down to lossless mode represented by q 1.0.
  if (minqtarget <= 2.0) return 0;

  return av1_find_qindex(minqtarget, bit_depth, 0, QINDEX_RANGE - 1);
}

static void init_minq_luts(int *kf_low_m, int *kf_high_m, int *arfgf_low,
                           int *arfgf_high, int *inter, int *rtc,
                           aom_bit_depth_t bit_depth) {
  int i;
  for (i = 0; i < QINDEX_RANGE; i++) {
    const double maxq = av1_convert_qindex_to_q(i, bit_depth);
    kf_low_m[i] = get_minq_index(maxq, 0.000001, -0.0004, 0.150, bit_depth);
    kf_high_m[i] = get_minq_index(maxq, 0.0000021, -0.00125, 0.45, bit_depth);
    arfgf_low[i] = get_minq_index(maxq, 0.0000015, -0.0009, 0.30, bit_depth);
    arfgf_high[i] = get_minq_index(maxq, 0.0000021, -0.00125, 0.55, bit_depth);
    inter[i] = get_minq_index(maxq, 0.00000271, -0.00113, 0.90, bit_depth);
    rtc[i] = get_minq_index(maxq, 0.00000271, -0.00113, 0.70, bit_depth);
  }
}

void av1_rc_init_minq_luts(void) {
  init_minq_luts(kf_low_motion_minq_8, kf_high_motion_minq_8,
                 arfgf_low_motion_minq_8, arfgf_high_motion_minq_8,
                 inter_minq_8, rtc_minq_8, AOM_BITS_8);
  init_minq_luts(kf_low_motion_minq_10, kf_high_motion_minq_10,
                 arfgf_low_motion_minq_10, arfgf_high_motion_minq_10,
                 inter_minq_10, rtc_minq_10, AOM_BITS_10);
  init_minq_luts(kf_low_motion_minq_12, kf_high_motion_minq_12,
                 arfgf_low_motion_minq_12, arfgf_high_motion_minq_12,
                 inter_minq_12, rtc_minq_12, AOM_BITS_12);
}

// These functions use formulaic calculations to make playing with the
// quantizer tables easier. If necessary they can be replaced by lookup
// tables if and when things settle down in the experimental bitstream
double av1_convert_qindex_to_q(int qindex, aom_bit_depth_t bit_depth) {
  // Convert the index to a real Q value (scaled down to match old Q values)
  switch (bit_depth) {
    case AOM_BITS_8: return av1_ac_quant_QTX(qindex, 0, bit_depth) / 4.0;
    case AOM_BITS_10: return av1_ac_quant_QTX(qindex, 0, bit_depth) / 16.0;
    case AOM_BITS_12: return av1_ac_quant_QTX(qindex, 0, bit_depth) / 64.0;
    default:
      assert(0 && "bit_depth should be AOM_BITS_8, AOM_BITS_10 or AOM_BITS_12");
      return -1.0;
  }
}

int av1_rc_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                       double correction_factor, aom_bit_depth_t bit_depth,
                       const int is_screen_content_type) {
  const double q = av1_convert_qindex_to_q(qindex, bit_depth);
  int enumerator = frame_type == KEY_FRAME ? 2000000 : 1500000;
  if (is_screen_content_type) {
    enumerator = frame_type == KEY_FRAME ? 1000000 : 750000;
  }

  assert(correction_factor <= MAX_BPB_FACTOR &&
         correction_factor >= MIN_BPB_FACTOR);

  // q based adjustment to baseline enumerator
  return (int)(enumerator * correction_factor / q);
}

int av1_estimate_bits_at_q(FRAME_TYPE frame_type, int q, int mbs,
                           double correction_factor, aom_bit_depth_t bit_depth,
                           const int is_screen_content_type) {
  const int bpm = (int)(av1_rc_bits_per_mb(frame_type, q, correction_factor,
                                           bit_depth, is_screen_content_type));
  return AOMMAX(FRAME_OVERHEAD_BITS,
                (int)((uint64_t)bpm * mbs) >> BPER_MB_NORMBITS);
}

int av1_rc_clamp_pframe_target_size(const AV1_COMP *const cpi, int target,
                                    FRAME_UPDATE_TYPE frame_update_type) {
  const RATE_CONTROL *rc = &cpi->rc;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  const int min_frame_target =
      AOMMAX(rc->min_frame_bandwidth, rc->avg_frame_bandwidth >> 5);
  // Clip the frame target to the minimum setup value.
  if (frame_update_type == OVERLAY_UPDATE ||
      frame_update_type == INTNL_OVERLAY_UPDATE) {
    // If there is an active ARF at this location use the minimum
    // bits on this frame even if it is a constructed arf.
    // The active maximum quantizer insures that an appropriate
    // number of bits will be spent if needed for constructed ARFs.
    target = min_frame_target;
  } else if (target < min_frame_target) {
    target = min_frame_target;
  }

  // Clip the frame target to the maximum allowed value.
  if (target > rc->max_frame_bandwidth) target = rc->max_frame_bandwidth;
  if (oxcf->rc_cfg.max_inter_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * oxcf->rc_cfg.max_inter_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }

  return target;
}

int av1_rc_clamp_iframe_target_size(const AV1_COMP *const cpi, int target) {
  const RATE_CONTROL *rc = &cpi->rc;
  const RateControlCfg *const rc_cfg = &cpi->oxcf.rc_cfg;
  if (rc_cfg->max_intra_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * rc_cfg->max_intra_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }
  if (target > rc->max_frame_bandwidth) target = rc->max_frame_bandwidth;
  return target;
}

// Update the buffer level for higher temporal layers, given the encoded current
// temporal layer.
static void update_layer_buffer_level(SVC *svc, int encoded_frame_size) {
  const int current_temporal_layer = svc->temporal_layer_id;
  for (int i = current_temporal_layer + 1; i < svc->number_temporal_layers;
       ++i) {
    const int layer =
        LAYER_IDS_TO_IDX(svc->spatial_layer_id, i, svc->number_temporal_layers);
    LAYER_CONTEXT *lc = &svc->layer_context[layer];
    RATE_CONTROL *lrc = &lc->rc;
    lrc->bits_off_target +=
        (int)(lc->target_bandwidth / lc->framerate) - encoded_frame_size;
    // Clip buffer level to maximum buffer size for the layer.
    lrc->bits_off_target =
        AOMMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
    lrc->buffer_level = lrc->bits_off_target;
  }
}
// Update the buffer level: leaky bucket model.
static void update_buffer_level(AV1_COMP *cpi, int encoded_frame_size) {
  const AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;

  // Non-viewable frames are a special case and are treated as pure overhead.
  if (!cm->show_frame)
    rc->bits_off_target -= encoded_frame_size;
  else
    rc->bits_off_target += rc->avg_frame_bandwidth - encoded_frame_size;

  // Clip the buffer level to the maximum specified buffer size.
  rc->bits_off_target = AOMMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = rc->bits_off_target;

  if (cpi->use_svc) update_layer_buffer_level(&cpi->svc, encoded_frame_size);
}

int av1_rc_get_default_min_gf_interval(int width, int height,
                                       double framerate) {
  // Assume we do not need any constraint lower than 4K 20 fps
  static const double factor_safe = 3840 * 2160 * 20.0;
  const double factor = width * height * framerate;
  const int default_interval =
      clamp((int)(framerate * 0.125), MIN_GF_INTERVAL, MAX_GF_INTERVAL);

  if (factor <= factor_safe)
    return default_interval;
  else
    return AOMMAX(default_interval,
                  (int)(MIN_GF_INTERVAL * factor / factor_safe + 0.5));
  // Note this logic makes:
  // 4K24: 5
  // 4K30: 6
  // 4K60: 12
}

int av1_rc_get_default_max_gf_interval(double framerate, int min_gf_interval) {
  int interval = AOMMIN(MAX_GF_INTERVAL, (int)(framerate * 0.75));
  interval += (interval & 0x01);  // Round to even value
  interval = AOMMAX(MAX_GF_INTERVAL, interval);
  return AOMMAX(interval, min_gf_interval);
}

void av1_rc_init(const AV1EncoderConfig *oxcf, int pass, RATE_CONTROL *rc) {
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;
  int i;

  if (pass == 0 && rc_cfg->mode == AOM_CBR) {
    rc->avg_frame_qindex[KEY_FRAME] = rc_cfg->worst_allowed_q;
    rc->avg_frame_qindex[INTER_FRAME] = rc_cfg->worst_allowed_q;
  } else {
    rc->avg_frame_qindex[KEY_FRAME] =
        (rc_cfg->worst_allowed_q + rc_cfg->best_allowed_q) / 2;
    rc->avg_frame_qindex[INTER_FRAME] =
        (rc_cfg->worst_allowed_q + rc_cfg->best_allowed_q) / 2;
  }

  rc->last_q[KEY_FRAME] = rc_cfg->best_allowed_q;
  rc->last_q[INTER_FRAME] = rc_cfg->worst_allowed_q;

  rc->buffer_level = rc->starting_buffer_level;
  rc->bits_off_target = rc->starting_buffer_level;

  rc->rolling_target_bits = rc->avg_frame_bandwidth;
  rc->rolling_actual_bits = rc->avg_frame_bandwidth;
  rc->long_rolling_target_bits = rc->avg_frame_bandwidth;
  rc->long_rolling_actual_bits = rc->avg_frame_bandwidth;

  rc->total_actual_bits = 0;
  rc->total_target_bits = 0;
  rc->total_target_vs_actual = 0;

  rc->frames_since_key = 8;  // Sensible default for first frame.
  rc->this_key_frame_forced = 0;
  rc->next_key_frame_forced = 0;

  rc->frames_till_gf_update_due = 0;
  rc->ni_av_qi = rc_cfg->worst_allowed_q;
  rc->ni_tot_qi = 0;
  rc->ni_frames = 0;

  rc->tot_q = 0.0;
  rc->avg_q = av1_convert_qindex_to_q(rc_cfg->worst_allowed_q,
                                      oxcf->tool_cfg.bit_depth);

  for (i = 0; i < RATE_FACTOR_LEVELS; ++i) {
    rc->rate_correction_factors[i] = 0.7;
  }
  rc->rate_correction_factors[KF_STD] = 1.0;
  rc->min_gf_interval = oxcf->gf_cfg.min_gf_interval;
  rc->max_gf_interval = oxcf->gf_cfg.max_gf_interval;
  if (rc->min_gf_interval == 0)
    rc->min_gf_interval = av1_rc_get_default_min_gf_interval(
        oxcf->frm_dim_cfg.width, oxcf->frm_dim_cfg.height,
        oxcf->input_cfg.init_framerate);
  if (rc->max_gf_interval == 0)
    rc->max_gf_interval = av1_rc_get_default_max_gf_interval(
        oxcf->input_cfg.init_framerate, rc->min_gf_interval);
  rc->baseline_gf_interval = (rc->min_gf_interval + rc->max_gf_interval) / 2;
  rc->avg_frame_low_motion = 0;

  rc->resize_state = ORIG;
  rc->resize_avg_qp = 0;
  rc->resize_buffer_underflow = 0;
  rc->resize_count = 0;
}

int av1_rc_drop_frame(AV1_COMP *cpi) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  RATE_CONTROL *const rc = &cpi->rc;

  if (!oxcf->rc_cfg.drop_frames_water_mark) {
    return 0;
  } else {
    if (rc->buffer_level < 0) {
      // Always drop if buffer is below 0.
      return 1;
    } else {
      // If buffer is below drop_mark, for now just drop every other frame
      // (starting with the next frame) until it increases back over drop_mark.
      int drop_mark = (int)(oxcf->rc_cfg.drop_frames_water_mark *
                            rc->optimal_buffer_level / 100);
      if ((rc->buffer_level > drop_mark) && (rc->decimation_factor > 0)) {
        --rc->decimation_factor;
      } else if (rc->buffer_level <= drop_mark && rc->decimation_factor == 0) {
        rc->decimation_factor = 1;
      }
      if (rc->decimation_factor > 0) {
        if (rc->decimation_count > 0) {
          --rc->decimation_count;
          return 1;
        } else {
          rc->decimation_count = rc->decimation_factor;
          return 0;
        }
      } else {
        rc->decimation_count = 0;
        return 0;
      }
    }
  }
}

static int adjust_q_cbr(const AV1_COMP *cpi, int q, int active_worst_quality) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1_COMMON *const cm = &cpi->common;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;
  const int max_delta = 16;
  const int change_avg_frame_bandwidth =
      abs(rc->avg_frame_bandwidth - rc->prev_avg_frame_bandwidth) >
      0.1 * (rc->avg_frame_bandwidth);
  // If resolution changes or avg_frame_bandwidth significantly changed,
  // then set this flag to indicate change in target bits per macroblock.
  const int change_target_bits_mb =
      cm->prev_frame &&
      (cm->width != cm->prev_frame->width ||
       cm->height != cm->prev_frame->height || change_avg_frame_bandwidth);
  // Apply some control/clamp to QP under certain conditions.
  if (cm->current_frame.frame_type != KEY_FRAME && !cpi->use_svc &&
      rc->frames_since_key > 1 && !change_target_bits_mb &&
      (!cpi->oxcf.rc_cfg.gf_cbr_boost_pct ||
       !(refresh_frame_flags->alt_ref_frame ||
         refresh_frame_flags->golden_frame))) {
    // Make sure q is between oscillating Qs to prevent resonance.
    if (rc->rc_1_frame * rc->rc_2_frame == -1 &&
        rc->q_1_frame != rc->q_2_frame) {
      q = clamp(q, AOMMIN(rc->q_1_frame, rc->q_2_frame),
                AOMMAX(rc->q_1_frame, rc->q_2_frame));
    }
    // Adjust Q base on source content change from scene detection.
    if (cpi->sf.rt_sf.check_scene_detection && rc->prev_avg_source_sad > 0 &&
        rc->frames_since_key > 10) {
      const int bit_depth = cm->seq_params.bit_depth;
      double delta =
          (double)rc->avg_source_sad / (double)rc->prev_avg_source_sad - 1.0;
      // Push Q downwards if content change is decreasing and buffer level
      // is stable (at least 1/4-optimal level), so not overshooting. Do so
      // only for high Q to avoid excess overshoot.
      // Else reduce decrease in Q from previous frame if content change is
      // increasing and buffer is below max (so not undershooting).
      if (delta < 0.0 && rc->buffer_level > (rc->optimal_buffer_level >> 2) &&
          q > (rc->worst_quality >> 1)) {
        double q_adj_factor = 1.0 + 0.5 * tanh(4.0 * delta);
        double q_val = av1_convert_qindex_to_q(q, bit_depth);
        q += av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);
      } else if (rc->q_1_frame - q > 0 && delta > 0.1 &&
                 rc->buffer_level < AOMMIN(rc->maximum_buffer_size,
                                           rc->optimal_buffer_level << 1)) {
        q = (3 * q + rc->q_1_frame) >> 2;
      }
    }
    // Limit the decrease in Q from previous frame.
    if (rc->q_1_frame - q > max_delta) q = rc->q_1_frame - max_delta;
  }
  // For single spatial layer: if resolution has increased push q closer
  // to the active_worst to avoid excess overshoot.
  if (cpi->svc.number_spatial_layers <= 1 && cm->prev_frame &&
      (cm->width * cm->height >
       1.5 * cm->prev_frame->width * cm->prev_frame->height))
    q = (q + active_worst_quality) >> 1;
  return AOMMAX(AOMMIN(q, cpi->rc.worst_quality), cpi->rc.best_quality);
}

static const RATE_FACTOR_LEVEL rate_factor_levels[FRAME_UPDATE_TYPES] = {
  KF_STD,        // KF_UPDATE
  INTER_NORMAL,  // LF_UPDATE
  GF_ARF_STD,    // GF_UPDATE
  GF_ARF_STD,    // ARF_UPDATE
  INTER_NORMAL,  // OVERLAY_UPDATE
  INTER_NORMAL,  // INTNL_OVERLAY_UPDATE
  GF_ARF_LOW,    // INTNL_ARF_UPDATE
};

static RATE_FACTOR_LEVEL get_rate_factor_level(const GF_GROUP *const gf_group) {
  const FRAME_UPDATE_TYPE update_type = gf_group->update_type[gf_group->index];
  assert(update_type < FRAME_UPDATE_TYPES);
  return rate_factor_levels[update_type];
}

/*!\brief Gets a rate vs Q correction factor
 *
 * This function returns the current value of a correction factor used to
 * dynamilcally adjust the relationship between Q and the expected number
 * of bits for the frame.
 *
 * \ingroup rate_control
 * \param[in]   cpi                   Top level encoder instance structure
 * \param[in]   width                 Frame width
 * \param[in]   height                Frame height
 *
 * \return Returns a correction factor for the current frame
 */
static double get_rate_correction_factor(const AV1_COMP *cpi, int width,
                                         int height) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;
  double rcf;

  if (cpi->common.current_frame.frame_type == KEY_FRAME) {
    rcf = rc->rate_correction_factors[KF_STD];
  } else if (is_stat_consumption_stage(cpi)) {
    const RATE_FACTOR_LEVEL rf_lvl = get_rate_factor_level(&cpi->gf_group);
    rcf = rc->rate_correction_factors[rf_lvl];
  } else {
    if ((refresh_frame_flags->alt_ref_frame ||
         refresh_frame_flags->golden_frame) &&
        !rc->is_src_frame_alt_ref && !cpi->use_svc &&
        (cpi->oxcf.rc_cfg.mode != AOM_CBR ||
         cpi->oxcf.rc_cfg.gf_cbr_boost_pct > 20))
      rcf = rc->rate_correction_factors[GF_ARF_STD];
    else
      rcf = rc->rate_correction_factors[INTER_NORMAL];
  }
  rcf *= resize_rate_factor(&cpi->oxcf.frm_dim_cfg, width, height);
  return fclamp(rcf, MIN_BPB_FACTOR, MAX_BPB_FACTOR);
}

/*!\brief Sets a rate vs Q correction factor
 *
 * This function updates the current value of a correction factor used to
 * dynamilcally adjust the relationship between Q and the expected number
 * of bits for the frame.
 *
 * \ingroup rate_control
 * \param[in]   cpi                   Top level encoder instance structure
 * \param[in]   factor                New correction factor
 * \param[in]   width                 Frame width
 * \param[in]   height                Frame height
 *
 * \return None but updates the rate correction factor for the
 *         current frame type in cpi->rc.
 */
static void set_rate_correction_factor(AV1_COMP *cpi, double factor, int width,
                                       int height) {
  RATE_CONTROL *const rc = &cpi->rc;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;

  // Normalize RCF to account for the size-dependent scaling factor.
  factor /= resize_rate_factor(&cpi->oxcf.frm_dim_cfg, width, height);

  factor = fclamp(factor, MIN_BPB_FACTOR, MAX_BPB_FACTOR);

  if (cpi->common.current_frame.frame_type == KEY_FRAME) {
    rc->rate_correction_factors[KF_STD] = factor;
  } else if (is_stat_consumption_stage(cpi)) {
    const RATE_FACTOR_LEVEL rf_lvl = get_rate_factor_level(&cpi->gf_group);
    rc->rate_correction_factors[rf_lvl] = factor;
  } else {
    if ((refresh_frame_flags->alt_ref_frame ||
         refresh_frame_flags->golden_frame) &&
        !rc->is_src_frame_alt_ref && !cpi->use_svc &&
        (cpi->oxcf.rc_cfg.mode != AOM_CBR ||
         cpi->oxcf.rc_cfg.gf_cbr_boost_pct > 20))
      rc->rate_correction_factors[GF_ARF_STD] = factor;
    else
      rc->rate_correction_factors[INTER_NORMAL] = factor;
  }
}

void av1_rc_update_rate_correction_factors(AV1_COMP *cpi, int width,
                                           int height) {
  const AV1_COMMON *const cm = &cpi->common;
  int correction_factor = 100;
  double rate_correction_factor =
      get_rate_correction_factor(cpi, width, height);
  double adjustment_limit;
  const int MBs = av1_get_MBs(width, height);

  int projected_size_based_on_q = 0;

  // Do not update the rate factors for arf overlay frames.
  if (cpi->rc.is_src_frame_alt_ref) return;

  // Clear down mmx registers to allow floating point in what follows
  aom_clear_system_state();

  // Work out how big we would have expected the frame to be at this Q given
  // the current correction factor.
  // Stay in double to avoid int overflow when values are large
  if (cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ && cpi->common.seg.enabled) {
    projected_size_based_on_q =
        av1_cyclic_refresh_estimate_bits_at_q(cpi, rate_correction_factor);
  } else {
    projected_size_based_on_q = av1_estimate_bits_at_q(
        cm->current_frame.frame_type, cm->quant_params.base_qindex, MBs,
        rate_correction_factor, cm->seq_params.bit_depth,
        cpi->is_screen_content_type);
  }
  // Work out a size correction factor.
  if (projected_size_based_on_q > FRAME_OVERHEAD_BITS)
    correction_factor = (int)((100 * (int64_t)cpi->rc.projected_frame_size) /
                              projected_size_based_on_q);

  // More heavily damped adjustment used if we have been oscillating either side
  // of target.
  if (correction_factor > 0) {
    adjustment_limit =
        0.25 + 0.5 * AOMMIN(1, fabs(log10(0.01 * correction_factor)));
  } else {
    adjustment_limit = 0.75;
  }

  cpi->rc.q_2_frame = cpi->rc.q_1_frame;
  cpi->rc.q_1_frame = cm->quant_params.base_qindex;
  cpi->rc.rc_2_frame = cpi->rc.rc_1_frame;
  if (correction_factor > 110)
    cpi->rc.rc_1_frame = -1;
  else if (correction_factor < 90)
    cpi->rc.rc_1_frame = 1;
  else
    cpi->rc.rc_1_frame = 0;

  if (correction_factor > 102) {
    // We are not already at the worst allowable quality
    correction_factor =
        (int)(100 + ((correction_factor - 100) * adjustment_limit));
    rate_correction_factor = (rate_correction_factor * correction_factor) / 100;
    // Keep rate_correction_factor within limits
    if (rate_correction_factor > MAX_BPB_FACTOR)
      rate_correction_factor = MAX_BPB_FACTOR;
  } else if (correction_factor < 99) {
    // We are not already at the best allowable quality
    correction_factor =
        (int)(100 - ((100 - correction_factor) * adjustment_limit));
    rate_correction_factor = (rate_correction_factor * correction_factor) / 100;

    // Keep rate_correction_factor within limits
    if (rate_correction_factor < MIN_BPB_FACTOR)
      rate_correction_factor = MIN_BPB_FACTOR;
  }

  set_rate_correction_factor(cpi, rate_correction_factor, width, height);
}

// Calculate rate for the given 'q'.
static int get_bits_per_mb(const AV1_COMP *cpi, int use_cyclic_refresh,
                           double correction_factor, int q) {
  const AV1_COMMON *const cm = &cpi->common;
  return use_cyclic_refresh
             ? av1_cyclic_refresh_rc_bits_per_mb(cpi, q, correction_factor)
             : av1_rc_bits_per_mb(cm->current_frame.frame_type, q,
                                  correction_factor, cm->seq_params.bit_depth,
                                  cpi->is_screen_content_type);
}

/*!\brief Searches for a Q index value predicted to give an average macro
 * block rate closest to the target value.
 *
 * Similar to find_qindex_by_rate() function, but returns a q index with a
 * rate just above or below the desired rate, depending on which of the two
 * rates is closer to the desired rate.
 * Also, respects the selected aq_mode when computing the rate.
 *
 * \ingroup rate_control
 * \param[in]   desired_bits_per_mb   Target bits per mb
 * \param[in]   cpi                   Top level encoder instance structure
 * \param[in]   correction_factor     Current Q to rate correction factor
 * \param[in]   best_qindex           Min allowed Q value.
 * \param[in]   worst_qindex          Max allowed Q value.
 *
 * \return Returns a correction factor for the current frame
 */
static int find_closest_qindex_by_rate(int desired_bits_per_mb,
                                       const AV1_COMP *cpi,
                                       double correction_factor,
                                       int best_qindex, int worst_qindex) {
  const int use_cyclic_refresh = cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ &&
                                 cpi->cyclic_refresh->apply_cyclic_refresh;

  // Find 'qindex' based on 'desired_bits_per_mb'.
  assert(best_qindex <= worst_qindex);
  int low = best_qindex;
  int high = worst_qindex;
  while (low < high) {
    const int mid = (low + high) >> 1;
    const int mid_bits_per_mb =
        get_bits_per_mb(cpi, use_cyclic_refresh, correction_factor, mid);
    if (mid_bits_per_mb > desired_bits_per_mb) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  assert(low == high);

  // Calculate rate difference of this q index from the desired rate.
  const int curr_q = low;
  const int curr_bits_per_mb =
      get_bits_per_mb(cpi, use_cyclic_refresh, correction_factor, curr_q);
  const int curr_bit_diff = (curr_bits_per_mb <= desired_bits_per_mb)
                                ? desired_bits_per_mb - curr_bits_per_mb
                                : INT_MAX;
  assert((curr_bit_diff != INT_MAX && curr_bit_diff >= 0) ||
         curr_q == worst_qindex);

  // Calculate rate difference for previous q index too.
  const int prev_q = curr_q - 1;
  int prev_bit_diff;
  if (curr_bit_diff == INT_MAX || curr_q == best_qindex) {
    prev_bit_diff = INT_MAX;
  } else {
    const int prev_bits_per_mb =
        get_bits_per_mb(cpi, use_cyclic_refresh, correction_factor, prev_q);
    assert(prev_bits_per_mb > desired_bits_per_mb);
    prev_bit_diff = prev_bits_per_mb - desired_bits_per_mb;
  }

  // Pick one of the two q indices, depending on which one has rate closer to
  // the desired rate.
  return (curr_bit_diff <= prev_bit_diff) ? curr_q : prev_q;
}

int av1_rc_regulate_q(const AV1_COMP *cpi, int target_bits_per_frame,
                      int active_best_quality, int active_worst_quality,
                      int width, int height) {
  const int MBs = av1_get_MBs(width, height);
  const double correction_factor =
      get_rate_correction_factor(cpi, width, height);
  const int target_bits_per_mb =
      (int)(((uint64_t)target_bits_per_frame << BPER_MB_NORMBITS) / MBs);

  int q =
      find_closest_qindex_by_rate(target_bits_per_mb, cpi, correction_factor,
                                  active_best_quality, active_worst_quality);
  if (cpi->oxcf.rc_cfg.mode == AOM_CBR && has_no_stats_stage(cpi))
    return adjust_q_cbr(cpi, q, active_worst_quality);

  return q;
}

static int get_active_quality(int q, int gfu_boost, int low, int high,
                              int *low_motion_minq, int *high_motion_minq) {
  if (gfu_boost > high) {
    return low_motion_minq[q];
  } else if (gfu_boost < low) {
    return high_motion_minq[q];
  } else {
    const int gap = high - low;
    const int offset = high - gfu_boost;
    const int qdiff = high_motion_minq[q] - low_motion_minq[q];
    const int adjustment = ((offset * qdiff) + (gap >> 1)) / gap;
    return low_motion_minq[q] + adjustment;
  }
}

static int get_kf_active_quality(const RATE_CONTROL *const rc, int q,
                                 aom_bit_depth_t bit_depth) {
  int *kf_low_motion_minq;
  int *kf_high_motion_minq;
  ASSIGN_MINQ_TABLE(bit_depth, kf_low_motion_minq);
  ASSIGN_MINQ_TABLE(bit_depth, kf_high_motion_minq);
  return get_active_quality(q, rc->kf_boost, kf_low, kf_high,
                            kf_low_motion_minq, kf_high_motion_minq);
}

static int get_gf_active_quality(const RATE_CONTROL *const rc, int q,
                                 aom_bit_depth_t bit_depth) {
  int *arfgf_low_motion_minq;
  int *arfgf_high_motion_minq;
  ASSIGN_MINQ_TABLE(bit_depth, arfgf_low_motion_minq);
  ASSIGN_MINQ_TABLE(bit_depth, arfgf_high_motion_minq);
  return get_active_quality(q, rc->gfu_boost, gf_low, gf_high,
                            arfgf_low_motion_minq, arfgf_high_motion_minq);
}

static int get_gf_high_motion_quality(int q, aom_bit_depth_t bit_depth) {
  int *arfgf_high_motion_minq;
  ASSIGN_MINQ_TABLE(bit_depth, arfgf_high_motion_minq);
  return arfgf_high_motion_minq[q];
}

static int calc_active_worst_quality_no_stats_vbr(const AV1_COMP *cpi) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;
  const unsigned int curr_frame = cpi->common.current_frame.frame_number;
  int active_worst_quality;

  if (cpi->common.current_frame.frame_type == KEY_FRAME) {
    active_worst_quality =
        curr_frame == 0 ? rc->worst_quality : rc->last_q[KEY_FRAME] * 2;
  } else {
    if (!rc->is_src_frame_alt_ref && (refresh_frame_flags->golden_frame ||
                                      refresh_frame_flags->bwd_ref_frame ||
                                      refresh_frame_flags->alt_ref_frame)) {
      active_worst_quality = curr_frame == 1 ? rc->last_q[KEY_FRAME] * 5 / 4
                                             : rc->last_q[INTER_FRAME];
    } else {
      active_worst_quality = curr_frame == 1 ? rc->last_q[KEY_FRAME] * 2
                                             : rc->last_q[INTER_FRAME] * 2;
    }
  }
  return AOMMIN(active_worst_quality, rc->worst_quality);
}

// Adjust active_worst_quality level based on buffer level.
static int calc_active_worst_quality_no_stats_cbr(const AV1_COMP *cpi) {
  // Adjust active_worst_quality: If buffer is above the optimal/target level,
  // bring active_worst_quality down depending on fullness of buffer.
  // If buffer is below the optimal level, let the active_worst_quality go from
  // ambient Q (at buffer = optimal level) to worst_quality level
  // (at buffer = critical level).
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *rc = &cpi->rc;
  // Buffer level below which we push active_worst to worst_quality.
  int64_t critical_level = rc->optimal_buffer_level >> 3;
  int64_t buff_lvl_step = 0;
  int adjustment = 0;
  int active_worst_quality;
  int ambient_qp;
  if (cm->current_frame.frame_type == KEY_FRAME) return rc->worst_quality;
  // For ambient_qp we use minimum of avg_frame_qindex[KEY_FRAME/INTER_FRAME]
  // for the first few frames following key frame. These are both initialized
  // to worst_quality and updated with (3/4, 1/4) average in postencode_update.
  // So for first few frames following key, the qp of that key frame is weighted
  // into the active_worst_quality setting.
  ambient_qp = (cm->current_frame.frame_number < 5)
                   ? AOMMIN(rc->avg_frame_qindex[INTER_FRAME],
                            rc->avg_frame_qindex[KEY_FRAME])
                   : rc->avg_frame_qindex[INTER_FRAME];
  active_worst_quality = AOMMIN(rc->worst_quality, ambient_qp * 5 / 4);
  if (rc->buffer_level > rc->optimal_buffer_level) {
    // Adjust down.
    // Maximum limit for down adjustment, ~30%.
    int max_adjustment_down = active_worst_quality / 3;
    if (max_adjustment_down) {
      buff_lvl_step = ((rc->maximum_buffer_size - rc->optimal_buffer_level) /
                       max_adjustment_down);
      if (buff_lvl_step)
        adjustment = (int)((rc->buffer_level - rc->optimal_buffer_level) /
                           buff_lvl_step);
      active_worst_quality -= adjustment;
    }
  } else if (rc->buffer_level > critical_level) {
    // Adjust up from ambient Q.
    if (critical_level) {
      buff_lvl_step = (rc->optimal_buffer_level - critical_level);
      if (buff_lvl_step) {
        adjustment = (int)((rc->worst_quality - ambient_qp) *
                           (rc->optimal_buffer_level - rc->buffer_level) /
                           buff_lvl_step);
      }
      active_worst_quality = ambient_qp + adjustment;
    }
  } else {
    // Set to worst_quality if buffer is below critical level.
    active_worst_quality = rc->worst_quality;
  }
  return active_worst_quality;
}

// Calculate the active_best_quality level.
static int calc_active_best_quality_no_stats_cbr(const AV1_COMP *cpi,
                                                 int active_worst_quality,
                                                 int width, int height) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;
  const CurrentFrame *const current_frame = &cm->current_frame;
  int *rtc_minq;
  const int bit_depth = cm->seq_params.bit_depth;
  int active_best_quality = rc->best_quality;
  ASSIGN_MINQ_TABLE(bit_depth, rtc_minq);

  if (frame_is_intra_only(cm)) {
    // Handle the special case for key frames forced when we have reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (rc->this_key_frame_forced) {
      int qindex = rc->last_boosted_qindex;
      double last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
      int delta_qindex = av1_compute_qdelta(rc, last_boosted_q,
                                            (last_boosted_q * 0.75), bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else if (current_frame->frame_number > 0) {
      // not first frame of one pass and kf_boost is set
      double q_adj_factor = 1.0;
      double q_val;
      active_best_quality =
          get_kf_active_quality(rc, rc->avg_frame_qindex[KEY_FRAME], bit_depth);
      // Allow somewhat lower kf minq with small image formats.
      if ((width * height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }
      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = av1_convert_qindex_to_q(active_best_quality, bit_depth);
      active_best_quality +=
          av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);
    }
  } else if (!rc->is_src_frame_alt_ref && !cpi->use_svc &&
             cpi->oxcf.rc_cfg.gf_cbr_boost_pct &&
             (refresh_frame_flags->golden_frame ||
              refresh_frame_flags->alt_ref_frame)) {
    // Use the lower of active_worst_quality and recent
    // average Q as basis for GF/ARF best Q limit unless last frame was
    // a key frame.
    int q = active_worst_quality;
    if (rc->frames_since_key > 1 &&
        rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality) {
      q = rc->avg_frame_qindex[INTER_FRAME];
    }
    active_best_quality = get_gf_active_quality(rc, q, bit_depth);
  } else {
    // Use the lower of active_worst_quality and recent/average Q.
    FRAME_TYPE frame_type =
        (current_frame->frame_number > 1) ? INTER_FRAME : KEY_FRAME;
    if (rc->avg_frame_qindex[frame_type] < active_worst_quality)
      active_best_quality = rtc_minq[rc->avg_frame_qindex[frame_type]];
    else
      active_best_quality = rtc_minq[active_worst_quality];
  }
  return active_best_quality;
}

/*!\brief Picks q and q bounds given CBR rate control parameters in \c cpi->rc.
 *
 * Handles the special case when using:
 * - Constant bit-rate mode: \c cpi->oxcf.rc_cfg.mode == \ref AOM_CBR, and
 * - 1-pass encoding without LAP (look-ahead processing), so 1st pass stats are
 * NOT available.
 *
 * \ingroup rate_control
 * \param[in]       cpi          Top level encoder structure
 * \param[in]       width        Coded frame width
 * \param[in]       height       Coded frame height
 * \param[out]      bottom_index Bottom bound for q index (best quality)
 * \param[out]      top_index    Top bound for q index (worst quality)
 * \return Returns selected q index to be used for encoding this frame.
 */
static int rc_pick_q_and_bounds_no_stats_cbr(const AV1_COMP *cpi, int width,
                                             int height, int *bottom_index,
                                             int *top_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const CurrentFrame *const current_frame = &cm->current_frame;
  int q;
  const int bit_depth = cm->seq_params.bit_depth;
  int active_worst_quality = calc_active_worst_quality_no_stats_cbr(cpi);
  int active_best_quality = calc_active_best_quality_no_stats_cbr(
      cpi, active_worst_quality, width, height);
  assert(has_no_stats_stage(cpi));
  assert(cpi->oxcf.rc_cfg.mode == AOM_CBR);

  // Clip the active best and worst quality values to limits
  active_best_quality =
      clamp(active_best_quality, rc->best_quality, rc->worst_quality);
  active_worst_quality =
      clamp(active_worst_quality, active_best_quality, rc->worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

  // Limit Q range for the adaptive loop.
  if (current_frame->frame_type == KEY_FRAME && !rc->this_key_frame_forced &&
      current_frame->frame_number != 0) {
    int qdelta = 0;
    aom_clear_system_state();
    qdelta = av1_compute_qdelta_by_rate(&cpi->rc, current_frame->frame_type,
                                        active_worst_quality, 2.0,
                                        cpi->is_screen_content_type, bit_depth);
    *top_index = active_worst_quality + qdelta;
    *top_index = AOMMAX(*top_index, *bottom_index);
  }

  // Special case code to try and match quality with forced key frames
  if (current_frame->frame_type == KEY_FRAME && rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = av1_rc_regulate_q(cpi, rc->this_frame_target, active_best_quality,
                          active_worst_quality, width, height);
    if (q > *top_index) {
      // Special case when we are targeting the max allowed rate
      if (rc->this_frame_target >= rc->max_frame_bandwidth)
        *top_index = q;
      else
        q = *top_index;
    }
  }

  assert(*top_index <= rc->worst_quality && *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

static int gf_group_pyramid_level(const GF_GROUP *gf_group, int gf_index) {
  return gf_group->layer_depth[gf_index];
}

static int get_active_cq_level(const RATE_CONTROL *rc,
                               const AV1EncoderConfig *const oxcf,
                               int intra_only, aom_superres_mode superres_mode,
                               int superres_denom) {
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;
  static const double cq_adjust_threshold = 0.1;
  int active_cq_level = rc_cfg->cq_level;
  (void)intra_only;
  if (rc_cfg->mode == AOM_CQ || rc_cfg->mode == AOM_Q) {
    // printf("Superres %d %d %d = %d\n", superres_denom, intra_only,
    //        rc->frames_to_key, !(intra_only && rc->frames_to_key <= 1));
    if ((superres_mode == AOM_SUPERRES_QTHRESH ||
         superres_mode == AOM_SUPERRES_AUTO) &&
        superres_denom != SCALE_NUMERATOR) {
      int mult = SUPERRES_QADJ_PER_DENOM_KEYFRAME_SOLO;
      if (intra_only && rc->frames_to_key <= 1) {
        mult = 0;
      } else if (intra_only) {
        mult = SUPERRES_QADJ_PER_DENOM_KEYFRAME;
      } else {
        mult = SUPERRES_QADJ_PER_DENOM_ARFFRAME;
      }
      active_cq_level = AOMMAX(
          active_cq_level - ((superres_denom - SCALE_NUMERATOR) * mult), 0);
    }
  }
  if (rc_cfg->mode == AOM_CQ && rc->total_target_bits > 0) {
    const double x = (double)rc->total_actual_bits / rc->total_target_bits;
    if (x < cq_adjust_threshold) {
      active_cq_level = (int)(active_cq_level * x / cq_adjust_threshold);
    }
  }
  return active_cq_level;
}

/*! \brief Pick q index for this frame using fixed q index offsets.
 *
 * The q index offsets are fixed in the sense that they are independent of the
 * video content. The offsets for each pyramid level are taken from
 * \c oxcf->q_cfg.fixed_qp_offsets array.
 *
 * \ingroup rate_control
 * \param[in]   oxcf        Top level encoder configuration
 * \param[in]   rc          Top level rate control structure
 * \param[in]   gf_group    Configuration of current golden frame group
 * \param[in]   gf_index    Index of this frame in the golden frame group
 * \param[in]   cq_level    Upper bound for q index (this may be same as
 *                          \c oxcf->cq_level, or slightly modified for some
 *                          special cases)
 * \param[in]   bit_depth   Bit depth of the codec (same as
 *                          \c cm->seq_params.bit_depth)
 * \return Returns selected q index to be used for encoding this frame.
 */
static int get_q_using_fixed_offsets(const AV1EncoderConfig *const oxcf,
                                     const RATE_CONTROL *const rc,
                                     const GF_GROUP *const gf_group,
                                     int gf_index, int cq_level,
                                     int bit_depth) {
  assert(oxcf->q_cfg.use_fixed_qp_offsets);
  assert(oxcf->rc_cfg.mode == AOM_Q);
  const FRAME_UPDATE_TYPE update_type = gf_group->update_type[gf_index];

  int offset_idx = -1;
  if (update_type == KF_UPDATE) {
    if (rc->frames_to_key <= 1) {
      // Image / intra-only coding: ignore offsets.
      return cq_level;
    }
    offset_idx = 0;
  } else if (update_type == ARF_UPDATE || update_type == GF_UPDATE) {
    offset_idx = 1;
  } else if (update_type == INTNL_ARF_UPDATE) {
    offset_idx =
        AOMMIN(gf_group->layer_depth[gf_index], FIXED_QP_OFFSET_COUNT - 1);
  } else {  // Leaf level / overlay frame.
    assert(update_type == LF_UPDATE || update_type == OVERLAY_UPDATE ||
           update_type == INTNL_OVERLAY_UPDATE);
    return cq_level;  // Directly Return worst quality allowed.
  }
  assert(offset_idx >= 0 && offset_idx < FIXED_QP_OFFSET_COUNT);
  assert(oxcf->q_cfg.fixed_qp_offsets[offset_idx] >= 0);

  // Get qindex offset, by first converting to 'q' and then back.
  const double q_val_orig = av1_convert_qindex_to_q(cq_level, bit_depth);
  const double q_val_target =
      AOMMAX(q_val_orig - oxcf->q_cfg.fixed_qp_offsets[offset_idx], 0.0);
  const int delta_qindex =
      av1_compute_qdelta(rc, q_val_orig, q_val_target, bit_depth);
  return AOMMAX(cq_level + delta_qindex, 0);
}

/*!\brief Picks q and q bounds given non-CBR rate control params in \c cpi->rc.
 *
 * Handles the special case when using:
 * - Any rate control other than constant bit-rate mode:
 * \c cpi->oxcf.rc_cfg.mode != \ref AOM_CBR, and
 * - 1-pass encoding without LAP (look-ahead processing), so 1st pass stats are
 * NOT available.
 *
 * \ingroup rate_control
 * \param[in]       cpi          Top level encoder structure
 * \param[in]       width        Coded frame width
 * \param[in]       height       Coded frame height
 * \param[in]       gf_index     Index of this frame in the golden frame group
 * \param[out]      bottom_index Bottom bound for q index (best quality)
 * \param[out]      top_index    Top bound for q index (worst quality)
 * \return Returns selected q index to be used for encoding this frame.
 */
static int rc_pick_q_and_bounds_no_stats(const AV1_COMP *cpi, int width,
                                         int height, int gf_index,
                                         int *bottom_index, int *top_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const CurrentFrame *const current_frame = &cm->current_frame;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const enum aom_rc_mode rc_mode = oxcf->rc_cfg.mode;

  assert(has_no_stats_stage(cpi));
  assert(rc_mode == AOM_VBR ||
         (!USE_UNRESTRICTED_Q_IN_CQ_MODE && rc_mode == AOM_CQ) ||
         rc_mode == AOM_Q);
  assert(
      IMPLIES(rc_mode == AOM_Q, gf_group->update_type[gf_index] == ARF_UPDATE));

  const int cq_level =
      get_active_cq_level(rc, oxcf, frame_is_intra_only(cm), cpi->superres_mode,
                          cm->superres_scale_denominator);
  const int bit_depth = cm->seq_params.bit_depth;

  if (oxcf->q_cfg.use_fixed_qp_offsets) {
    return get_q_using_fixed_offsets(oxcf, rc, gf_group, gf_index, cq_level,
                                     bit_depth);
  }

  int active_best_quality;
  int active_worst_quality = calc_active_worst_quality_no_stats_vbr(cpi);
  int q;
  int *inter_minq;
  ASSIGN_MINQ_TABLE(bit_depth, inter_minq);

  if (frame_is_intra_only(cm)) {
    if (rc_mode == AOM_Q) {
      const int qindex = cq_level;
      const double q_val = av1_convert_qindex_to_q(qindex, bit_depth);
      const int delta_qindex =
          av1_compute_qdelta(rc, q_val, q_val * 0.25, bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else if (rc->this_key_frame_forced) {
      const int qindex = rc->last_boosted_qindex;
      const double last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
      const int delta_qindex = av1_compute_qdelta(
          rc, last_boosted_q, last_boosted_q * 0.75, bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else {  // not first frame of one pass and kf_boost is set
      double q_adj_factor = 1.0;

      active_best_quality =
          get_kf_active_quality(rc, rc->avg_frame_qindex[KEY_FRAME], bit_depth);

      // Allow somewhat lower kf minq with small image formats.
      if ((width * height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Convert the adjustment factor to a qindex delta on active_best_quality.
      {
        const double q_val =
            av1_convert_qindex_to_q(active_best_quality, bit_depth);
        active_best_quality +=
            av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);
      }
    }
  } else if (!rc->is_src_frame_alt_ref &&
             (refresh_frame_flags->golden_frame ||
              refresh_frame_flags->alt_ref_frame)) {
    // Use the lower of active_worst_quality and recent
    // average Q as basis for GF/ARF best Q limit unless last frame was
    // a key frame.
    q = (rc->frames_since_key > 1 &&
         rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality)
            ? rc->avg_frame_qindex[INTER_FRAME]
            : rc->avg_frame_qindex[KEY_FRAME];
    // For constrained quality dont allow Q less than the cq level
    if (rc_mode == AOM_CQ) {
      if (q < cq_level) q = cq_level;
      active_best_quality = get_gf_active_quality(rc, q, bit_depth);
      // Constrained quality use slightly lower active best.
      active_best_quality = active_best_quality * 15 / 16;
    } else if (rc_mode == AOM_Q) {
      const int qindex = cq_level;
      const double q_val = av1_convert_qindex_to_q(qindex, bit_depth);
      const int delta_qindex =
          (refresh_frame_flags->alt_ref_frame)
              ? av1_compute_qdelta(rc, q_val, q_val * 0.40, bit_depth)
              : av1_compute_qdelta(rc, q_val, q_val * 0.50, bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else {
      active_best_quality = get_gf_active_quality(rc, q, bit_depth);
    }
  } else {
    if (rc_mode == AOM_Q) {
      const int qindex = cq_level;
      const double q_val = av1_convert_qindex_to_q(qindex, bit_depth);
      const double delta_rate[FIXED_GF_INTERVAL] = { 0.50, 1.0, 0.85, 1.0,
                                                     0.70, 1.0, 0.85, 1.0 };
      const int delta_qindex = av1_compute_qdelta(
          rc, q_val,
          q_val * delta_rate[current_frame->frame_number % FIXED_GF_INTERVAL],
          bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    } else {
      // Use the lower of active_worst_quality and recent/average Q.
      active_best_quality = (current_frame->frame_number > 1)
                                ? inter_minq[rc->avg_frame_qindex[INTER_FRAME]]
                                : inter_minq[rc->avg_frame_qindex[KEY_FRAME]];
      // For the constrained quality mode we don't want
      // q to fall below the cq level.
      if ((rc_mode == AOM_CQ) && (active_best_quality < cq_level)) {
        active_best_quality = cq_level;
      }
    }
  }

  // Clip the active best and worst quality values to limits
  active_best_quality =
      clamp(active_best_quality, rc->best_quality, rc->worst_quality);
  active_worst_quality =
      clamp(active_worst_quality, active_best_quality, rc->worst_quality);

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

  // Limit Q range for the adaptive loop.
  {
    int qdelta = 0;
    aom_clear_system_state();
    if (current_frame->frame_type == KEY_FRAME && !rc->this_key_frame_forced &&
        current_frame->frame_number != 0) {
      qdelta = av1_compute_qdelta_by_rate(
          &cpi->rc, current_frame->frame_type, active_worst_quality, 2.0,
          cpi->is_screen_content_type, bit_depth);
    } else if (!rc->is_src_frame_alt_ref &&
               (refresh_frame_flags->golden_frame ||
                refresh_frame_flags->alt_ref_frame)) {
      qdelta = av1_compute_qdelta_by_rate(
          &cpi->rc, current_frame->frame_type, active_worst_quality, 1.75,
          cpi->is_screen_content_type, bit_depth);
    }
    *top_index = active_worst_quality + qdelta;
    *top_index = AOMMAX(*top_index, *bottom_index);
  }

  if (rc_mode == AOM_Q) {
    q = active_best_quality;
    // Special case code to try and match quality with forced key frames
  } else if ((current_frame->frame_type == KEY_FRAME) &&
             rc->this_key_frame_forced) {
    q = rc->last_boosted_qindex;
  } else {
    q = av1_rc_regulate_q(cpi, rc->this_frame_target, active_best_quality,
                          active_worst_quality, width, height);
    if (q > *top_index) {
      // Special case when we are targeting the max allowed rate
      if (rc->this_frame_target >= rc->max_frame_bandwidth)
        *top_index = q;
      else
        q = *top_index;
    }
  }

  assert(*top_index <= rc->worst_quality && *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);
  return q;
}

static const double arf_layer_deltas[MAX_ARF_LAYERS + 1] = { 2.50, 2.00, 1.75,
                                                             1.50, 1.25, 1.15,
                                                             1.0 };
int av1_frame_type_qdelta(const AV1_COMP *cpi, int q) {
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const RATE_FACTOR_LEVEL rf_lvl = get_rate_factor_level(gf_group);
  const FRAME_TYPE frame_type = gf_group->frame_type[gf_group->index];
  const int arf_layer = AOMMIN(gf_group->layer_depth[gf_group->index], 6);
  const double rate_factor =
      (rf_lvl == INTER_NORMAL) ? 1.0 : arf_layer_deltas[arf_layer];

  return av1_compute_qdelta_by_rate(&cpi->rc, frame_type, q, rate_factor,
                                    cpi->is_screen_content_type,
                                    cpi->common.seq_params.bit_depth);
}

// This unrestricted Q selection on CQ mode is useful when testing new features,
// but may lead to Q being out of range on current RC restrictions
#if USE_UNRESTRICTED_Q_IN_CQ_MODE
static int rc_pick_q_and_bounds_no_stats_cq(const AV1_COMP *cpi, int width,
                                            int height, int *bottom_index,
                                            int *top_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const int cq_level =
      get_active_cq_level(rc, oxcf, frame_is_intra_only(cm), cpi->superres_mode,
                          cm->superres_scale_denominator);
  const int bit_depth = cm->seq_params.bit_depth;
  const int q = (int)av1_convert_qindex_to_q(cq_level, bit_depth);
  (void)width;
  (void)height;
  assert(has_no_stats_stage(cpi));
  assert(cpi->oxcf.rc_cfg.mode == AOM_CQ);

  *top_index = q;
  *bottom_index = q;

  return q;
}
#endif  // USE_UNRESTRICTED_Q_IN_CQ_MODE

#define STATIC_MOTION_THRESH 95
static void get_intra_q_and_bounds(const AV1_COMP *cpi, int width, int height,
                                   int *active_best, int *active_worst,
                                   int cq_level, int is_fwd_kf) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  int active_best_quality;
  int active_worst_quality = *active_worst;
  const int bit_depth = cm->seq_params.bit_depth;

  if (rc->frames_to_key <= 1 && oxcf->rc_cfg.mode == AOM_Q) {
    // If the next frame is also a key frame or the current frame is the
    // only frame in the sequence in AOM_Q mode, just use the cq_level
    // as q.
    active_best_quality = cq_level;
    active_worst_quality = cq_level;
  } else if (is_fwd_kf) {
    // Handle the special case for forward reference key frames.
    // Increase the boost because this keyframe is used as a forward and
    // backward reference.
    const int qindex = rc->last_boosted_qindex;
    const double last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
    const int delta_qindex = av1_compute_qdelta(
        rc, last_boosted_q, last_boosted_q * 0.25, bit_depth);
    active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
  } else if (rc->this_key_frame_forced) {
    // Handle the special case for key frames forced when we have reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    double last_boosted_q;
    int delta_qindex;
    int qindex;

    if (is_stat_consumption_stage_twopass(cpi) &&
        cpi->twopass.last_kfgroup_zeromotion_pct >= STATIC_MOTION_THRESH) {
      qindex = AOMMIN(rc->last_kf_qindex, rc->last_boosted_qindex);
      active_best_quality = qindex;
      last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
      delta_qindex = av1_compute_qdelta(rc, last_boosted_q,
                                        last_boosted_q * 1.25, bit_depth);
      active_worst_quality =
          AOMMIN(qindex + delta_qindex, active_worst_quality);
    } else {
      qindex = rc->last_boosted_qindex;
      last_boosted_q = av1_convert_qindex_to_q(qindex, bit_depth);
      delta_qindex = av1_compute_qdelta(rc, last_boosted_q,
                                        last_boosted_q * 0.50, bit_depth);
      active_best_quality = AOMMAX(qindex + delta_qindex, rc->best_quality);
    }
  } else {
    // Not forced keyframe.
    double q_adj_factor = 1.0;
    double q_val;

    // Baseline value derived from cpi->active_worst_quality and kf boost.
    active_best_quality =
        get_kf_active_quality(rc, active_worst_quality, bit_depth);
    if (cpi->is_screen_content_type) {
      active_best_quality /= 2;
    }

    if (is_stat_consumption_stage_twopass(cpi) &&
        cpi->twopass.kf_zeromotion_pct >= STATIC_KF_GROUP_THRESH) {
      active_best_quality /= 3;
    }

    // Allow somewhat lower kf minq with small image formats.
    if ((width * height) <= (352 * 288)) {
      q_adj_factor -= 0.25;
    }

    // Make a further adjustment based on the kf zero motion measure.
    if (is_stat_consumption_stage_twopass(cpi))
      q_adj_factor += 0.05 - (0.001 * (double)cpi->twopass.kf_zeromotion_pct);

    // Convert the adjustment factor to a qindex delta
    // on active_best_quality.
    q_val = av1_convert_qindex_to_q(active_best_quality, bit_depth);
    active_best_quality +=
        av1_compute_qdelta(rc, q_val, q_val * q_adj_factor, bit_depth);

    // Tweak active_best_quality for AOM_Q mode when superres is on, as this
    // will be used directly as 'q' later.
    if (oxcf->rc_cfg.mode == AOM_Q &&
        (cpi->superres_mode == AOM_SUPERRES_QTHRESH ||
         cpi->superres_mode == AOM_SUPERRES_AUTO) &&
        cm->superres_scale_denominator != SCALE_NUMERATOR) {
      active_best_quality =
          AOMMAX(active_best_quality -
                     ((cm->superres_scale_denominator - SCALE_NUMERATOR) *
                      SUPERRES_QADJ_PER_DENOM_KEYFRAME),
                 0);
    }
  }
  *active_best = active_best_quality;
  *active_worst = active_worst_quality;
}

static void adjust_active_best_and_worst_quality(const AV1_COMP *cpi,
                                                 const int is_intrl_arf_boost,
                                                 int *active_worst,
                                                 int *active_best) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;
  const int bit_depth = cpi->common.seq_params.bit_depth;
  int active_best_quality = *active_best;
  int active_worst_quality = *active_worst;
  // Extension to max or min Q if undershoot or overshoot is outside
  // the permitted range.
  if (cpi->oxcf.rc_cfg.mode != AOM_Q) {
    if (frame_is_intra_only(cm) ||
        (!rc->is_src_frame_alt_ref &&
         (refresh_frame_flags->golden_frame || is_intrl_arf_boost ||
          refresh_frame_flags->alt_ref_frame))) {
      active_best_quality -=
          (cpi->twopass.extend_minq + cpi->twopass.extend_minq_fast);
      active_worst_quality += (cpi->twopass.extend_maxq / 2);
    } else {
      active_best_quality -=
          (cpi->twopass.extend_minq + cpi->twopass.extend_minq_fast) / 2;
      active_worst_quality += cpi->twopass.extend_maxq;
    }
  }

  aom_clear_system_state();
#ifndef STRICT_RC
  // Static forced key frames Q restrictions dealt with elsewhere.
  if (!(frame_is_intra_only(cm)) || !rc->this_key_frame_forced ||
      (cpi->twopass.last_kfgroup_zeromotion_pct < STATIC_MOTION_THRESH)) {
    const int qdelta = av1_frame_type_qdelta(cpi, active_worst_quality);
    active_worst_quality =
        AOMMAX(active_worst_quality + qdelta, active_best_quality);
  }
#endif

  // Modify active_best_quality for downscaled normal frames.
  if (av1_frame_scaled(cm) && !frame_is_kf_gf_arf(cpi)) {
    int qdelta = av1_compute_qdelta_by_rate(
        rc, cm->current_frame.frame_type, active_best_quality, 2.0,
        cpi->is_screen_content_type, bit_depth);
    active_best_quality =
        AOMMAX(active_best_quality + qdelta, rc->best_quality);
  }

  active_best_quality =
      clamp(active_best_quality, rc->best_quality, rc->worst_quality);
  active_worst_quality =
      clamp(active_worst_quality, active_best_quality, rc->worst_quality);

  *active_best = active_best_quality;
  *active_worst = active_worst_quality;
}

/*!\brief Gets a Q value to use  for the current frame
 *
 *
 * Selects a Q value from a permitted range that we estimate
 * will result in approximately the target number of bits.
 *
 * \ingroup rate_control
 * \param[in]   cpi                   Top level encoder instance structure
 * \param[in]   width                 Width of frame
 * \param[in]   height                Height of frame
 * \param[in]   active_worst_quality  Max Q allowed
 * \param[in]   active_best_quality   Min Q allowed
 *
 * \return The suggested Q for this frame.
 */
static int get_q(const AV1_COMP *cpi, const int width, const int height,
                 const int active_worst_quality,
                 const int active_best_quality) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  int q;

  if (cpi->oxcf.rc_cfg.mode == AOM_Q ||
      (frame_is_intra_only(cm) && !rc->this_key_frame_forced &&
       cpi->twopass.kf_zeromotion_pct >= STATIC_KF_GROUP_THRESH &&
       rc->frames_to_key > 1)) {
    q = active_best_quality;
    // Special case code to try and match quality with forced key frames.
  } else if (frame_is_intra_only(cm) && rc->this_key_frame_forced) {
    // If static since last kf use better of last boosted and last kf q.
    if (cpi->twopass.last_kfgroup_zeromotion_pct >= STATIC_MOTION_THRESH) {
      q = AOMMIN(rc->last_kf_qindex, rc->last_boosted_qindex);
    } else {
      q = AOMMIN(rc->last_boosted_qindex,
                 (active_best_quality + active_worst_quality) / 2);
    }
    q = clamp(q, active_best_quality, active_worst_quality);
  } else {
    q = av1_rc_regulate_q(cpi, rc->this_frame_target, active_best_quality,
                          active_worst_quality, width, height);
    if (q > active_worst_quality) {
      // Special case when we are targeting the max allowed rate.
      if (rc->this_frame_target < rc->max_frame_bandwidth) {
        q = active_worst_quality;
      }
    }
    q = AOMMAX(q, active_best_quality);
  }
  return q;
}

// Returns |active_best_quality| for an inter frame.
// The |active_best_quality| depends on different rate control modes:
// VBR, Q, CQ, CBR.
// The returning active_best_quality could further be adjusted in
// adjust_active_best_and_worst_quality().
static int get_active_best_quality(const AV1_COMP *const cpi,
                                   const int active_worst_quality,
                                   const int cq_level, const int gf_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const int bit_depth = cm->seq_params.bit_depth;
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;
  const GF_GROUP *gf_group = &cpi->gf_group;
  const enum aom_rc_mode rc_mode = oxcf->rc_cfg.mode;
  int *inter_minq;
  ASSIGN_MINQ_TABLE(bit_depth, inter_minq);
  int active_best_quality = 0;
  const int is_intrl_arf_boost =
      gf_group->update_type[gf_index] == INTNL_ARF_UPDATE;
  const int is_leaf_frame =
      !(refresh_frame_flags->golden_frame ||
        refresh_frame_flags->alt_ref_frame || is_intrl_arf_boost);
  const int is_overlay_frame = rc->is_src_frame_alt_ref;

  if (is_leaf_frame || is_overlay_frame) {
    if (rc_mode == AOM_Q) return cq_level;

    active_best_quality = inter_minq[active_worst_quality];
    // For the constrained quality mode we don't want
    // q to fall below the cq level.
    if ((rc_mode == AOM_CQ) && (active_best_quality < cq_level)) {
      active_best_quality = cq_level;
    }
    return active_best_quality;
  }

  // TODO(chengchen): can we remove this condition?
  if (rc_mode == AOM_Q && !refresh_frame_flags->alt_ref_frame &&
      !refresh_frame_flags->golden_frame && !is_intrl_arf_boost) {
    return cq_level;
  }

  // Determine active_best_quality for frames that are not leaf or overlay.
  int q = active_worst_quality;
  // Use the lower of active_worst_quality and recent
  // average Q as basis for GF/ARF best Q limit unless last frame was
  // a key frame.
  if (rc->frames_since_key > 1 &&
      rc->avg_frame_qindex[INTER_FRAME] < active_worst_quality) {
    q = rc->avg_frame_qindex[INTER_FRAME];
  }
  if (rc_mode == AOM_CQ && q < cq_level) q = cq_level;
  active_best_quality = get_gf_active_quality(rc, q, bit_depth);
  // Constrained quality use slightly lower active best.
  if (rc_mode == AOM_CQ) active_best_quality = active_best_quality * 15 / 16;
  const int min_boost = get_gf_high_motion_quality(q, bit_depth);
  const int boost = min_boost - active_best_quality;
  active_best_quality = min_boost - (int)(boost * rc->arf_boost_factor);
  if (!is_intrl_arf_boost) return active_best_quality;

  if (rc_mode == AOM_Q || rc_mode == AOM_CQ) active_best_quality = rc->arf_q;
  int this_height = gf_group_pyramid_level(gf_group, gf_index);
  while (this_height > 1) {
    active_best_quality = (active_best_quality + active_worst_quality + 1) / 2;
    --this_height;
  }
  return active_best_quality;
}

/*!\brief Picks q and q bounds given rate control parameters in \c cpi->rc.
 *
 * Handles the the general cases not covered by
 * \ref rc_pick_q_and_bounds_no_stats_cbr() and
 * \ref rc_pick_q_and_bounds_no_stats()
 *
 * \ingroup rate_control
 * \param[in]       cpi          Top level encoder structure
 * \param[in]       width        Coded frame width
 * \param[in]       height       Coded frame height
 * \param[in]       gf_index     Index of this frame in the golden frame group
 * \param[out]      bottom_index Bottom bound for q index (best quality)
 * \param[out]      top_index    Top bound for q index (worst quality)
 * \return Returns selected q index to be used for encoding this frame.
 */
static int rc_pick_q_and_bounds(const AV1_COMP *cpi, int width, int height,
                                int gf_index, int *bottom_index,
                                int *top_index) {
  const AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;
  const GF_GROUP *gf_group = &cpi->gf_group;
  assert(IMPLIES(has_no_stats_stage(cpi),
                 cpi->oxcf.rc_cfg.mode == AOM_Q &&
                     gf_group->update_type[gf_index] != ARF_UPDATE));
  const int cq_level =
      get_active_cq_level(rc, oxcf, frame_is_intra_only(cm), cpi->superres_mode,
                          cm->superres_scale_denominator);
  const int bit_depth = cm->seq_params.bit_depth;

  if (oxcf->q_cfg.use_fixed_qp_offsets) {
    return get_q_using_fixed_offsets(oxcf, rc, gf_group, gf_group->index,
                                     cq_level, bit_depth);
  }

  int active_best_quality = 0;
  int active_worst_quality = rc->active_worst_quality;
  int q;

  const int is_intrl_arf_boost =
      gf_group->update_type[gf_index] == INTNL_ARF_UPDATE;

  if (frame_is_intra_only(cm)) {
    const int is_fwd_kf = cm->current_frame.frame_type == KEY_FRAME &&
                          cm->show_frame == 0 && cpi->no_show_fwd_kf;
    get_intra_q_and_bounds(cpi, width, height, &active_best_quality,
                           &active_worst_quality, cq_level, is_fwd_kf);
#ifdef STRICT_RC
    active_best_quality = 0;
#endif
  } else {
    //  Active best quality limited by previous layer.
    const int pyramid_level = gf_group_pyramid_level(gf_group, gf_index);

    if ((pyramid_level <= 1) || (pyramid_level > MAX_ARF_LAYERS) ||
        (oxcf->rc_cfg.mode == AOM_Q)) {
      active_best_quality = get_active_best_quality(cpi, active_worst_quality,
                                                    cq_level, gf_index);
    } else {
      active_best_quality = rc->active_best_quality[pyramid_level - 1] + 1;
      active_best_quality = AOMMIN(active_best_quality, active_worst_quality);
#ifdef STRICT_RC
      active_best_quality += (active_worst_quality - active_best_quality) / 16;
#else
      active_best_quality += (active_worst_quality - active_best_quality) / 2;
#endif
    }

    // For alt_ref and GF frames (including internal arf frames) adjust the
    // worst allowed quality as well. This insures that even on hard
    // sections we dont clamp the Q at the same value for arf frames and
    // leaf (non arf) frames. This is important to the TPL model which assumes
    // Q drops with each arf level.
    if (!(rc->is_src_frame_alt_ref) &&
        (refresh_frame_flags->golden_frame ||
         refresh_frame_flags->alt_ref_frame || is_intrl_arf_boost)) {
      active_worst_quality =
          (active_best_quality + (3 * active_worst_quality) + 2) / 4;
    }
  }

  adjust_active_best_and_worst_quality(
      cpi, is_intrl_arf_boost, &active_worst_quality, &active_best_quality);
  q = get_q(cpi, width, height, active_worst_quality, active_best_quality);

  // Special case when we are targeting the max allowed rate.
  if (rc->this_frame_target >= rc->max_frame_bandwidth &&
      q > active_worst_quality) {
    active_worst_quality = q;
  }

  *top_index = active_worst_quality;
  *bottom_index = active_best_quality;

  assert(*top_index <= rc->worst_quality && *top_index >= rc->best_quality);
  assert(*bottom_index <= rc->worst_quality &&
         *bottom_index >= rc->best_quality);
  assert(q <= rc->worst_quality && q >= rc->best_quality);

  return q;
}

int av1_rc_pick_q_and_bounds(const AV1_COMP *cpi, RATE_CONTROL *rc, int width,
                             int height, int gf_index, int *bottom_index,
                             int *top_index) {
  int q;
  // TODO(sarahparker) merge no-stats vbr and altref q computation
  // with rc_pick_q_and_bounds().
  const GF_GROUP *gf_group = &cpi->gf_group;
  if ((cpi->oxcf.rc_cfg.mode != AOM_Q ||
       gf_group->update_type[gf_index] == ARF_UPDATE) &&
      has_no_stats_stage(cpi)) {
    if (cpi->oxcf.rc_cfg.mode == AOM_CBR) {
      q = rc_pick_q_and_bounds_no_stats_cbr(cpi, width, height, bottom_index,
                                            top_index);
#if USE_UNRESTRICTED_Q_IN_CQ_MODE
    } else if (cpi->oxcf.rc_cfg.mode == AOM_CQ) {
      q = rc_pick_q_and_bounds_no_stats_cq(cpi, width, height, bottom_index,
                                           top_index);
#endif  // USE_UNRESTRICTED_Q_IN_CQ_MODE
    } else {
      q = rc_pick_q_and_bounds_no_stats(cpi, width, height, gf_index,
                                        bottom_index, top_index);
    }
  } else {
    q = rc_pick_q_and_bounds(cpi, width, height, gf_index, bottom_index,
                             top_index);
  }
  if (gf_group->update_type[gf_index] == ARF_UPDATE) rc->arf_q = q;

  return q;
}

void av1_rc_compute_frame_size_bounds(const AV1_COMP *cpi, int frame_target,
                                      int *frame_under_shoot_limit,
                                      int *frame_over_shoot_limit) {
  if (cpi->oxcf.rc_cfg.mode == AOM_Q) {
    *frame_under_shoot_limit = 0;
    *frame_over_shoot_limit = INT_MAX;
  } else {
    // For very small rate targets where the fractional adjustment
    // may be tiny make sure there is at least a minimum range.
    assert(cpi->sf.hl_sf.recode_tolerance <= 100);
    const int tolerance = (int)AOMMAX(
        100, ((int64_t)cpi->sf.hl_sf.recode_tolerance * frame_target) / 100);
    *frame_under_shoot_limit = AOMMAX(frame_target - tolerance, 0);
    *frame_over_shoot_limit =
        AOMMIN(frame_target + tolerance, cpi->rc.max_frame_bandwidth);
  }
}

void av1_rc_set_frame_target(AV1_COMP *cpi, int target, int width, int height) {
  const AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;

  rc->this_frame_target = target;

  // Modify frame size target when down-scaled.
  if (av1_frame_scaled(cm) && cpi->oxcf.rc_cfg.mode != AOM_CBR) {
    rc->this_frame_target =
        (int)(rc->this_frame_target *
              resize_rate_factor(&cpi->oxcf.frm_dim_cfg, width, height));
  }

  // Target rate per SB64 (including partial SB64s.
  rc->sb64_target_rate =
      (int)(((int64_t)rc->this_frame_target << 12) / (width * height));
}

static void update_alt_ref_frame_stats(AV1_COMP *cpi) {
  // this frame refreshes means next frames don't unless specified by user
  RATE_CONTROL *const rc = &cpi->rc;
  rc->frames_since_golden = 0;
}

static void update_golden_frame_stats(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;

  // Update the Golden frame usage counts.
  if (cpi->refresh_frame.golden_frame || rc->is_src_frame_alt_ref) {
    rc->frames_since_golden = 0;
  } else if (cpi->common.show_frame) {
    rc->frames_since_golden++;
  }
}

void av1_rc_postencode_update(AV1_COMP *cpi, uint64_t bytes_used) {
  const AV1_COMMON *const cm = &cpi->common;
  const CurrentFrame *const current_frame = &cm->current_frame;
  RATE_CONTROL *const rc = &cpi->rc;
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const RefreshFrameFlagsInfo *const refresh_frame_flags = &cpi->refresh_frame;

  const int is_intrnl_arf =
      gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE;

  const int qindex = cm->quant_params.base_qindex;

  // Update rate control heuristics
  rc->projected_frame_size = (int)(bytes_used << 3);

  // Post encode loop adjustment of Q prediction.
  av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);

  // Keep a record of last Q and ambient average Q.
  if (current_frame->frame_type == KEY_FRAME) {
    rc->last_q[KEY_FRAME] = qindex;
    rc->avg_frame_qindex[KEY_FRAME] =
        ROUND_POWER_OF_TWO(3 * rc->avg_frame_qindex[KEY_FRAME] + qindex, 2);
  } else {
    if ((cpi->use_svc && cpi->oxcf.rc_cfg.mode == AOM_CBR) ||
        (!rc->is_src_frame_alt_ref &&
         !(refresh_frame_flags->golden_frame || is_intrnl_arf ||
           refresh_frame_flags->alt_ref_frame))) {
      rc->last_q[INTER_FRAME] = qindex;
      rc->avg_frame_qindex[INTER_FRAME] =
          ROUND_POWER_OF_TWO(3 * rc->avg_frame_qindex[INTER_FRAME] + qindex, 2);
      rc->ni_frames++;
      rc->tot_q += av1_convert_qindex_to_q(qindex, cm->seq_params.bit_depth);
      rc->avg_q = rc->tot_q / rc->ni_frames;
      // Calculate the average Q for normal inter frames (not key or GFU
      // frames).
      rc->ni_tot_qi += qindex;
      rc->ni_av_qi = rc->ni_tot_qi / rc->ni_frames;
    }
  }

  // Keep record of last boosted (KF/GF/ARF) Q value.
  // If the current frame is coded at a lower Q then we also update it.
  // If all mbs in this group are skipped only update if the Q value is
  // better than that already stored.
  // This is used to help set quality in forced key frames to reduce popping
  if ((qindex < rc->last_boosted_qindex) ||
      (current_frame->frame_type == KEY_FRAME) ||
      (!rc->constrained_gf_group &&
       (refresh_frame_flags->alt_ref_frame || is_intrnl_arf ||
        (refresh_frame_flags->golden_frame && !rc->is_src_frame_alt_ref)))) {
    rc->last_boosted_qindex = qindex;
  }
  if (current_frame->frame_type == KEY_FRAME) rc->last_kf_qindex = qindex;

  update_buffer_level(cpi, rc->projected_frame_size);
  rc->prev_avg_frame_bandwidth = rc->avg_frame_bandwidth;

  // Rolling monitors of whether we are over or underspending used to help
  // regulate min and Max Q in two pass.
  if (av1_frame_scaled(cm))
    rc->this_frame_target = (int)(rc->this_frame_target /
                                  resize_rate_factor(&cpi->oxcf.frm_dim_cfg,
                                                     cm->width, cm->height));
  if (current_frame->frame_type != KEY_FRAME) {
    rc->rolling_target_bits = (int)ROUND_POWER_OF_TWO_64(
        rc->rolling_target_bits * 3 + rc->this_frame_target, 2);
    rc->rolling_actual_bits = (int)ROUND_POWER_OF_TWO_64(
        rc->rolling_actual_bits * 3 + rc->projected_frame_size, 2);
    rc->long_rolling_target_bits = (int)ROUND_POWER_OF_TWO_64(
        rc->long_rolling_target_bits * 31 + rc->this_frame_target, 5);
    rc->long_rolling_actual_bits = (int)ROUND_POWER_OF_TWO_64(
        rc->long_rolling_actual_bits * 31 + rc->projected_frame_size, 5);
  }

  // Actual bits spent
  rc->total_actual_bits += rc->projected_frame_size;
  rc->total_target_bits += cm->show_frame ? rc->avg_frame_bandwidth : 0;

  rc->total_target_vs_actual = rc->total_actual_bits - rc->total_target_bits;

  if (is_altref_enabled(cpi->oxcf.gf_cfg.lag_in_frames,
                        cpi->oxcf.gf_cfg.enable_auto_arf) &&
      refresh_frame_flags->alt_ref_frame &&
      (current_frame->frame_type != KEY_FRAME && !frame_is_sframe(cm)))
    // Update the alternate reference frame stats as appropriate.
    update_alt_ref_frame_stats(cpi);
  else
    // Update the Golden frame stats as appropriate.
    update_golden_frame_stats(cpi);

  if (current_frame->frame_type == KEY_FRAME) rc->frames_since_key = 0;
  // if (current_frame->frame_number == 1 && cm->show_frame)
  /*
  rc->this_frame_target =
      (int)(rc->this_frame_target / resize_rate_factor(&cpi->oxcf.frm_dim_cfg,
  cm->width, cm->height));
      */
}

void av1_rc_postencode_update_drop_frame(AV1_COMP *cpi) {
  // Update buffer level with zero size, update frame counters, and return.
  update_buffer_level(cpi, 0);
  cpi->rc.frames_since_key++;
  cpi->rc.frames_to_key--;
  cpi->rc.rc_2_frame = 0;
  cpi->rc.rc_1_frame = 0;
}

int av1_find_qindex(double desired_q, aom_bit_depth_t bit_depth,
                    int best_qindex, int worst_qindex) {
  assert(best_qindex <= worst_qindex);
  int low = best_qindex;
  int high = worst_qindex;
  while (low < high) {
    const int mid = (low + high) >> 1;
    const double mid_q = av1_convert_qindex_to_q(mid, bit_depth);
    if (mid_q < desired_q) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  assert(low == high);
  assert(av1_convert_qindex_to_q(low, bit_depth) >= desired_q ||
         low == worst_qindex);
  return low;
}

int av1_compute_qdelta(const RATE_CONTROL *rc, double qstart, double qtarget,
                       aom_bit_depth_t bit_depth) {
  const int start_index =
      av1_find_qindex(qstart, bit_depth, rc->best_quality, rc->worst_quality);
  const int target_index =
      av1_find_qindex(qtarget, bit_depth, rc->best_quality, rc->worst_quality);
  return target_index - start_index;
}

// Find q_index for the desired_bits_per_mb, within [best_qindex, worst_qindex],
// assuming 'correction_factor' is 1.0.
// To be precise, 'q_index' is the smallest integer, for which the corresponding
// bits per mb <= desired_bits_per_mb.
// If no such q index is found, returns 'worst_qindex'.
static int find_qindex_by_rate(int desired_bits_per_mb,
                               aom_bit_depth_t bit_depth, FRAME_TYPE frame_type,
                               const int is_screen_content_type,
                               int best_qindex, int worst_qindex) {
  assert(best_qindex <= worst_qindex);
  int low = best_qindex;
  int high = worst_qindex;
  while (low < high) {
    const int mid = (low + high) >> 1;
    const int mid_bits_per_mb = av1_rc_bits_per_mb(
        frame_type, mid, 1.0, bit_depth, is_screen_content_type);
    if (mid_bits_per_mb > desired_bits_per_mb) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  assert(low == high);
  assert(av1_rc_bits_per_mb(frame_type, low, 1.0, bit_depth,
                            is_screen_content_type) <= desired_bits_per_mb ||
         low == worst_qindex);
  return low;
}

int av1_compute_qdelta_by_rate(const RATE_CONTROL *rc, FRAME_TYPE frame_type,
                               int qindex, double rate_target_ratio,
                               const int is_screen_content_type,
                               aom_bit_depth_t bit_depth) {
  // Look up the current projected bits per block for the base index
  const int base_bits_per_mb = av1_rc_bits_per_mb(
      frame_type, qindex, 1.0, bit_depth, is_screen_content_type);

  // Find the target bits per mb based on the base value and given ratio.
  const int target_bits_per_mb = (int)(rate_target_ratio * base_bits_per_mb);

  const int target_index = find_qindex_by_rate(
      target_bits_per_mb, bit_depth, frame_type, is_screen_content_type,
      rc->best_quality, rc->worst_quality);
  return target_index - qindex;
}

void av1_rc_set_gf_interval_range(const AV1_COMP *const cpi,
                                  RATE_CONTROL *const rc) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  // Special case code for 1 pass fixed Q mode tests
  if ((has_no_stats_stage(cpi)) && (oxcf->rc_cfg.mode == AOM_Q)) {
    rc->max_gf_interval = FIXED_GF_INTERVAL;
    rc->min_gf_interval = FIXED_GF_INTERVAL;
    rc->static_scene_max_gf_interval = FIXED_GF_INTERVAL;
  } else {
    // Set Maximum gf/arf interval
    rc->max_gf_interval = oxcf->gf_cfg.max_gf_interval;
    rc->min_gf_interval = oxcf->gf_cfg.min_gf_interval;
    if (rc->min_gf_interval == 0)
      rc->min_gf_interval = av1_rc_get_default_min_gf_interval(
          oxcf->frm_dim_cfg.width, oxcf->frm_dim_cfg.height, cpi->framerate);
    if (rc->max_gf_interval == 0)
      rc->max_gf_interval = av1_rc_get_default_max_gf_interval(
          cpi->framerate, rc->min_gf_interval);
    /*
     * Extended max interval for genuinely static scenes like slide shows.
     * The no.of.stats available in the case of LAP is limited,
     * hence setting to max_gf_interval.
     */
    if (cpi->lap_enabled)
      rc->static_scene_max_gf_interval = rc->max_gf_interval + 1;
    else
      rc->static_scene_max_gf_interval = MAX_STATIC_GF_GROUP_LENGTH;

    if (rc->max_gf_interval > rc->static_scene_max_gf_interval)
      rc->max_gf_interval = rc->static_scene_max_gf_interval;

    // Clamp min to max
    rc->min_gf_interval = AOMMIN(rc->min_gf_interval, rc->max_gf_interval);
  }
}

void av1_rc_update_framerate(AV1_COMP *cpi, int width, int height) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  RATE_CONTROL *const rc = &cpi->rc;
  int vbr_max_bits;
  const int MBs = av1_get_MBs(width, height);

  rc->avg_frame_bandwidth =
      (int)(oxcf->rc_cfg.target_bandwidth / cpi->framerate);
  rc->min_frame_bandwidth =
      (int)(rc->avg_frame_bandwidth * oxcf->rc_cfg.vbrmin_section / 100);

  rc->min_frame_bandwidth =
      AOMMAX(rc->min_frame_bandwidth, FRAME_OVERHEAD_BITS);

  // A maximum bitrate for a frame is defined.
  // The baseline for this aligns with HW implementations that
  // can support decode of 1080P content up to a bitrate of MAX_MB_RATE bits
  // per 16x16 MB (averaged over a frame). However this limit is extended if
  // a very high rate is given on the command line or the the rate cannnot
  // be acheived because of a user specificed max q (e.g. when the user
  // specifies lossless encode.
  vbr_max_bits =
      (int)(((int64_t)rc->avg_frame_bandwidth * oxcf->rc_cfg.vbrmax_section) /
            100);
  rc->max_frame_bandwidth =
      AOMMAX(AOMMAX((MBs * MAX_MB_RATE), MAXRATE_1080P), vbr_max_bits);

  av1_rc_set_gf_interval_range(cpi, rc);
}

#define VBR_PCT_ADJUSTMENT_LIMIT 50
// For VBR...adjustment to the frame target based on error from previous frames
static void vbr_rate_correction(AV1_COMP *cpi, int *this_frame_target) {
  RATE_CONTROL *const rc = &cpi->rc;
  int64_t vbr_bits_off_target = rc->vbr_bits_off_target;
  const int stats_count =
      cpi->twopass.stats_buf_ctx->total_stats != NULL
          ? (int)cpi->twopass.stats_buf_ctx->total_stats->count
          : 0;
  const int frame_window = AOMMIN(
      16, (int)(stats_count - (int)cpi->common.current_frame.frame_number));
  assert(VBR_PCT_ADJUSTMENT_LIMIT <= 100);
  if (frame_window > 0) {
    const int max_delta = (int)AOMMIN(
        abs((int)(vbr_bits_off_target / frame_window)),
        ((int64_t)(*this_frame_target) * VBR_PCT_ADJUSTMENT_LIMIT) / 100);

    // vbr_bits_off_target > 0 means we have extra bits to spend
    // vbr_bits_off_target < 0 we are currently overshooting
    *this_frame_target += (vbr_bits_off_target >= 0) ? max_delta : -max_delta;
  }

  // Fast redistribution of bits arising from massive local undershoot.
  // Dont do it for kf,arf,gf or overlay frames.
  if (!frame_is_kf_gf_arf(cpi) && !rc->is_src_frame_alt_ref &&
      rc->vbr_bits_off_target_fast) {
    int one_frame_bits = AOMMAX(rc->avg_frame_bandwidth, *this_frame_target);
    int fast_extra_bits;
    fast_extra_bits = (int)AOMMIN(rc->vbr_bits_off_target_fast, one_frame_bits);
    fast_extra_bits = (int)AOMMIN(
        fast_extra_bits,
        AOMMAX(one_frame_bits / 8, rc->vbr_bits_off_target_fast / 8));
    *this_frame_target += (int)fast_extra_bits;
    rc->vbr_bits_off_target_fast -= fast_extra_bits;
  }
}

void av1_set_target_rate(AV1_COMP *cpi, int width, int height) {
  RATE_CONTROL *const rc = &cpi->rc;
  int target_rate = rc->base_frame_target;

  // Correction to rate target based on prior over or under shoot.
  if (cpi->oxcf.rc_cfg.mode == AOM_VBR || cpi->oxcf.rc_cfg.mode == AOM_CQ)
    vbr_rate_correction(cpi, &target_rate);
  av1_rc_set_frame_target(cpi, target_rate, width, height);
}

int av1_calc_pframe_target_size_one_pass_vbr(
    const AV1_COMP *const cpi, FRAME_UPDATE_TYPE frame_update_type) {
  static const int af_ratio = 10;
  const RATE_CONTROL *const rc = &cpi->rc;
  int64_t target;
#if USE_ALTREF_FOR_ONE_PASS
  if (frame_update_type == KF_UPDATE || frame_update_type == GF_UPDATE ||
      frame_update_type == ARF_UPDATE) {
    target = ((int64_t)rc->avg_frame_bandwidth * rc->baseline_gf_interval *
              af_ratio) /
             (rc->baseline_gf_interval + af_ratio - 1);
  } else {
    target = ((int64_t)rc->avg_frame_bandwidth * rc->baseline_gf_interval) /
             (rc->baseline_gf_interval + af_ratio - 1);
  }
  if (target > INT_MAX) target = INT_MAX;
#else
  target = rc->avg_frame_bandwidth;
#endif
  return av1_rc_clamp_pframe_target_size(cpi, (int)target, frame_update_type);
}

int av1_calc_iframe_target_size_one_pass_vbr(const AV1_COMP *const cpi) {
  static const int kf_ratio = 25;
  const RATE_CONTROL *rc = &cpi->rc;
  const int target = rc->avg_frame_bandwidth * kf_ratio;
  return av1_rc_clamp_iframe_target_size(cpi, target);
}

int av1_calc_pframe_target_size_one_pass_cbr(
    const AV1_COMP *cpi, FRAME_UPDATE_TYPE frame_update_type) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  const RATE_CONTROL *rc = &cpi->rc;
  const RateControlCfg *rc_cfg = &oxcf->rc_cfg;
  const int64_t diff = rc->optimal_buffer_level - rc->buffer_level;
  const int64_t one_pct_bits = 1 + rc->optimal_buffer_level / 100;
  int min_frame_target =
      AOMMAX(rc->avg_frame_bandwidth >> 4, FRAME_OVERHEAD_BITS);
  int target;

  if (rc_cfg->gf_cbr_boost_pct) {
    const int af_ratio_pct = rc_cfg->gf_cbr_boost_pct + 100;
    if (frame_update_type == GF_UPDATE || frame_update_type == OVERLAY_UPDATE) {
      target =
          (rc->avg_frame_bandwidth * rc->baseline_gf_interval * af_ratio_pct) /
          (rc->baseline_gf_interval * 100 + af_ratio_pct - 100);
    } else {
      target = (rc->avg_frame_bandwidth * rc->baseline_gf_interval * 100) /
               (rc->baseline_gf_interval * 100 + af_ratio_pct - 100);
    }
  } else {
    target = rc->avg_frame_bandwidth;
  }
  if (cpi->use_svc) {
    // Note that for layers, avg_frame_bandwidth is the cumulative
    // per-frame-bandwidth. For the target size of this frame, use the
    // layer average frame size (i.e., non-cumulative per-frame-bw).
    int layer =
        LAYER_IDS_TO_IDX(cpi->svc.spatial_layer_id, cpi->svc.temporal_layer_id,
                         cpi->svc.number_temporal_layers);
    const LAYER_CONTEXT *lc = &cpi->svc.layer_context[layer];
    target = lc->avg_frame_size;
    min_frame_target = AOMMAX(lc->avg_frame_size >> 4, FRAME_OVERHEAD_BITS);
  }
  if (diff > 0) {
    // Lower the target bandwidth for this frame.
    const int pct_low =
        (int)AOMMIN(diff / one_pct_bits, rc_cfg->under_shoot_pct);
    target -= (target * pct_low) / 200;
  } else if (diff < 0) {
    // Increase the target bandwidth for this frame.
    const int pct_high =
        (int)AOMMIN(-diff / one_pct_bits, rc_cfg->over_shoot_pct);
    target += (target * pct_high) / 200;
  }
  if (rc_cfg->max_inter_bitrate_pct) {
    const int max_rate =
        rc->avg_frame_bandwidth * rc_cfg->max_inter_bitrate_pct / 100;
    target = AOMMIN(target, max_rate);
  }
  return AOMMAX(min_frame_target, target);
}

int av1_calc_iframe_target_size_one_pass_cbr(const AV1_COMP *cpi) {
  const RATE_CONTROL *rc = &cpi->rc;
  int target;
  if (cpi->common.current_frame.frame_number == 0) {
    target = ((rc->starting_buffer_level / 2) > INT_MAX)
                 ? INT_MAX
                 : (int)(rc->starting_buffer_level / 2);
  } else {
    int kf_boost = 32;
    double framerate = cpi->framerate;

    kf_boost = AOMMAX(kf_boost, (int)(2 * framerate - 16));
    if (rc->frames_since_key < framerate / 2) {
      kf_boost = (int)(kf_boost * rc->frames_since_key / (framerate / 2));
    }
    target = ((16 + kf_boost) * rc->avg_frame_bandwidth) >> 4;
  }
  return av1_rc_clamp_iframe_target_size(cpi, target);
}

/*!\brief Setup the reference prediction structure for 1 pass real-time
 *
 * Set the reference prediction structure for 1 layer.
 * Current structue is to use 3 references (LAST, GOLDEN, ALTREF),
 * where ALT_REF always behind current by lag_alt frames, and GOLDEN is
 * either updated on LAST with period baseline_gf_interval (fixed slot)
 * or always behind current by lag_gld (gld_fixed_slot = 0, lag_gld <= 7).
 *
 * \ingroup rate_control
 * \param[in]       cpi          Top level encoder structure
 * \param[in]       gf_update    Flag to indicate if GF is updated
 *
 * \return Nothing is returned. Instead the settings for the prediction
 * structure are set in \c cpi-ext_flags; and the buffer slot index
 * (for each of 7 references) and refresh flags (for each of the 8 slots)
 * are set in \c cpi->svc.ref_idx[] and \c cpi->svc.refresh[].
 */
void av1_set_reference_structure_one_pass_rt(AV1_COMP *cpi, int gf_update) {
  AV1_COMMON *const cm = &cpi->common;
  ExternalFlags *const ext_flags = &cpi->ext_flags;
  ExtRefreshFrameFlagsInfo *const ext_refresh_frame_flags =
      &ext_flags->refresh_frame;
  SVC *const svc = &cpi->svc;
  const int gld_fixed_slot = 1;
  const unsigned int lag_alt = 4;
  int last_idx = 0;
  int last_idx_refresh = 0;
  int gld_idx = 0;
  int alt_ref_idx = 0;
  ext_refresh_frame_flags->update_pending = 1;
  svc->external_ref_frame_config = 1;
  ext_flags->ref_frame_flags = 0;
  ext_refresh_frame_flags->last_frame = 1;
  ext_refresh_frame_flags->golden_frame = 0;
  ext_refresh_frame_flags->alt_ref_frame = 0;
  for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) svc->ref_idx[i] = 7;
  for (int i = 0; i < REF_FRAMES; ++i) svc->refresh[i] = 0;
  // Always reference LAST, GOLDEN, ALTREF
  ext_flags->ref_frame_flags ^= AOM_LAST_FLAG;
  ext_flags->ref_frame_flags ^= AOM_GOLD_FLAG;
  ext_flags->ref_frame_flags ^= AOM_ALT_FLAG;
  const int sh = 7 - gld_fixed_slot;
  // Moving index slot for last: 0 - (sh - 1).
  if (cm->current_frame.frame_number > 1)
    last_idx = ((cm->current_frame.frame_number - 1) % sh);
  // Moving index for refresh of last: one ahead for next frame.
  last_idx_refresh = (cm->current_frame.frame_number % sh);
  gld_idx = 6;
  if (!gld_fixed_slot) {
    gld_idx = 7;
    const unsigned int lag_gld = 7;  // Must be <= 7.
    // Moving index for gld_ref, lag behind current by gld_interval frames.
    if (cm->current_frame.frame_number > lag_gld)
      gld_idx = ((cm->current_frame.frame_number - lag_gld) % sh);
  }
  // Moving index for alt_ref, lag behind LAST by lag_alt frames.
  if (cm->current_frame.frame_number > lag_alt)
    alt_ref_idx = ((cm->current_frame.frame_number - lag_alt) % sh);
  svc->ref_idx[0] = last_idx;          // LAST
  svc->ref_idx[1] = last_idx_refresh;  // LAST2 (for refresh of last).
  svc->ref_idx[3] = gld_idx;           // GOLDEN
  svc->ref_idx[6] = alt_ref_idx;       // ALT_REF
  // Refresh this slot, which will become LAST on next frame.
  svc->refresh[last_idx_refresh] = 1;
  // Update GOLDEN on period for fixed slot case.
  if (gld_fixed_slot && gf_update) {
    ext_refresh_frame_flags->golden_frame = 1;
    svc->refresh[gld_idx] = 1;
  }
}

/*!\brief Check for scene detection, for 1 pass real-time mode.
 *
 * Compute average source sad (temporal sad: between current source and
 * previous source) over a subset of superblocks. Use this is detect big changes
 * in content and set the \c cpi->rc.high_source_sad flag.
 *
 * \ingroup rate_control
 * \param[in]       cpi          Top level encoder structure
 *
 * \return Nothing is returned. Instead the flag \c cpi->rc.high_source_sad
 * is set if scene change is detected, and \c cpi->rc.avg_source_sad is updated.
 */
static void rc_scene_detection_onepass_rt(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  YV12_BUFFER_CONFIG const *unscaled_src = cpi->unscaled_source;
  YV12_BUFFER_CONFIG const *unscaled_last_src = cpi->unscaled_last_source;
  uint8_t *src_y;
  int src_ystride;
  int src_width;
  int src_height;
  uint8_t *last_src_y;
  int last_src_ystride;
  int last_src_width;
  int last_src_height;
  if (cpi->unscaled_source == NULL || cpi->unscaled_last_source == NULL) return;
  src_y = unscaled_src->y_buffer;
  src_ystride = unscaled_src->y_stride;
  src_width = unscaled_src->y_width;
  src_height = unscaled_src->y_height;
  last_src_y = unscaled_last_src->y_buffer;
  last_src_ystride = unscaled_last_src->y_stride;
  last_src_width = unscaled_last_src->y_width;
  last_src_height = unscaled_last_src->y_height;
  rc->high_source_sad = 0;
  rc->prev_avg_source_sad = rc->avg_source_sad;
  if (src_width == last_src_width && src_height == last_src_height) {
    const int num_mi_cols = cm->mi_params.mi_cols;
    const int num_mi_rows = cm->mi_params.mi_rows;
    int num_zero_temp_sad = 0;
    uint32_t min_thresh = 10000;
    if (cpi->oxcf.tune_cfg.content != AOM_CONTENT_SCREEN) min_thresh = 100000;
    const BLOCK_SIZE bsize = BLOCK_64X64;
    int full_sampling = (cm->width * cm->height < 640 * 360) ? 1 : 0;
    // Loop over sub-sample of frame, compute average sad over 64x64 blocks.
    uint64_t avg_sad = 0;
    uint64_t tmp_sad = 0;
    int num_samples = 0;
    const int thresh = 6;
    // SAD is computed on 64x64 blocks
    const int sb_size_by_mb = (cm->seq_params.sb_size == BLOCK_128X128)
                                  ? (cm->seq_params.mib_size >> 1)
                                  : cm->seq_params.mib_size;
    const int sb_cols = (num_mi_cols + sb_size_by_mb - 1) / sb_size_by_mb;
    const int sb_rows = (num_mi_rows + sb_size_by_mb - 1) / sb_size_by_mb;
    uint64_t sum_sq_thresh = 10000;  // sum = sqrt(thresh / 64*64)) ~1.5
    int num_low_var_high_sumdiff = 0;
    int light_change = 0;
    // Flag to check light change or not.
    const int check_light_change = 0;
    for (int sbi_row = 0; sbi_row < sb_rows; ++sbi_row) {
      for (int sbi_col = 0; sbi_col < sb_cols; ++sbi_col) {
        // Checker-board pattern, ignore boundary.
        if (full_sampling ||
            ((sbi_row > 0 && sbi_col > 0) &&
             (sbi_row < sb_rows - 1 && sbi_col < sb_cols - 1) &&
             ((sbi_row % 2 == 0 && sbi_col % 2 == 0) ||
              (sbi_row % 2 != 0 && sbi_col % 2 != 0)))) {
          tmp_sad = cpi->fn_ptr[bsize].sdf(src_y, src_ystride, last_src_y,
                                           last_src_ystride);
          if (check_light_change) {
            unsigned int sse, variance;
            variance = cpi->fn_ptr[bsize].vf(src_y, src_ystride, last_src_y,
                                             last_src_ystride, &sse);
            // Note: sse - variance = ((sum * sum) >> 12)
            // Detect large lighting change.
            if (variance < (sse >> 1) && (sse - variance) > sum_sq_thresh) {
              num_low_var_high_sumdiff++;
            }
          }
          avg_sad += tmp_sad;
          num_samples++;
          if (tmp_sad == 0) num_zero_temp_sad++;
        }
        src_y += 64;
        last_src_y += 64;
      }
      src_y += (src_ystride << 6) - (sb_cols << 6);
      last_src_y += (last_src_ystride << 6) - (sb_cols << 6);
    }
    if (check_light_change && num_samples > 0 &&
        num_low_var_high_sumdiff > (num_samples >> 1))
      light_change = 1;
    if (num_samples > 0) avg_sad = avg_sad / num_samples;
    // Set high_source_sad flag if we detect very high increase in avg_sad
    // between current and previous frame value(s). Use minimum threshold
    // for cases where there is small change from content that is completely
    // static.
    if (!light_change &&
        avg_sad >
            AOMMAX(min_thresh, (unsigned int)(rc->avg_source_sad * thresh)) &&
        rc->frames_since_key > 1 + cpi->svc.number_spatial_layers &&
        num_zero_temp_sad < 3 * (num_samples >> 2))
      rc->high_source_sad = 1;
    else
      rc->high_source_sad = 0;
    rc->avg_source_sad = (3 * rc->avg_source_sad + avg_sad) >> 2;
  }
}

#define DEFAULT_KF_BOOST_RT 2300
#define DEFAULT_GF_BOOST_RT 2000

/*!\brief Set the GF baseline interval for 1 pass real-time mode.
 *
 *
 * \ingroup rate_control
 * \param[in]       cpi          Top level encoder structure
 * \param[in]       frame_type   frame type
 *
 * \return Return GF update flag, and update the \c cpi->rc with
 * the next GF interval settings.
 */
static int set_gf_interval_update_onepass_rt(AV1_COMP *cpi,
                                             FRAME_TYPE frame_type) {
  RATE_CONTROL *const rc = &cpi->rc;
  GF_GROUP *const gf_group = &cpi->gf_group;
  ResizePendingParams *const resize_pending_params =
      &cpi->resize_pending_params;
  int gf_update = 0;
  const int resize_pending =
      (resize_pending_params->width && resize_pending_params->height &&
       (cpi->common.width != resize_pending_params->width ||
        cpi->common.height != resize_pending_params->height));
  // GF update based on frames_till_gf_update_due, also
  // force upddate on resize pending frame or for scene change.
  if ((resize_pending || rc->high_source_sad ||
       rc->frames_till_gf_update_due == 0) &&
      cpi->svc.temporal_layer_id == 0 && cpi->svc.spatial_layer_id == 0) {
    if (cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ)
      av1_cyclic_refresh_set_golden_update(cpi);
    else
      rc->baseline_gf_interval = MAX_GF_INTERVAL;
    if (rc->baseline_gf_interval > rc->frames_to_key)
      rc->baseline_gf_interval = rc->frames_to_key;
    rc->gfu_boost = DEFAULT_GF_BOOST_RT;
    rc->constrained_gf_group =
        (rc->baseline_gf_interval >= rc->frames_to_key) ? 1 : 0;
    rc->frames_till_gf_update_due = rc->baseline_gf_interval;
    gf_group->index = 0;
    // SVC does not use GF as periodic boost.
    // TODO(marpan): Find better way to disable this for SVC.
    if (cpi->use_svc) {
      SVC *const svc = &cpi->svc;
      rc->baseline_gf_interval = MAX_STATIC_GF_GROUP_LENGTH - 1;
      rc->gfu_boost = 1;
      rc->constrained_gf_group = 0;
      rc->frames_till_gf_update_due = rc->baseline_gf_interval;
      for (int layer = 0;
           layer < svc->number_spatial_layers * svc->number_temporal_layers;
           ++layer) {
        LAYER_CONTEXT *const lc = &svc->layer_context[layer];
        lc->rc.baseline_gf_interval = rc->baseline_gf_interval;
        lc->rc.gfu_boost = rc->gfu_boost;
        lc->rc.constrained_gf_group = rc->constrained_gf_group;
        lc->rc.frames_till_gf_update_due = rc->frames_till_gf_update_due;
        lc->group_index = 0;
      }
    }
    gf_group->size = rc->baseline_gf_interval;
    gf_group->update_type[0] =
        (frame_type == KEY_FRAME) ? KF_UPDATE : GF_UPDATE;
    gf_update = 1;
  }
  return gf_update;
}

static void resize_reset_rc(AV1_COMP *cpi, int resize_width, int resize_height,
                            int prev_width, int prev_height) {
  RATE_CONTROL *const rc = &cpi->rc;
  SVC *const svc = &cpi->svc;
  double tot_scale_change = 1.0;
  int target_bits_per_frame;
  int active_worst_quality;
  int qindex;
  tot_scale_change = (double)(resize_width * resize_height) /
                     (double)(prev_width * prev_height);
  // Reset buffer level to optimal, update target size.
  rc->buffer_level = rc->optimal_buffer_level;
  rc->bits_off_target = rc->optimal_buffer_level;
  rc->this_frame_target =
      av1_calc_pframe_target_size_one_pass_cbr(cpi, INTER_FRAME);
  target_bits_per_frame = rc->this_frame_target;
  if (tot_scale_change > 4.0)
    rc->avg_frame_qindex[INTER_FRAME] = rc->worst_quality;
  else if (tot_scale_change > 1.0)
    rc->avg_frame_qindex[INTER_FRAME] =
        (rc->avg_frame_qindex[INTER_FRAME] + rc->worst_quality) >> 1;
  active_worst_quality = calc_active_worst_quality_no_stats_cbr(cpi);
  qindex = av1_rc_regulate_q(cpi, target_bits_per_frame, rc->best_quality,
                             active_worst_quality, resize_width, resize_height);
  // If resize is down, check if projected q index is close to worst_quality,
  // and if so, reduce the rate correction factor (since likely can afford
  // lower q for resized frame).
  if (tot_scale_change < 1.0 && qindex > 90 * cpi->rc.worst_quality / 100)
    rc->rate_correction_factors[INTER_NORMAL] *= 0.85;
  // Apply the same rate control reset to all temporal layers.
  for (int tl = 0; tl < svc->number_temporal_layers; tl++) {
    LAYER_CONTEXT *lc = NULL;
    lc = &svc->layer_context[svc->spatial_layer_id *
                                 svc->number_temporal_layers +
                             tl];
    lc->rc.resize_state = rc->resize_state;
    lc->rc.buffer_level = lc->rc.optimal_buffer_level;
    lc->rc.bits_off_target = lc->rc.optimal_buffer_level;
    lc->rc.rate_correction_factors[INTER_FRAME] =
        rc->rate_correction_factors[INTER_FRAME];
  }
  // If resize is back up: check if projected q index is too much above the
  // previous index, and if so, reduce the rate correction factor
  // (since prefer to keep q for resized frame at least closet to previous q).
  // Also check if projected qindex is close to previous qindex, if so
  // increase correction factor (to push qindex higher and avoid overshoot).
  if (tot_scale_change >= 1.0) {
    if (tot_scale_change < 4.0 && qindex > 130 * rc->last_q[INTER_FRAME] / 100)
      rc->rate_correction_factors[INTER_NORMAL] *= 0.8;
    if (qindex <= 120 * rc->last_q[INTER_FRAME] / 100)
      rc->rate_correction_factors[INTER_NORMAL] *= 2.0;
  }
}

/*!\brief ChecK for resize based on Q, for 1 pass real-time mode.
 *
 * Check if we should resize, based on average QP from past x frames.
 * Only allow for resize at most 1/2 scale down for now, Scaling factor
 * for each step may be 3/4 or 1/2.
 *
 * \ingroup rate_control
 * \param[in]       cpi          Top level encoder structure
 *
 * \return Return resized width/height in \c cpi->resize_pending_params,
 * and update some resize counters in \c rc.
 */
static void dynamic_resize_one_pass_cbr(AV1_COMP *cpi) {
  const AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  RESIZE_ACTION resize_action = NO_RESIZE;
  const int avg_qp_thr1 = 70;
  const int avg_qp_thr2 = 50;
  // Don't allow for resized frame to go below 160x90, resize in steps of 3/4.
  const int min_width = (160 * 4) / 3;
  const int min_height = (90 * 4) / 3;
  int down_size_on = 1;
  // Don't resize on key frame; reset the counters on key frame.
  if (cm->current_frame.frame_type == KEY_FRAME) {
    rc->resize_avg_qp = 0;
    rc->resize_count = 0;
    rc->resize_buffer_underflow = 0;
    return;
  }
  // No resizing down if frame size is below some limit.
  if ((cm->width * cm->height) < min_width * min_height) down_size_on = 0;

  // Resize based on average buffer underflow and QP over some window.
  // Ignore samples close to key frame, since QP is usually high after key.
  if (cpi->rc.frames_since_key > cpi->framerate) {
    const int window = AOMMIN(30, (int)(2 * cpi->framerate));
    rc->resize_avg_qp += rc->last_q[INTER_FRAME];
    if (cpi->rc.buffer_level < (int)(30 * rc->optimal_buffer_level / 100))
      ++rc->resize_buffer_underflow;
    ++rc->resize_count;
    // Check for resize action every "window" frames.
    if (rc->resize_count >= window) {
      int avg_qp = rc->resize_avg_qp / rc->resize_count;
      // Resize down if buffer level has underflowed sufficient amount in past
      // window, and we are at original or 3/4 of original resolution.
      // Resize back up if average QP is low, and we are currently in a resized
      // down state, i.e. 1/2 or 3/4 of original resolution.
      // Currently, use a flag to turn 3/4 resizing feature on/off.
      if (rc->resize_buffer_underflow > (rc->resize_count >> 2) &&
          down_size_on) {
        if (rc->resize_state == THREE_QUARTER) {
          resize_action = DOWN_ONEHALF;
          rc->resize_state = ONE_HALF;
        } else if (rc->resize_state == ORIG) {
          resize_action = DOWN_THREEFOUR;
          rc->resize_state = THREE_QUARTER;
        }
      } else if (rc->resize_state != ORIG &&
                 avg_qp < avg_qp_thr1 * cpi->rc.worst_quality / 100) {
        if (rc->resize_state == THREE_QUARTER ||
            avg_qp < avg_qp_thr2 * cpi->rc.worst_quality / 100) {
          resize_action = UP_ORIG;
          rc->resize_state = ORIG;
        } else if (rc->resize_state == ONE_HALF) {
          resize_action = UP_THREEFOUR;
          rc->resize_state = THREE_QUARTER;
        }
      }
      // Reset for next window measurement.
      rc->resize_avg_qp = 0;
      rc->resize_count = 0;
      rc->resize_buffer_underflow = 0;
    }
  }
  // If decision is to resize, reset some quantities, and check is we should
  // reduce rate correction factor,
  if (resize_action != NO_RESIZE) {
    int resize_width = cpi->oxcf.frm_dim_cfg.width;
    int resize_height = cpi->oxcf.frm_dim_cfg.height;
    int resize_scale_num = 1;
    int resize_scale_den = 1;
    if (resize_action == DOWN_THREEFOUR || resize_action == UP_THREEFOUR) {
      resize_scale_num = 3;
      resize_scale_den = 4;
    } else if (resize_action == DOWN_ONEHALF) {
      resize_scale_num = 1;
      resize_scale_den = 2;
    }
    resize_width = resize_width * resize_scale_num / resize_scale_den;
    resize_height = resize_height * resize_scale_num / resize_scale_den;
    resize_reset_rc(cpi, resize_width, resize_height, cm->width, cm->height);
  }
  return;
}

void av1_get_one_pass_rt_params(AV1_COMP *cpi,
                                EncodeFrameParams *const frame_params,
                                unsigned int frame_flags) {
  RATE_CONTROL *const rc = &cpi->rc;
  AV1_COMMON *const cm = &cpi->common;
  GF_GROUP *const gf_group = &cpi->gf_group;
  SVC *const svc = &cpi->svc;
  ResizePendingParams *const resize_pending_params =
      &cpi->resize_pending_params;
  int target;
  const int layer =
      LAYER_IDS_TO_IDX(svc->spatial_layer_id, svc->temporal_layer_id,
                       svc->number_temporal_layers);
  // Turn this on to explicitly set the reference structure rather than
  // relying on internal/default structure.
  if (cpi->use_svc) {
    av1_update_temporal_layer_framerate(cpi);
    av1_restore_layer_context(cpi);
  }
  // Set frame type.
  if ((!cpi->use_svc && rc->frames_to_key == 0) ||
      (cpi->use_svc && svc->spatial_layer_id == 0 &&
       svc->current_superframe % cpi->oxcf.kf_cfg.key_freq_max == 0) ||
      (frame_flags & FRAMEFLAGS_KEY)) {
    frame_params->frame_type = KEY_FRAME;
    rc->this_key_frame_forced =
        cm->current_frame.frame_number != 0 && rc->frames_to_key == 0;
    rc->frames_to_key = cpi->oxcf.kf_cfg.key_freq_max;
    rc->kf_boost = DEFAULT_KF_BOOST_RT;
    gf_group->update_type[gf_group->index] = KF_UPDATE;
    gf_group->frame_type[gf_group->index] = KEY_FRAME;
    gf_group->refbuf_state[gf_group->index] = REFBUF_RESET;
    if (cpi->use_svc) {
      if (cm->current_frame.frame_number > 0)
        av1_svc_reset_temporal_layers(cpi, 1);
      svc->layer_context[layer].is_key_frame = 1;
    }
  } else {
    frame_params->frame_type = INTER_FRAME;
    gf_group->update_type[gf_group->index] = LF_UPDATE;
    gf_group->frame_type[gf_group->index] = INTER_FRAME;
    gf_group->refbuf_state[gf_group->index] = REFBUF_UPDATE;
    if (cpi->use_svc) {
      LAYER_CONTEXT *lc = &svc->layer_context[layer];
      lc->is_key_frame =
          svc->spatial_layer_id == 0
              ? 0
              : svc->layer_context[svc->temporal_layer_id].is_key_frame;
    }
  }
  // Check for scene change, for non-SVC for now.
  if (!cpi->use_svc && cpi->sf.rt_sf.check_scene_detection)
    rc_scene_detection_onepass_rt(cpi);
  // Check for dynamic resize, for single spatial layer for now.
  // For temporal layers only check on base temporal layer.
  if (cpi->oxcf.resize_cfg.resize_mode == RESIZE_DYNAMIC) {
    if (svc->number_spatial_layers == 1 && svc->temporal_layer_id == 0)
      dynamic_resize_one_pass_cbr(cpi);
    if (rc->resize_state == THREE_QUARTER) {
      resize_pending_params->width = (3 + cpi->oxcf.frm_dim_cfg.width * 3) >> 2;
      resize_pending_params->height =
          (3 + cpi->oxcf.frm_dim_cfg.height * 3) >> 2;
    } else if (rc->resize_state == ONE_HALF) {
      resize_pending_params->width = (1 + cpi->oxcf.frm_dim_cfg.width) >> 1;
      resize_pending_params->height = (1 + cpi->oxcf.frm_dim_cfg.height) >> 1;
    } else {
      resize_pending_params->width = cpi->oxcf.frm_dim_cfg.width;
      resize_pending_params->height = cpi->oxcf.frm_dim_cfg.height;
    }
  } else if (resize_pending_params->width && resize_pending_params->height &&
             (cpi->common.width != resize_pending_params->width ||
              cpi->common.height != resize_pending_params->height)) {
    resize_reset_rc(cpi, resize_pending_params->width,
                    resize_pending_params->height, cm->width, cm->height);
  }
  // Set the GF interval and update flag.
  set_gf_interval_update_onepass_rt(cpi, frame_params->frame_type);
  // Set target size.
  if (cpi->oxcf.rc_cfg.mode == AOM_CBR) {
    if (frame_params->frame_type == KEY_FRAME) {
      target = av1_calc_iframe_target_size_one_pass_cbr(cpi);
    } else {
      target = av1_calc_pframe_target_size_one_pass_cbr(
          cpi, gf_group->update_type[gf_group->index]);
    }
  } else {
    if (frame_params->frame_type == KEY_FRAME) {
      target = av1_calc_iframe_target_size_one_pass_vbr(cpi);
    } else {
      target = av1_calc_pframe_target_size_one_pass_vbr(
          cpi, gf_group->update_type[gf_group->index]);
    }
  }
  av1_rc_set_frame_target(cpi, target, cm->width, cm->height);
  rc->base_frame_target = target;
  cm->current_frame.frame_type = frame_params->frame_type;
}

int av1_encodedframe_overshoot_cbr(AV1_COMP *cpi, int *q) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  SPEED_FEATURES *const sf = &cpi->sf;
  int thresh_qp = 7 * (rc->worst_quality >> 3);
  // Lower thresh_qp for video (more overshoot at lower Q) to be
  // more conservative for video.
  if (cpi->oxcf.tune_cfg.content != AOM_CONTENT_SCREEN)
    thresh_qp = 3 * (rc->worst_quality >> 2);
  if (sf->rt_sf.overshoot_detection_cbr == FAST_DETECTION_MAXQ &&
      cm->quant_params.base_qindex < thresh_qp) {
    double rate_correction_factor =
        cpi->rc.rate_correction_factors[INTER_NORMAL];
    const int target_size = cpi->rc.avg_frame_bandwidth;
    double new_correction_factor;
    int target_bits_per_mb;
    double q2;
    int enumerator;
    *q = (3 * cpi->rc.worst_quality + *q) >> 2;
    // Adjust avg_frame_qindex, buffer_level, and rate correction factors, as
    // these parameters will affect QP selection for subsequent frames. If they
    // have settled down to a very different (low QP) state, then not adjusting
    // them may cause next frame to select low QP and overshoot again.
    cpi->rc.avg_frame_qindex[INTER_FRAME] = *q;
    rc->buffer_level = rc->optimal_buffer_level;
    rc->bits_off_target = rc->optimal_buffer_level;
    // Reset rate under/over-shoot flags.
    cpi->rc.rc_1_frame = 0;
    cpi->rc.rc_2_frame = 0;
    // Adjust rate correction factor.
    target_bits_per_mb =
        (int)(((uint64_t)target_size << BPER_MB_NORMBITS) / cm->mi_params.MBs);
    // Rate correction factor based on target_bits_per_mb and qp (==max_QP).
    // This comes from the inverse computation of vp9_rc_bits_per_mb().
    q2 = av1_convert_qindex_to_q(*q, cm->seq_params.bit_depth);
    enumerator = 1800000;  // Factor for inter frame.
    enumerator += (int)(enumerator * q2) >> 12;
    new_correction_factor = (double)target_bits_per_mb * q2 / enumerator;
    if (new_correction_factor > rate_correction_factor) {
      rate_correction_factor =
          AOMMIN(2.0 * rate_correction_factor, new_correction_factor);
      if (rate_correction_factor > MAX_BPB_FACTOR)
        rate_correction_factor = MAX_BPB_FACTOR;
      cpi->rc.rate_correction_factors[INTER_NORMAL] = rate_correction_factor;
    }
    return 1;
  } else {
    return 0;
  }
}

void av1_compute_frame_low_motion(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  SVC *const svc = &cpi->svc;
  MB_MODE_INFO **mi = mi_params->mi_grid_base;
  RATE_CONTROL *const rc = &cpi->rc;
  const int rows = mi_params->mi_rows, cols = mi_params->mi_cols;
  int cnt_zeromv = 0;
  for (int mi_row = 0; mi_row < rows; mi_row++) {
    for (int mi_col = 0; mi_col < cols; mi_col++) {
      if (mi[0]->ref_frame[0] == LAST_FRAME &&
          abs(mi[0]->mv[0].as_mv.row) < 16 && abs(mi[0]->mv[0].as_mv.col) < 16)
        cnt_zeromv++;
      mi++;
    }
    mi += mi_params->mi_stride - cols;
  }
  cnt_zeromv = 100 * cnt_zeromv / (rows * cols);
  rc->avg_frame_low_motion = (3 * rc->avg_frame_low_motion + cnt_zeromv) >> 2;

  // For SVC: set avg_frame_low_motion (only computed on top spatial layer)
  // to all lower spatial layers.
  if (cpi->use_svc && svc->spatial_layer_id == svc->number_spatial_layers - 1) {
    for (int i = 0; i < svc->number_spatial_layers - 1; ++i) {
      const int layer = LAYER_IDS_TO_IDX(i, svc->temporal_layer_id,
                                         svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;
      lrc->avg_frame_low_motion = rc->avg_frame_low_motion;
    }
  }
}
