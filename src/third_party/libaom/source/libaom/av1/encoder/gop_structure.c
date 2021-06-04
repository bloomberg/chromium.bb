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

#include <stdint.h>

#include "av1/common/blockd.h"
#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"

#include "aom_ports/system_state.h"

#include "av1/common/av1_common_int.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/gop_structure.h"

#if CONFIG_FRAME_PARALLEL_ENCODE
// This function sets gf_group->frame_parallel_level for LF_UPDATE frames based
// on the value of parallel_frame_count.
static void set_frame_parallel_level(int *frame_parallel_level,
                                     int *parallel_frame_count,
                                     int max_parallel_frames) {
  assert(*parallel_frame_count > 0);
  // parallel_frame_count > 1 indicates subsequent frame(s) in the current
  // parallel encode set.
  *frame_parallel_level = 1 + (*parallel_frame_count > 1);
  // Update the count of no. of parallel frames.
  (*parallel_frame_count)++;
  if (*parallel_frame_count > max_parallel_frames) *parallel_frame_count = 1;
}
#endif  // CONFIG_FRAME_PARALLEL_ENCODE

// Set parameters for frames between 'start' and 'end' (excluding both).
static void set_multi_layer_params(
    const TWO_PASS *twopass, GF_GROUP *const gf_group,
    const PRIMARY_RATE_CONTROL *p_rc, RATE_CONTROL *rc, FRAME_INFO *frame_info,
    int start, int end, int *cur_frame_idx, int *frame_ind,
#if CONFIG_FRAME_PARALLEL_ENCODE
    int *parallel_frame_count, int max_parallel_frames,
    int do_frame_parallel_encode,
#endif  // CONFIG_FRAME_PARALLEL_ENCODE
    int layer_depth) {
  const int num_frames_to_process = end - start;

  // Either we are at the last level of the pyramid, or we don't have enough
  // frames between 'l' and 'r' to create one more level.
  if (layer_depth > gf_group->max_layer_depth_allowed ||
      num_frames_to_process < 3) {
    // Leaf nodes.
    while (start < end) {
      gf_group->update_type[*frame_ind] = LF_UPDATE;
      gf_group->arf_src_offset[*frame_ind] = 0;
      gf_group->cur_frame_idx[*frame_ind] = *cur_frame_idx;
      gf_group->layer_depth[*frame_ind] = MAX_ARF_LAYERS;
      gf_group->arf_boost[*frame_ind] = av1_calc_arf_boost(
          twopass, p_rc, rc, frame_info, start, end - start, 0, NULL, NULL, 0);
      gf_group->frame_type[*frame_ind] = INTER_FRAME;
      gf_group->refbuf_state[*frame_ind] = REFBUF_UPDATE;
      gf_group->max_layer_depth =
          AOMMAX(gf_group->max_layer_depth, layer_depth);
#if CONFIG_FRAME_PARALLEL_ENCODE
      // Set the level of parallelism for the LF_UPDATE frame.
      if (do_frame_parallel_encode) {
        set_frame_parallel_level(&gf_group->frame_parallel_level[*frame_ind],
                                 parallel_frame_count, max_parallel_frames);
        // Set LF_UPDATE frames as non-reference frames.
        gf_group->is_frame_non_ref[*frame_ind] = 1;
      }
#endif  // CONFIG_FRAME_PARALLEL_ENCODE
      ++(*frame_ind);
      ++(*cur_frame_idx);
      ++start;
    }
  } else {
    const int m = (start + end - 1) / 2;

    // Internal ARF.
    gf_group->update_type[*frame_ind] = INTNL_ARF_UPDATE;
    gf_group->arf_src_offset[*frame_ind] = m - start;
    gf_group->cur_frame_idx[*frame_ind] = *cur_frame_idx;
    gf_group->layer_depth[*frame_ind] = layer_depth;
    gf_group->frame_type[*frame_ind] = INTER_FRAME;
    gf_group->refbuf_state[*frame_ind] = REFBUF_UPDATE;

#if CONFIG_FRAME_PARALLEL_ENCODE
    if (do_frame_parallel_encode) {
      // If max_parallel_frames is not exceeded, encode the next internal ARF
      // frame in parallel.
      if (*parallel_frame_count > 1 &&
          *parallel_frame_count <= max_parallel_frames) {
        gf_group->frame_parallel_level[*frame_ind] = 2;
        *parallel_frame_count = 1;
      }
    }
#endif  // CONFIG_FRAME_PARALLEL_ENCODE

    // Get the boost factor for intermediate ARF frames.
    gf_group->arf_boost[*frame_ind] = av1_calc_arf_boost(
        twopass, p_rc, rc, frame_info, m, end - m, m - start, NULL, NULL, 0);
    ++(*frame_ind);

    // Frames displayed before this internal ARF.
    set_multi_layer_params(twopass, gf_group, p_rc, rc, frame_info, start, m,
                           cur_frame_idx, frame_ind,
#if CONFIG_FRAME_PARALLEL_ENCODE
                           parallel_frame_count, max_parallel_frames,
                           do_frame_parallel_encode,
#endif  // CONFIG_FRAME_PARALLEL_ENCODE
                           layer_depth + 1);

    // Overlay for internal ARF.
    gf_group->update_type[*frame_ind] = INTNL_OVERLAY_UPDATE;
    gf_group->arf_src_offset[*frame_ind] = 0;
    gf_group->cur_frame_idx[*frame_ind] = *cur_frame_idx;
    gf_group->arf_boost[*frame_ind] = 0;
    gf_group->layer_depth[*frame_ind] = layer_depth;
    gf_group->frame_type[*frame_ind] = INTER_FRAME;
    gf_group->refbuf_state[*frame_ind] = REFBUF_UPDATE;
    ++(*frame_ind);
    ++(*cur_frame_idx);

    // Frames displayed after this internal ARF.
    set_multi_layer_params(twopass, gf_group, p_rc, rc, frame_info, m + 1, end,
                           cur_frame_idx, frame_ind,
#if CONFIG_FRAME_PARALLEL_ENCODE
                           parallel_frame_count, max_parallel_frames,
                           do_frame_parallel_encode,
#endif  // CONFIG_FRAME_PARALLEL_ENCODE
                           layer_depth + 1);
  }
}

static int construct_multi_layer_gf_structure(
    AV1_COMP *cpi, TWO_PASS *twopass, GF_GROUP *const gf_group,
    RATE_CONTROL *rc, FRAME_INFO *const frame_info, int gf_interval,
    FRAME_UPDATE_TYPE first_frame_update_type) {
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  int frame_index = 0;
  int cur_frame_index = 0;

  // Keyframe / Overlay frame / Golden frame.
  assert(first_frame_update_type == KF_UPDATE ||
         first_frame_update_type == OVERLAY_UPDATE ||
         first_frame_update_type == GF_UPDATE);

#if CONFIG_FRAME_PARALLEL_ENCODE
  // Initialize gf_group->frame_parallel_level and gf_group->is_frame_non_ref to
  // 0.
  memset(
      gf_group->frame_parallel_level, 0,
      sizeof(gf_group->frame_parallel_level[0]) * MAX_STATIC_GF_GROUP_LENGTH);
  memset(gf_group->is_frame_non_ref, 0,
         sizeof(gf_group->is_frame_non_ref[0]) * MAX_STATIC_GF_GROUP_LENGTH);
#endif

  if (first_frame_update_type == KF_UPDATE &&
      cpi->oxcf.kf_cfg.enable_keyframe_filtering > 1) {
    gf_group->update_type[frame_index] = ARF_UPDATE;
    gf_group->arf_src_offset[frame_index] = 0;
    gf_group->cur_frame_idx[frame_index] = cur_frame_index;
    gf_group->layer_depth[frame_index] = 0;
    gf_group->frame_type[frame_index] = KEY_FRAME;
    gf_group->refbuf_state[frame_index] = REFBUF_RESET;
    gf_group->max_layer_depth = 0;
    ++frame_index;

    gf_group->update_type[frame_index] = OVERLAY_UPDATE;
    gf_group->arf_src_offset[frame_index] = 0;
    gf_group->cur_frame_idx[frame_index] = cur_frame_index;
    gf_group->layer_depth[frame_index] = 0;
    gf_group->frame_type[frame_index] = INTER_FRAME;
    gf_group->refbuf_state[frame_index] = REFBUF_UPDATE;
    gf_group->max_layer_depth = 0;
    ++frame_index;
    cur_frame_index++;
  } else if (first_frame_update_type != OVERLAY_UPDATE) {
    gf_group->update_type[frame_index] = first_frame_update_type;
    gf_group->arf_src_offset[frame_index] = 0;
    gf_group->cur_frame_idx[frame_index] = cur_frame_index;
    gf_group->layer_depth[frame_index] =
        first_frame_update_type == OVERLAY_UPDATE ? MAX_ARF_LAYERS + 1 : 0;
    gf_group->frame_type[frame_index] =
        (first_frame_update_type == KF_UPDATE) ? KEY_FRAME : INTER_FRAME;
    gf_group->refbuf_state[frame_index] =
        (first_frame_update_type == KF_UPDATE) ? REFBUF_RESET : REFBUF_UPDATE;
    gf_group->max_layer_depth = 0;
    ++frame_index;
    ++cur_frame_index;
  }

  // ALTREF.
  const int use_altref = gf_group->max_layer_depth_allowed > 0;
  int is_fwd_kf = (gf_interval == cpi->rc.frames_to_key);
  if (use_altref) {
    gf_group->update_type[frame_index] = ARF_UPDATE;
    gf_group->arf_src_offset[frame_index] = gf_interval - cur_frame_index;
    gf_group->cur_frame_idx[frame_index] = cur_frame_index;
    gf_group->layer_depth[frame_index] = 1;
    gf_group->arf_boost[frame_index] = cpi->ppi->p_rc.gfu_boost;
    gf_group->frame_type[frame_index] = is_fwd_kf ? KEY_FRAME : INTER_FRAME;
    gf_group->refbuf_state[frame_index] = REFBUF_UPDATE;
    gf_group->max_layer_depth = 1;
    gf_group->arf_index = frame_index;
    ++frame_index;
  } else {
    gf_group->arf_index = -1;
  }

#if CONFIG_FRAME_PARALLEL_ENCODE
  // Running count of no. of frames that is part of a given parallel
  // encode set in a gf_group. Value of 1 indicates no parallel encode.
  int parallel_frame_count = 1;
  // Enable parallel encode of frames if gf_group has a multi-layer pyramid
  // structure.
  int do_frame_parallel_encode = (cpi->ppi->num_fp_contexts > 1 && use_altref);
#endif  // CONFIG_FRAME_PARALLEL_ENCODE

  // Rest of the frames.
  set_multi_layer_params(twopass, gf_group, p_rc, rc, frame_info,
                         cur_frame_index, gf_interval, &cur_frame_index,
                         &frame_index,
#if CONFIG_FRAME_PARALLEL_ENCODE
                         &parallel_frame_count, cpi->ppi->num_fp_contexts,
                         do_frame_parallel_encode,
#endif  // CONFIG_FRAME_PARALLEL_ENCODE
                         use_altref + 1);

  if (use_altref) {
    gf_group->update_type[frame_index] = OVERLAY_UPDATE;
    gf_group->arf_src_offset[frame_index] = 0;
    gf_group->cur_frame_idx[frame_index] = cur_frame_index;
    gf_group->layer_depth[frame_index] = MAX_ARF_LAYERS;
    gf_group->arf_boost[frame_index] = NORMAL_BOOST;
    gf_group->frame_type[frame_index] = is_fwd_kf ? KEY_FRAME : INTER_FRAME;
    gf_group->refbuf_state[frame_index] =
        is_fwd_kf ? REFBUF_RESET : REFBUF_UPDATE;
    ++frame_index;
  } else {
    for (; cur_frame_index <= gf_interval; ++cur_frame_index) {
      gf_group->update_type[frame_index] = LF_UPDATE;
      gf_group->arf_src_offset[frame_index] = 0;
      gf_group->cur_frame_idx[frame_index] = cur_frame_index;
      gf_group->layer_depth[frame_index] = MAX_ARF_LAYERS;
      gf_group->arf_boost[frame_index] = NORMAL_BOOST;
      gf_group->frame_type[frame_index] = INTER_FRAME;
      gf_group->refbuf_state[frame_index] = REFBUF_UPDATE;
      gf_group->max_layer_depth = AOMMAX(gf_group->max_layer_depth, 2);
      ++frame_index;
    }
  }
#if CONFIG_FRAME_PARALLEL_ENCODE
  if (do_frame_parallel_encode) {
    // If frame_parallel_level is set to 1 for the last LF_UPDATE
    // frame in the gf_group, reset it to zero since there are no subsequent
    // frames in the gf_group.
    if (gf_group->frame_parallel_level[frame_index - 2] == 1) {
      assert(gf_group->update_type[frame_index - 2] == LF_UPDATE);
      gf_group->frame_parallel_level[frame_index - 2] = 0;
    }
  }
#endif
  return frame_index;
}

void av1_gop_setup_structure(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  GF_GROUP *const gf_group = &cpi->ppi->gf_group;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  const int key_frame = rc->frames_since_key == 0;
  const FRAME_UPDATE_TYPE first_frame_update_type =
      key_frame ? KF_UPDATE
                : cpi->ppi->gf_state.arf_gf_boost_lst ||
                          (p_rc->baseline_gf_interval == 1)
                      ? OVERLAY_UPDATE
                      : GF_UPDATE;
  gf_group->size = construct_multi_layer_gf_structure(
      cpi, twopass, gf_group, rc, frame_info, p_rc->baseline_gf_interval - 1,
      first_frame_update_type);
}
