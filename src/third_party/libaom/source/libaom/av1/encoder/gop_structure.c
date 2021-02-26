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

// Set parameters for frames between 'start' and 'end' (excluding both).
static void set_multi_layer_params(const TWO_PASS *twopass,
                                   GF_GROUP *const gf_group, RATE_CONTROL *rc,
                                   FRAME_INFO *frame_info, int start, int end,
                                   int *cur_frame_idx, int *frame_ind,
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
          twopass, rc, frame_info, start, end - start, 0, NULL, NULL);
      gf_group->frame_type[*frame_ind] = INTER_FRAME;
      gf_group->refbuf_state[*frame_ind] = REFBUF_UPDATE;
      gf_group->max_layer_depth =
          AOMMAX(gf_group->max_layer_depth, layer_depth);
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

    // Get the boost factor for intermediate ARF frames.
    gf_group->arf_boost[*frame_ind] = av1_calc_arf_boost(
        twopass, rc, frame_info, m, end - m, m - start, NULL, NULL);
    ++(*frame_ind);

    // Frames displayed before this internal ARF.
    set_multi_layer_params(twopass, gf_group, rc, frame_info, start, m,
                           cur_frame_idx, frame_ind, layer_depth + 1);

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
    set_multi_layer_params(twopass, gf_group, rc, frame_info, m + 1, end,
                           cur_frame_idx, frame_ind, layer_depth + 1);
  }
}

static int construct_multi_layer_gf_structure(
    AV1_COMP *cpi, TWO_PASS *twopass, GF_GROUP *const gf_group,
    RATE_CONTROL *rc, FRAME_INFO *const frame_info, int gf_interval,
    FRAME_UPDATE_TYPE first_frame_update_type) {
  int frame_index = 0;
  int cur_frame_index = 0;

  // Keyframe / Overlay frame / Golden frame.
  assert(first_frame_update_type == KF_UPDATE ||
         first_frame_update_type == OVERLAY_UPDATE ||
         first_frame_update_type == GF_UPDATE);

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
    gf_group->arf_boost[frame_index] = cpi->rc.gfu_boost;
    gf_group->frame_type[frame_index] = is_fwd_kf ? KEY_FRAME : INTER_FRAME;
    gf_group->refbuf_state[frame_index] = REFBUF_UPDATE;
    gf_group->max_layer_depth = 1;
    gf_group->arf_index = frame_index;
    ++frame_index;
  } else {
    gf_group->arf_index = -1;
  }

  // Rest of the frames.
  set_multi_layer_params(twopass, gf_group, rc, frame_info, cur_frame_index,
                         gf_interval, &cur_frame_index, &frame_index,
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
  return frame_index;
}

void av1_gop_setup_structure(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  GF_GROUP *const gf_group = &cpi->gf_group;
  TWO_PASS *const twopass = &cpi->twopass;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  const int key_frame = rc->frames_since_key == 0;
  const FRAME_UPDATE_TYPE first_frame_update_type =
      key_frame
          ? KF_UPDATE
          : cpi->gf_state.arf_gf_boost_lst || (rc->baseline_gf_interval == 1)
                ? OVERLAY_UPDATE
                : GF_UPDATE;
  gf_group->size = construct_multi_layer_gf_structure(
      cpi, twopass, gf_group, rc, frame_info, rc->baseline_gf_interval - 1,
      first_frame_update_type);
}
