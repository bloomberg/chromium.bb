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
#include <limits.h>

#include "config/aom_config.h"

#include "av1/common/alloccommon.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/odintrin.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/temporal_filter.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"
#include "aom_scale/aom_scale.h"

/*!\cond */

// NOTE: All `tf` in this file means `temporal filtering`.

// Allocates memory for members of TemporalFilterData.
// Inputs:
//   tf_data: Pointer to the structure containing temporal filter related data.
//   num_pels: Number of pixels in the block across all planes.
//   is_high_bitdepth: Whether the frame is high-bitdepth or not.
// Returns:
//   Nothing will be returned. But the contents of tf_data will be modified.
static AOM_INLINE void tf_alloc_and_reset_data(TemporalFilterData *tf_data,
                                               int num_pels,
                                               int is_high_bitdepth) {
  tf_data->tmp_mbmi = (MB_MODE_INFO *)malloc(sizeof(MB_MODE_INFO));
  memset(tf_data->tmp_mbmi, 0, sizeof(*tf_data->tmp_mbmi));
  tf_data->accum = aom_memalign(16, num_pels * sizeof(uint32_t));
  tf_data->count = aom_memalign(16, num_pels * sizeof(uint16_t));
  memset(&tf_data->diff, 0, sizeof(tf_data->diff));
  if (is_high_bitdepth)
    tf_data->pred =
        CONVERT_TO_BYTEPTR(aom_memalign(32, num_pels * sizeof(uint16_t)));
  else
    tf_data->pred = aom_memalign(32, num_pels * sizeof(uint8_t));
}

// Setup macroblockd params for temporal filtering process.
// Inputs:
//   mbd: Pointer to the block for filtering.
//   tf_data: Pointer to the structure containing temporal filter related data.
//   scale: Scaling factor.
// Returns:
//   Nothing will be returned. Contents of mbd will be modified.
static AOM_INLINE void tf_setup_macroblockd(MACROBLOCKD *mbd,
                                            TemporalFilterData *tf_data,
                                            const struct scale_factors *scale) {
  mbd->block_ref_scale_factors[0] = scale;
  mbd->block_ref_scale_factors[1] = scale;
  mbd->mi = &tf_data->tmp_mbmi;
  mbd->mi[0]->motion_mode = SIMPLE_TRANSLATION;
}

// Saves the state prior to temporal filter process.
// Inputs:
//   mbd: Pointer to the block for filtering.
//   input_mbmi: Backup block info to save input state.
//   input_buffer: Backup buffer pointer to save input state.
//   num_planes: Number of planes.
// Returns:
//   Nothing will be returned. Contents of input_mbmi and input_buffer will be
//   modified.
static void tf_save_state(MACROBLOCKD *mbd, MB_MODE_INFO ***input_mbmi,
                          uint8_t **input_buffer, int num_planes) {
  for (int i = 0; i < num_planes; i++) {
    input_buffer[i] = mbd->plane[i].pre[0].buf;
  }
  *input_mbmi = mbd->mi;
}

// Restores the initial state after temporal filter process.
// Inputs:
//   mbd: Pointer to the block for filtering.
//   input_mbmi: Backup block info from where input state is restored.
//   input_buffer: Backup buffer pointer from where input state is restored.
//   num_planes: Number of planes.
// Returns:
//   Nothing will be returned. Contents of mbd will be modified.
static void tf_restore_state(MACROBLOCKD *mbd, MB_MODE_INFO **input_mbmi,
                             uint8_t **input_buffer, int num_planes) {
  for (int i = 0; i < num_planes; i++) {
    mbd->plane[i].pre[0].buf = input_buffer[i];
  }
  mbd->mi = input_mbmi;
}

// Deallocates the memory allocated for members of TemporalFilterData.
// Inputs:
//   tf_data: Pointer to the structure containing temporal filter related data.
//   is_high_bitdepth: Whether the frame is high-bitdepth or not.
// Returns:
//   Nothing will be returned.
static AOM_INLINE void tf_dealloc_data(TemporalFilterData *tf_data,
                                       int is_high_bitdepth) {
  if (is_high_bitdepth)
    tf_data->pred = (uint8_t *)CONVERT_TO_SHORTPTR(tf_data->pred);
  free(tf_data->tmp_mbmi);
  aom_free(tf_data->accum);
  aom_free(tf_data->count);
  aom_free(tf_data->pred);
}

// Forward Declaration.
static void tf_determine_block_partition(const MV block_mv, const int block_mse,
                                         MV *subblock_mvs, int *subblock_mses);

/*!\endcond */
/*!\brief Does motion search for blocks in temporal filtering. This is
 *  the first step for temporal filtering. More specifically, given a frame to
 * be filtered and another frame as reference, this function searches the
 * reference frame to find out the most similar block as that from the frame
 * to be filtered. This found block will be further used for weighted
 * averaging.
 *
 * NOTE: Besides doing motion search for the entire block, this function will
 *       also do motion search for each 1/4 sub-block to get more precise
 *       predictions. Then, this function will determines whether to use 4
 *       sub-blocks to replace the entire block. If we do need to split the
 *       entire block, 4 elements in `subblock_mvs` and `subblock_mses` refer to
 *       the searched motion vector and search error (MSE) w.r.t. each sub-block
 *       respectively. Otherwise, the 4 elements will be the same, all of which
 *       are assigned as the searched motion vector and search error (MSE) for
 *       the entire block.
 *
 * \ingroup src_frame_proc
 * \param[in]   cpi             Top level encoder instance structure
 * \param[in]   mb              Pointer to macroblock
 * \param[in]   frame_to_filter Pointer to the frame to be filtered
 * \param[in]   ref_frame       Pointer to the reference frame
 * \param[in]   block_size      Block size used for motion search
 * \param[in]   mb_row          Row index of the block in the frame
 * \param[in]   mb_col          Column index of the block in the frame
 * \param[in]   ref_mv          Reference motion vector, which is commonly
 *                              inherited from the motion search result of
 *                              previous frame.
 * \param[out]  subblock_mvs    Pointer to the motion vectors for 4 sub-blocks
 * \param[out]  subblock_mses   Pointer to the search errors (MSE) for 4
 *                              sub-blocks
 *
 * \return Nothing will be returned. Results are saved in subblock_mvs and
 *         subblock_mses
 */
static void tf_motion_search(AV1_COMP *cpi, MACROBLOCK *mb,
                             const YV12_BUFFER_CONFIG *frame_to_filter,
                             const YV12_BUFFER_CONFIG *ref_frame,
                             const BLOCK_SIZE block_size, const int mb_row,
                             const int mb_col, MV *ref_mv, MV *subblock_mvs,
                             int *subblock_mses) {
  // Frame information
  const int min_frame_size = AOMMIN(cpi->common.width, cpi->common.height);

  // Block information (ONLY Y-plane is used for motion search).
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_pels = mb_height * mb_width;
  const int y_stride = frame_to_filter->y_stride;
  assert(y_stride == ref_frame->y_stride);
  const int y_offset = mb_row * mb_height * y_stride + mb_col * mb_width;

  // Save input state.
  MACROBLOCKD *const mbd = &mb->e_mbd;
  const struct buf_2d ori_src_buf = mb->plane[0].src;
  const struct buf_2d ori_pre_buf = mbd->plane[0].pre[0];

  // Parameters used for motion search.
  FULLPEL_MOTION_SEARCH_PARAMS full_ms_params;
  SUBPEL_MOTION_SEARCH_PARAMS ms_params;
  const SEARCH_METHODS search_method = NSTEP;
  const search_site_config *search_site_cfg =
      cpi->mv_search_params.search_site_cfg[SS_CFG_LOOKAHEAD];
  const int step_param = av1_init_search_range(
      AOMMAX(frame_to_filter->y_crop_width, frame_to_filter->y_crop_height));
  const SUBPEL_SEARCH_TYPE subpel_search_type = USE_8_TAPS;
  const int force_integer_mv = cpi->common.features.cur_frame_force_integer_mv;
  const MV_COST_TYPE mv_cost_type =
      min_frame_size >= 720
          ? MV_COST_L1_HDRES
          : (min_frame_size >= 480 ? MV_COST_L1_MIDRES : MV_COST_L1_LOWRES);

  // Starting position for motion search.
  FULLPEL_MV start_mv = get_fullmv_from_mv(ref_mv);
  // Baseline position for motion search (used for rate distortion comparison).
  const MV baseline_mv = kZeroMv;

  // Setup.
  mb->plane[0].src.buf = frame_to_filter->y_buffer + y_offset;
  mb->plane[0].src.stride = y_stride;
  mbd->plane[0].pre[0].buf = ref_frame->y_buffer + y_offset;
  mbd->plane[0].pre[0].stride = y_stride;
  // Unused intermediate results for motion search.
  unsigned int sse, error;
  int distortion;
  int cost_list[5];

  // Do motion search.
  int_mv best_mv;  // Searched motion vector.
  int block_mse = INT_MAX;
  MV block_mv = kZeroMv;

  av1_make_default_fullpel_ms_params(&full_ms_params, cpi, mb, block_size,
                                     &baseline_mv, search_site_cfg,
                                     /*fine_search_interval=*/0);
  av1_set_mv_search_method(&full_ms_params, search_site_cfg, search_method);
  full_ms_params.run_mesh_search = 1;
  full_ms_params.mv_cost_params.mv_cost_type = mv_cost_type;

  av1_full_pixel_search(start_mv, &full_ms_params, step_param,
                        cond_cost_list(cpi, cost_list), &best_mv.as_fullmv,
                        NULL);

  if (force_integer_mv == 1) {  // Only do full search on the entire block.
    const int mv_row = best_mv.as_mv.row;
    const int mv_col = best_mv.as_mv.col;
    best_mv.as_mv.row = GET_MV_SUBPEL(mv_row);
    best_mv.as_mv.col = GET_MV_SUBPEL(mv_col);
    const int mv_offset = mv_row * y_stride + mv_col;
    error = cpi->fn_ptr[block_size].vf(
        ref_frame->y_buffer + y_offset + mv_offset, y_stride,
        frame_to_filter->y_buffer + y_offset, y_stride, &sse);
    block_mse = DIVIDE_AND_ROUND(error, mb_pels);
    block_mv = best_mv.as_mv;
  } else {  // Do fractional search on the entire block and all sub-blocks.
    av1_make_default_subpel_ms_params(&ms_params, cpi, mb, block_size,
                                      &baseline_mv, cost_list);
    ms_params.forced_stop = EIGHTH_PEL;
    ms_params.var_params.subpel_search_type = subpel_search_type;
    // Since we are merely refining the result from full pixel search, we don't
    // need regularization for subpel search
    ms_params.mv_cost_params.mv_cost_type = MV_COST_NONE;

    MV subpel_start_mv = get_mv_from_fullmv(&best_mv.as_fullmv);
    error = cpi->mv_search_params.find_fractional_mv_step(
        &mb->e_mbd, &cpi->common, &ms_params, subpel_start_mv, &best_mv.as_mv,
        &distortion, &sse, NULL);
    block_mse = DIVIDE_AND_ROUND(error, mb_pels);
    block_mv = best_mv.as_mv;
    *ref_mv = best_mv.as_mv;
    // On 4 sub-blocks.
    const BLOCK_SIZE subblock_size = ss_size_lookup[block_size][1][1];
    const int subblock_height = block_size_high[subblock_size];
    const int subblock_width = block_size_wide[subblock_size];
    const int subblock_pels = subblock_height * subblock_width;
    start_mv = get_fullmv_from_mv(ref_mv);

    int subblock_idx = 0;
    for (int i = 0; i < mb_height; i += subblock_height) {
      for (int j = 0; j < mb_width; j += subblock_width) {
        const int offset = i * y_stride + j;
        mb->plane[0].src.buf = frame_to_filter->y_buffer + y_offset + offset;
        mbd->plane[0].pre[0].buf = ref_frame->y_buffer + y_offset + offset;
        av1_make_default_fullpel_ms_params(&full_ms_params, cpi, mb,
                                           subblock_size, &baseline_mv,
                                           search_site_cfg,
                                           /*fine_search_interval=*/0);
        av1_set_mv_search_method(&full_ms_params, search_site_cfg,
                                 search_method);
        full_ms_params.run_mesh_search = 1;
        full_ms_params.mv_cost_params.mv_cost_type = mv_cost_type;

        av1_full_pixel_search(start_mv, &full_ms_params, step_param,
                              cond_cost_list(cpi, cost_list),
                              &best_mv.as_fullmv, NULL);

        av1_make_default_subpel_ms_params(&ms_params, cpi, mb, subblock_size,
                                          &baseline_mv, cost_list);
        ms_params.forced_stop = EIGHTH_PEL;
        ms_params.var_params.subpel_search_type = subpel_search_type;
        // Since we are merely refining the result from full pixel search, we
        // don't need regularization for subpel search
        ms_params.mv_cost_params.mv_cost_type = MV_COST_NONE;

        subpel_start_mv = get_mv_from_fullmv(&best_mv.as_fullmv);
        error = cpi->mv_search_params.find_fractional_mv_step(
            &mb->e_mbd, &cpi->common, &ms_params, subpel_start_mv,
            &best_mv.as_mv, &distortion, &sse, NULL);
        subblock_mses[subblock_idx] = DIVIDE_AND_ROUND(error, subblock_pels);
        subblock_mvs[subblock_idx] = best_mv.as_mv;
        ++subblock_idx;
      }
    }
  }

  // Restore input state.
  mb->plane[0].src = ori_src_buf;
  mbd->plane[0].pre[0] = ori_pre_buf;

  // Make partition decision.
  tf_determine_block_partition(block_mv, block_mse, subblock_mvs,
                               subblock_mses);

  // Do not pass down the reference motion vector if error is too large.
  const int thresh = (min_frame_size >= 720) ? 12 : 3;
  if (block_mse > (thresh << (mbd->bd - 8))) {
    *ref_mv = kZeroMv;
  }
}
/*!\cond */

// Determines whether to split the entire block to 4 sub-blocks for filtering.
// In particular, this decision is made based on the comparison between the
// motion search error of the entire block and the errors of all sub-blocks.
// Inputs:
//   block_mv: Motion vector for the entire block (ONLY as reference).
//   block_mse: Motion search error (MSE) for the entire block (ONLY as
//              reference).
//   subblock_mvs: Pointer to the motion vectors for 4 sub-blocks (will be
//                 modified based on the partition decision).
//   subblock_mses: Pointer to the search errors (MSE) for 4 sub-blocks (will
//                  be modified based on the partition decision).
// Returns:
//   Nothing will be returned. Results are saved in `subblock_mvs` and
//   `subblock_mses`.
static void tf_determine_block_partition(const MV block_mv, const int block_mse,
                                         MV *subblock_mvs, int *subblock_mses) {
  int min_subblock_mse = INT_MAX;
  int max_subblock_mse = INT_MIN;
  int sum_subblock_mse = 0;
  for (int i = 0; i < 4; ++i) {
    sum_subblock_mse += subblock_mses[i];
    min_subblock_mse = AOMMIN(min_subblock_mse, subblock_mses[i]);
    max_subblock_mse = AOMMAX(max_subblock_mse, subblock_mses[i]);
  }

  // TODO(any): The following magic numbers may be tuned to improve the
  // performance OR find a way to get rid of these magic numbers.
  if (((block_mse * 15 < sum_subblock_mse * 4) &&
       max_subblock_mse - min_subblock_mse < 48) ||
      ((block_mse * 14 < sum_subblock_mse * 4) &&
       max_subblock_mse - min_subblock_mse < 24)) {  // No split.
    for (int i = 0; i < 4; ++i) {
      subblock_mvs[i] = block_mv;
      subblock_mses[i] = block_mse;
    }
  }
}

// Helper function to determine whether a frame is encoded with high bit-depth.
static INLINE int is_frame_high_bitdepth(const YV12_BUFFER_CONFIG *frame) {
  return (frame->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
}

/*!\endcond */
/*!\brief Builds predictor for blocks in temporal filtering. This is the
 * second step for temporal filtering, which is to construct predictions from
 * all reference frames INCLUDING the frame to be filtered itself. These
 * predictors are built based on the motion search results (motion vector is
 * set as 0 for the frame to be filtered), and will be futher used for
 * weighted averaging.
 *
 * \ingroup src_frame_proc
 * \param[in]   ref_frame      Pointer to the reference frame (or the frame
 *                             to be filtered)
 * \param[in]   mbd            Pointer to the block for filtering. Besides
 *                             containing the subsampling information of all
 *                             planes, this field also gives the searched
 *                             motion vector for the entire block, i.e.,
 *                             `mbd->mi[0]->mv[0]`. This vector  should be 0
 *                             if the `ref_frame` itself is the frame to be
 *                             filtered.
 * \param[in]   block_size     Size of the block
 * \param[in]   mb_row         Row index of the block in the frame
 * \param[in]   mb_col         Column index of the block in the frame
 * \param[in]   num_planes     Number of planes in the frame
 * \param[in]   scale          Scaling factor
 * \param[in]   subblock_mvs   The motion vectors for each sub-block (row-major
 *                             order)
 * \param[out]  pred           Pointer to the predictor to be built
 *
 * \return Nothing returned, But the contents of `pred` will be modified
 */
static void tf_build_predictor(const YV12_BUFFER_CONFIG *ref_frame,
                               const MACROBLOCKD *mbd,
                               const BLOCK_SIZE block_size, const int mb_row,
                               const int mb_col, const int num_planes,
                               const struct scale_factors *scale,
                               const MV *subblock_mvs, uint8_t *pred) {
  // Information of the entire block.
  const int mb_height = block_size_high[block_size];  // Height.
  const int mb_width = block_size_wide[block_size];   // Width.
  const int mb_y = mb_height * mb_row;                // Y-coord (Top-left).
  const int mb_x = mb_width * mb_col;                 // X-coord (Top-left).
  const int bit_depth = mbd->bd;                      // Bit depth.
  const int is_intrabc = 0;                           // Is intra-copied?
  const int is_high_bitdepth = is_frame_high_bitdepth(ref_frame);

  // Default interpolation filters.
  const int_interpfilters interp_filters =
      av1_broadcast_interp_filter(MULTITAP_SHARP2);

  // Handle Y-plane, U-plane and V-plane (if needed) in sequence.
  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const int subsampling_y = mbd->plane[plane].subsampling_y;
    const int subsampling_x = mbd->plane[plane].subsampling_x;
    // Information of each sub-block in current plane.
    const int plane_h = mb_height >> subsampling_y;  // Plane height.
    const int plane_w = mb_width >> subsampling_x;   // Plane width.
    const int plane_y = mb_y >> subsampling_y;       // Y-coord (Top-left).
    const int plane_x = mb_x >> subsampling_x;       // X-coord (Top-left).
    const int h = plane_h >> 1;                      // Sub-block height.
    const int w = plane_w >> 1;                      // Sub-block width.
    const int is_y_plane = (plane == 0);             // Is Y-plane?

    const struct buf_2d ref_buf = { NULL, ref_frame->buffers[plane],
                                    ref_frame->widths[is_y_plane ? 0 : 1],
                                    ref_frame->heights[is_y_plane ? 0 : 1],
                                    ref_frame->strides[is_y_plane ? 0 : 1] };

    // Handle each subblock.
    int subblock_idx = 0;
    for (int i = 0; i < plane_h; i += h) {
      for (int j = 0; j < plane_w; j += w) {
        // Choose proper motion vector.
        const MV mv = subblock_mvs[subblock_idx++];
        assert(mv.row >= INT16_MIN && mv.row <= INT16_MAX &&
               mv.col >= INT16_MIN && mv.col <= INT16_MAX);

        const int y = plane_y + i;
        const int x = plane_x + j;

        // Build predictior for each sub-block on current plane.
        InterPredParams inter_pred_params;
        av1_init_inter_params(&inter_pred_params, w, h, y, x, subsampling_x,
                              subsampling_y, bit_depth, is_high_bitdepth,
                              is_intrabc, scale, &ref_buf, interp_filters);
        inter_pred_params.conv_params = get_conv_params(0, plane, bit_depth);
        av1_enc_build_one_inter_predictor(&pred[plane_offset + i * plane_w + j],
                                          plane_w, &mv, &inter_pred_params);
      }
    }
    plane_offset += plane_h * plane_w;
  }
}
/*!\cond */

// Computes temporal filter weights and accumulators for the frame to be
// filtered. More concretely, the filter weights for all pixels are the same.
// Inputs:
//   mbd: Pointer to the block for filtering, which is ONLY used to get
//        subsampling information of all planes as well as the bit-depth.
//   block_size: Size of the block.
//   num_planes: Number of planes in the frame.
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void tf_apply_temporal_filter_self(const YV12_BUFFER_CONFIG *ref_frame,
                                   const MACROBLOCKD *mbd,
                                   const BLOCK_SIZE block_size,
                                   const int mb_row, const int mb_col,
                                   const int num_planes, uint32_t *accum,
                                   uint16_t *count) {
  // Block information.
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int is_high_bitdepth = is_cur_buf_hbd(mbd);

  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const int subsampling_y = mbd->plane[plane].subsampling_y;
    const int subsampling_x = mbd->plane[plane].subsampling_x;
    const int h = mb_height >> subsampling_y;  // Plane height.
    const int w = mb_width >> subsampling_x;   // Plane width.

    const int frame_stride = ref_frame->strides[plane == AOM_PLANE_Y ? 0 : 1];
    const uint8_t *buf8 = ref_frame->buffers[plane];
    const uint16_t *buf16 = CONVERT_TO_SHORTPTR(buf8);
    const int frame_offset = mb_row * h * frame_stride + mb_col * w;

    int pred_idx = 0;
    int pixel_idx = 0;
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        const int idx = plane_offset + pred_idx;  // Index with plane shift.
        const int pred_value = is_high_bitdepth
                                   ? buf16[frame_offset + pixel_idx]
                                   : buf8[frame_offset + pixel_idx];
        accum[idx] += TF_WEIGHT_SCALE * pred_value;
        count[idx] += TF_WEIGHT_SCALE;
        ++pred_idx;
        ++pixel_idx;
      }
      pixel_idx += (frame_stride - w);
    }
    plane_offset += h * w;
  }
}

// Function to compute pixel-wise squared difference between two buffers.
// Inputs:
//   ref: Pointer to reference buffer.
//   ref_offset: Start position of reference buffer for computation.
//   ref_stride: Stride for reference buffer.
//   tgt: Pointer to target buffer.
//   tgt_offset: Start position of target buffer for computation.
//   tgt_stride: Stride for target buffer.
//   height: Height of block for computation.
//   width: Width of block for computation.
//   is_high_bitdepth: Whether the two buffers point to high bit-depth frames.
//   square_diff: Pointer to save the squared differces.
// Returns:
//   Nothing will be returned. But the content to which `square_diff` points
//   will be modified.
static INLINE void compute_square_diff(const uint8_t *ref, const int ref_offset,
                                       const int ref_stride, const uint8_t *tgt,
                                       const int tgt_offset,
                                       const int tgt_stride, const int height,
                                       const int width,
                                       const int is_high_bitdepth,
                                       uint32_t *square_diff) {
  const uint16_t *ref16 = CONVERT_TO_SHORTPTR(ref);
  const uint16_t *tgt16 = CONVERT_TO_SHORTPTR(tgt);

  int ref_idx = 0;
  int tgt_idx = 0;
  int idx = 0;
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const uint16_t ref_value = is_high_bitdepth ? ref16[ref_offset + ref_idx]
                                                  : ref[ref_offset + ref_idx];
      const uint16_t tgt_value = is_high_bitdepth ? tgt16[tgt_offset + tgt_idx]
                                                  : tgt[tgt_offset + tgt_idx];
      const uint32_t diff = (ref_value > tgt_value) ? (ref_value - tgt_value)
                                                    : (tgt_value - ref_value);
      square_diff[idx] = diff * diff;

      ++ref_idx;
      ++tgt_idx;
      ++idx;
    }
    ref_idx += (ref_stride - width);
    tgt_idx += (tgt_stride - width);
  }
}

// Function to accumulate pixel-wise squared difference between two luma buffers
// to be consumed while filtering the chroma planes.
// Inputs:
//   square_diff: Pointer to squared differences from luma plane.
//   luma_sse_sum: Pointer to save the sum of luma squared differences.
//   block_height: Height of block for computation.
//   block_width: Width of block for computation.
//   ss_x_shift: Chroma subsampling shift in 'X' direction
//   ss_y_shift: Chroma subsampling shift in 'Y' direction
// Returns:
//   Nothing will be returned. But the content to which `luma_sse_sum` points
//   will be modified.
void compute_luma_sq_error_sum(uint32_t *square_diff, uint32_t *luma_sse_sum,
                               int block_height, int block_width,
                               int ss_x_shift, int ss_y_shift) {
  for (int i = 0; i < block_height; ++i) {
    for (int j = 0; j < block_width; ++j) {
      for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
        for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
          const int yy = (i << ss_y_shift) + ii;     // Y-coord on Y-plane.
          const int xx = (j << ss_x_shift) + jj;     // X-coord on Y-plane.
          const int ww = block_width << ss_x_shift;  // Width of Y-plane.
          luma_sse_sum[i * block_width + j] += square_diff[yy * ww + xx];
        }
      }
    }
  }
}

/*!\endcond */
/*!\brief Applies temporal filtering. NOTE that there are various optimised
 * versions of this function called where the appropriate instruction set is
 * supported.
 *
 * \ingroup src_frame_proc
 * \param[in]   frame_to_filter Pointer to the frame to be filtered, which is
 *                              used as reference to compute squared
 *                              difference from the predictor.
 * \param[in]   mbd             Pointer to the block for filtering, ONLY used
 *                              to get subsampling information for the  planes
 * \param[in]   block_size      Size of the block
 * \param[in]   mb_row          Row index of the block in the frame
 * \param[in]   mb_col          Column index of the block in the frame
 * \param[in]   num_planes      Number of planes in the frame
 * \param[in]   noise_levels    Estimated noise levels for each plane
 *                              in the frame (Y,U,V)
 * \param[in]   subblock_mvs    Pointer to the motion vectors for 4 sub-blocks
 * \param[in]   subblock_mses   Pointer to the search errors (MSE) for 4
 *                              sub-blocks
 * \param[in]   q_factor        Quantization factor. This is actually the `q`
 *                              defined in libaom, converted from `qindex`
 * \param[in]   filter_strength Filtering strength. This value lies in range
 *                              [0, 6] where 6 is the maximum strength.
 * \param[out]  pred            Pointer to the well-built predictors
 * \param[out]  accum           Pointer to the pixel-wise accumulator for
 *                              filtering
 * \param[out]  count           Pointer to the pixel-wise counter for
 *                              filtering
 *
 * \return Nothing returned, But the contents of `accum`, `pred` and 'count'
 *         will be modified
 */
void av1_apply_temporal_filter_c(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    const uint8_t *pred, uint32_t *accum, uint16_t *count) {
  // Block information.
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_pels = mb_height * mb_width;
  const int is_high_bitdepth = is_frame_high_bitdepth(frame_to_filter);
  const uint16_t *pred16 = CONVERT_TO_SHORTPTR(pred);
  // Frame information.
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  const int min_frame_size = AOMMIN(frame_height, frame_width);
  // Variables to simplify combined error calculation.
  const double inv_factor = 1.0 / ((TF_WINDOW_BLOCK_BALANCE_WEIGHT + 1) *
                                   TF_SEARCH_ERROR_NORM_WEIGHT);
  const double weight_factor =
      (double)TF_WINDOW_BLOCK_BALANCE_WEIGHT * inv_factor;
  // Decay factors for non-local mean approach.
  double decay_factor[MAX_MB_PLANE] = { 0 };
  // Smaller q -> smaller filtering weight.
  double q_decay = pow((double)q_factor / TF_Q_DECAY_THRESHOLD, 2);
  q_decay = CLIP(q_decay, 1e-5, 1);
  // Smaller strength -> smaller filtering weight.
  double s_decay = pow((double)filter_strength / TF_STRENGTH_THRESHOLD, 2);
  s_decay = CLIP(s_decay, 1e-5, 1);
  for (int plane = 0; plane < num_planes; plane++) {
    // Larger noise -> larger filtering weight.
    const double n_decay = 0.5 + log(2 * noise_levels[plane] + 5.0);
    decay_factor[plane] = 1 / (n_decay * q_decay * s_decay);
  }
  double d_factor[4] = { 0 };
  for (int subblock_idx = 0; subblock_idx < 4; subblock_idx++) {
    // Larger motion vector -> smaller filtering weight.
    const MV mv = subblock_mvs[subblock_idx];
    const double distance = sqrt(pow(mv.row, 2) + pow(mv.col, 2));
    double distance_threshold = min_frame_size * TF_SEARCH_DISTANCE_THRESHOLD;
    distance_threshold = AOMMAX(distance_threshold, 1);
    d_factor[subblock_idx] = distance / distance_threshold;
    d_factor[subblock_idx] = AOMMAX(d_factor[subblock_idx], 1);
  }

  // Allocate memory for pixel-wise squared differences. They,
  // regardless of the subsampling, are assigned with memory of size `mb_pels`.
  uint32_t *square_diff = aom_memalign(16, mb_pels * sizeof(uint32_t));
  memset(square_diff, 0, mb_pels * sizeof(square_diff[0]));

  // Allocate memory for accumulated luma squared error. This value will be
  // consumed while filtering the chroma planes.
  uint32_t *luma_sse_sum = aom_memalign(32, mb_pels * sizeof(uint32_t));
  memset(luma_sse_sum, 0, mb_pels * sizeof(luma_sse_sum[0]));

  // Get window size for pixel-wise filtering.
  assert(TF_WINDOW_LENGTH % 2 == 1);
  const int half_window = TF_WINDOW_LENGTH >> 1;

  // Handle planes in sequence.
  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    // Locate pixel on reference frame.
    const int subsampling_y = mbd->plane[plane].subsampling_y;
    const int subsampling_x = mbd->plane[plane].subsampling_x;
    const int h = mb_height >> subsampling_y;  // Plane height.
    const int w = mb_width >> subsampling_x;   // Plane width.
    const int frame_stride =
        frame_to_filter->strides[plane == AOM_PLANE_Y ? 0 : 1];
    const int frame_offset = mb_row * h * frame_stride + mb_col * w;
    const uint8_t *ref = frame_to_filter->buffers[plane];
    const int ss_y_shift =
        subsampling_y - mbd->plane[AOM_PLANE_Y].subsampling_y;
    const int ss_x_shift =
        subsampling_x - mbd->plane[AOM_PLANE_Y].subsampling_x;
    const int num_ref_pixels = TF_WINDOW_LENGTH * TF_WINDOW_LENGTH +
                               ((plane) ? (1 << (ss_x_shift + ss_y_shift)) : 0);
    const double inv_num_ref_pixels = 1.0 / num_ref_pixels;

    // Filter U-plane and V-plane using Y-plane. This is because motion
    // search is only done on Y-plane, so the information from Y-plane will
    // be more accurate. The luma sse sum is reused in both chroma planes.
    if (plane == AOM_PLANE_U)
      compute_luma_sq_error_sum(square_diff, luma_sse_sum, h, w, ss_x_shift,
                                ss_y_shift);
    compute_square_diff(ref, frame_offset, frame_stride, pred, plane_offset, w,
                        h, w, is_high_bitdepth, square_diff);

    // Perform filtering.
    int pred_idx = 0;
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        // non-local mean approach
        uint64_t sum_square_diff = 0;

        for (int wi = -half_window; wi <= half_window; ++wi) {
          for (int wj = -half_window; wj <= half_window; ++wj) {
            const int y = CLIP(i + wi, 0, h - 1);  // Y-coord on current plane.
            const int x = CLIP(j + wj, 0, w - 1);  // X-coord on current plane.
            sum_square_diff += square_diff[y * w + x];
          }
        }

        sum_square_diff += luma_sse_sum[i * w + j];

        // Scale down the difference for high bit depth input.
        if (mbd->bd > 8) sum_square_diff >>= ((mbd->bd - 8) * 2);

        // Combine window error and block error, and normalize it.
        const double window_error = sum_square_diff * inv_num_ref_pixels;
        const int subblock_idx = (i >= h / 2) * 2 + (j >= w / 2);
        const double block_error = (double)subblock_mses[subblock_idx];
        const double combined_error =
            weight_factor * window_error + block_error * inv_factor;

        // Compute filter weight.
        double scaled_error =
            combined_error * d_factor[subblock_idx] * decay_factor[plane];
        scaled_error = AOMMIN(scaled_error, 7);
        const int weight = (int)(exp(-scaled_error) * TF_WEIGHT_SCALE);

        const int idx = plane_offset + pred_idx;  // Index with plane shift.
        const int pred_value = is_high_bitdepth ? pred16[idx] : pred[idx];
        accum[idx] += weight * pred_value;
        count[idx] += weight;

        ++pred_idx;
      }
    }
    plane_offset += h * w;
  }

  aom_free(square_diff);
  aom_free(luma_sse_sum);
}
#if CONFIG_AV1_HIGHBITDEPTH
// Calls High bit-depth temporal filter
void av1_highbd_apply_temporal_filter_c(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    const uint8_t *pred, uint32_t *accum, uint16_t *count) {
  av1_apply_temporal_filter_c(frame_to_filter, mbd, block_size, mb_row, mb_col,
                              num_planes, noise_levels, subblock_mvs,
                              subblock_mses, q_factor, filter_strength, pred,
                              accum, count);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
/*!\brief Normalizes the accumulated filtering result to produce the filtered
 *        frame
 *
 * \ingroup src_frame_proc
 * \param[in]   mbd            Pointer to the block for filtering, which is
 *                             ONLY used to get subsampling information for
 *                             all the planes
 * \param[in]   block_size     Size of the block
 * \param[in]   mb_row         Row index of the block in the frame
 * \param[in]   mb_col         Column index of the block in the frame
 * \param[in]   num_planes     Number of planes in the frame
 * \param[in]   accum          Pointer to the pre-computed accumulator
 * \param[in]   count          Pointer to the pre-computed count
 * \param[out]  result_buffer  Pointer to result buffer
 *
 * \return Nothing returned, but the content to which `result_buffer` pointer
 *         will be modified
 */
static void tf_normalize_filtered_frame(
    const MACROBLOCKD *mbd, const BLOCK_SIZE block_size, const int mb_row,
    const int mb_col, const int num_planes, const uint32_t *accum,
    const uint16_t *count, YV12_BUFFER_CONFIG *result_buffer) {
  // Block information.
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int is_high_bitdepth = is_frame_high_bitdepth(result_buffer);

  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const int plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const int plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const int frame_stride = result_buffer->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;
    uint8_t *const buf = result_buffer->buffers[plane];
    uint16_t *const buf16 = CONVERT_TO_SHORTPTR(buf);

    int plane_idx = 0;             // Pixel index on current plane (block-base).
    int frame_idx = frame_offset;  // Pixel index on the entire frame.
    for (int i = 0; i < plane_h; ++i) {
      for (int j = 0; j < plane_w; ++j) {
        const int idx = plane_idx + plane_offset;
        const uint16_t rounding = count[idx] >> 1;
        if (is_high_bitdepth) {
          buf16[frame_idx] =
              (uint16_t)OD_DIVU(accum[idx] + rounding, count[idx]);
        } else {
          buf[frame_idx] = (uint8_t)OD_DIVU(accum[idx] + rounding, count[idx]);
        }
        ++plane_idx;
        ++frame_idx;
      }
      frame_idx += (frame_stride - plane_w);
    }
    plane_offset += plane_h * plane_w;
  }
}
/*!\cond */

// Helper function to compute number of blocks on either side of the frame.
static INLINE int get_num_blocks(const int frame_length, const int mb_length) {
  return (frame_length + mb_length - 1) / mb_length;
}

// Helper function to get `q` used for encoding.
static INLINE int get_q(const AV1_COMP *cpi) {
  const GF_GROUP *gf_group = &cpi->gf_group;
  const FRAME_TYPE frame_type = gf_group->frame_type[gf_group->index];
  const int q = (int)av1_convert_qindex_to_q(
      cpi->rc.avg_frame_qindex[frame_type], cpi->common.seq_params.bit_depth);
  return q;
}

/*!\endcond */

/*!\brief Does temporal filter for a given macroblock row.
*
* \ingroup src_frame_proc
* \param[in]   cpi                   Top level encoder instance structure
* \param[in]   td                    Pointer to thread data
* \param[in]   frames                Frame buffers used for temporal filtering
* \param[in]   num_frames            Number of frames in the frame buffer
* \param[in]   filter_frame_idx      Index of the frame to be filtered
* \param[in]   check_show_existing   whether to accumulate diff for show
existing condition check
* \param[in]   scale                 Frame scaling factor
* \param[in]   noise_levels          Estimated noise levels for each plane
*                                    in the frame (Y,U,V)
* \param[in]   num_pels              Number of pixels in the filtering block
across all planes
* \param[in]   mb_cols               Number of macroblock columns to be filtered
* \param[in]   mb_row                Macroblock row to be filtered
* \param[in]   q_factor              Quantization factor used in temporal
filtering
*
* \return Nothing will be returned, but the contents of td->diff will be
modified.
*/
static void tf_do_filtering_row(AV1_COMP *cpi, ThreadData *td,
                                YV12_BUFFER_CONFIG **frames,
                                const int num_frames,
                                const int filter_frame_idx,
                                const int check_show_existing,
                                const struct scale_factors *scale,
                                const double *noise_levels, int num_pels,
                                int mb_cols, int mb_row, int q_factor) {
  const BLOCK_SIZE block_size = TF_BLOCK_SIZE;
  const YV12_BUFFER_CONFIG *const frame_to_filter = frames[filter_frame_idx];
  MACROBLOCK *const mb = &td->mb;
  MACROBLOCKD *const mbd = &mb->e_mbd;
  TemporalFilterData *const tf_data = &td->tf_data;
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mi_h = mi_size_high_log2[block_size];
  const int mi_w = mi_size_wide_log2[block_size];
  const int num_planes = av1_num_planes(&cpi->common);
  uint32_t *accum = tf_data->accum;
  uint16_t *count = tf_data->count;
  uint8_t *pred = tf_data->pred;

  // Factor to control the filering strength.
  const int filter_strength = cpi->oxcf.algo_cfg.arnr_strength;

  // Do filtering.
  FRAME_DIFF *diff = &td->tf_data.diff;
  av1_set_mv_row_limits(&cpi->common.mi_params, &mb->mv_limits,
                        (mb_row << mi_h), (mb_height >> MI_SIZE_LOG2),
                        cpi->oxcf.border_in_pixels);
  for (int mb_col = 0; mb_col < mb_cols; mb_col++) {
    av1_set_mv_col_limits(&cpi->common.mi_params, &mb->mv_limits,
                          (mb_col << mi_w), (mb_width >> MI_SIZE_LOG2),
                          cpi->oxcf.border_in_pixels);
    memset(accum, 0, num_pels * sizeof(accum[0]));
    memset(count, 0, num_pels * sizeof(count[0]));
    MV ref_mv = kZeroMv;  // Reference motion vector passed down along frames.
                          // Perform temporal filtering frame by frame.
    for (int frame = 0; frame < num_frames; frame++) {
      if (frames[frame] == NULL) continue;

      // Motion search.
      MV subblock_mvs[4] = { kZeroMv, kZeroMv, kZeroMv, kZeroMv };
      int subblock_mses[4] = { INT_MAX, INT_MAX, INT_MAX, INT_MAX };
      if (frame ==
          filter_frame_idx) {  // Frame to be filtered.
                               // Change ref_mv sign for following frames.
        ref_mv.row *= -1;
        ref_mv.col *= -1;
      } else {  // Other reference frames.
        tf_motion_search(cpi, mb, frame_to_filter, frames[frame], block_size,
                         mb_row, mb_col, &ref_mv, subblock_mvs, subblock_mses);
      }

      // Perform weighted averaging.
      if (frame == filter_frame_idx) {  // Frame to be filtered.
        tf_apply_temporal_filter_self(frames[frame], mbd, block_size, mb_row,
                                      mb_col, num_planes, accum, count);
      } else {  // Other reference frames.
        tf_build_predictor(frames[frame], mbd, block_size, mb_row, mb_col,
                           num_planes, scale, subblock_mvs, pred);

        // TODO(any): avx2/sse2 version should be changed to align with C
        // function before using. In particular, current avx2/sse2 function
        // only supports 32x32 block size and 5x5 filtering window.
        if (is_frame_high_bitdepth(frame_to_filter)) {  // for high bit-depth
#if CONFIG_AV1_HIGHBITDEPTH
          if (TF_BLOCK_SIZE == BLOCK_32X32 && TF_WINDOW_LENGTH == 5) {
            av1_highbd_apply_temporal_filter(
                frame_to_filter, mbd, block_size, mb_row, mb_col, num_planes,
                noise_levels, subblock_mvs, subblock_mses, q_factor,
                filter_strength, pred, accum, count);
          } else {
#endif  // CONFIG_AV1_HIGHBITDEPTH
            av1_apply_temporal_filter_c(
                frame_to_filter, mbd, block_size, mb_row, mb_col, num_planes,
                noise_levels, subblock_mvs, subblock_mses, q_factor,
                filter_strength, pred, accum, count);
#if CONFIG_AV1_HIGHBITDEPTH
          }
#endif            // CONFIG_AV1_HIGHBITDEPTH
        } else {  // for 8-bit
          if (TF_BLOCK_SIZE == BLOCK_32X32 && TF_WINDOW_LENGTH == 5) {
            av1_apply_temporal_filter(frame_to_filter, mbd, block_size, mb_row,
                                      mb_col, num_planes, noise_levels,
                                      subblock_mvs, subblock_mses, q_factor,
                                      filter_strength, pred, accum, count);
          } else {
            av1_apply_temporal_filter_c(
                frame_to_filter, mbd, block_size, mb_row, mb_col, num_planes,
                noise_levels, subblock_mvs, subblock_mses, q_factor,
                filter_strength, pred, accum, count);
          }
        }
      }
    }
    tf_normalize_filtered_frame(mbd, block_size, mb_row, mb_col, num_planes,
                                accum, count, &cpi->alt_ref_buffer);

    if (check_show_existing) {
      const int y_height = mb_height >> mbd->plane[0].subsampling_y;
      const int y_width = mb_width >> mbd->plane[0].subsampling_x;
      const int source_y_stride = frame_to_filter->y_stride;
      const int filter_y_stride = cpi->alt_ref_buffer.y_stride;
      const int source_offset =
          mb_row * y_height * source_y_stride + mb_col * y_width;
      const int filter_offset =
          mb_row * y_height * filter_y_stride + mb_col * y_width;
      unsigned int sse = 0;
      cpi->fn_ptr[block_size].vf(
          frame_to_filter->y_buffer + source_offset, source_y_stride,
          cpi->alt_ref_buffer.y_buffer + filter_offset, filter_y_stride, &sse);
      diff->sum += sse;
      diff->sse += sse * sse;
    }
  }
}

/*!\brief Does temporal filter for a given frame.
 *
 * \ingroup src_frame_proc
 * \param[in]   cpi                   Top level encoder instance structure
 * \param[in]   frames                Frame buffers used for temporal filtering
 * \param[in]   num_frames            Number of frames in the frame buffer
 * \param[in]   filter_frame_idx      Index of the frame to be filtered
 * \param[in]   check_show_existing   whether to accumulate diff for show
                                      existing condition check
 * \param[in]   scale                 Frame scaling factor
 * \param[in]   noise_levels          Estimated noise levels for each plane
 *                                    in the frame (Y,U,V)
 * \param[in]   num_pels              Number of pixels in the filtering block
 across all planes
 *
 * \return Nothing will be returned, but the contents of td->diff will be
 modified.
 */
static void tf_do_filtering(AV1_COMP *cpi, YV12_BUFFER_CONFIG **frames,
                            const int num_frames, const int filter_frame_idx,
                            const int check_show_existing,
                            const struct scale_factors *scale,
                            const double *noise_levels, int num_pels) {
  // Basic information.
  ThreadData *td = &cpi->td;
  const YV12_BUFFER_CONFIG *const frame_to_filter = frames[filter_frame_idx];
  const BLOCK_SIZE block_size = TF_BLOCK_SIZE;
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_rows = get_num_blocks(frame_height, mb_height);
  const int mb_cols = get_num_blocks(frame_width, mb_width);
  const int num_planes = av1_num_planes(&cpi->common);
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  // Quantization factor used in temporal filtering.
  const int q_factor = get_q(cpi);

  MACROBLOCKD *mbd = &td->mb.e_mbd;
  uint8_t *input_buffer[MAX_MB_PLANE];
  MB_MODE_INFO **input_mb_mode_info;
  tf_save_state(mbd, &input_mb_mode_info, input_buffer, num_planes);
  tf_setup_macroblockd(mbd, &td->tf_data, scale);

  // Perform temporal filtering for each row.
  for (int mb_row = 0; mb_row < mb_rows; mb_row++) {
    tf_do_filtering_row(cpi, td, frames, num_frames, filter_frame_idx,
                        check_show_existing, scale, noise_levels, num_pels,
                        mb_cols, mb_row, q_factor);
  }

  tf_restore_state(mbd, input_mb_mode_info, input_buffer, num_planes);
}

/*!\brief Setups the frame buffer for temporal filtering. This fuction
 * determines how many frames will be used for temporal filtering and then
 * groups them into a buffer. This function will also estimate the noise level
 * of the to-filter frame.
 *
 * \ingroup src_frame_proc
 * \param[in]   cpi             Top level encoder instance structure
 * \param[in]   filter_frame_lookahead_idx  The index of the to-filter frame
 *                              in the lookahead buffer cpi->lookahead
 * \param[in]   is_second_arf   Whether the to-filter frame is the second ARF.
 *                              This field will affect the number of frames
 *                              used for filtering.
 * \param[in,out] frames        Pointer to the frame buffer to setup
 * \param[in,out] num_frames_for_filtering  Number of frames used for filtering
 * \param[in,out] filter_frame_idx Index of the to-filter frame in the setup
 *                              frame buffer.
 * \param[out]    noise_levels  Pointer to the noise levels of the to-filter
 *                              frame, estimated with each plane (in Y, U, V
 *                              order).
 *
 * \return Nothing will be returned. But the frame buffer `frames`, number of
 *         frames in the buffer `num_frames_for_filtering`, and the index of
 *         the to-filter frame in the buffer `filter_frame_idx` will be updated
 *         in this function. Estimated noise levels for YUV planes will be
 *         saved in `noise_levels`.
 */
static void tf_setup_filtering_buffer(AV1_COMP *cpi,
                                      const int filter_frame_lookahead_idx,
                                      const int is_second_arf,
                                      YV12_BUFFER_CONFIG **frames,
                                      int *num_frames_for_filtering,
                                      int *filter_frame_idx,
                                      double *noise_levels) {
  // Number of frames used for filtering. Set `arnr_max_frames` as 1 to disable
  // temporal filtering.
  int num_frames = AOMMAX(cpi->oxcf.algo_cfg.arnr_max_frames, 1);
  int num_before = 0;  // Number of filtering frames before the to-filter frame.
  int num_after = 0;   // Number of filtering frames after the to-filer frame.
  const int lookahead_depth =
      av1_lookahead_depth(cpi->lookahead, cpi->compressor_stage);

  // Temporal filtering should not go beyond key frames
  const int key_to_curframe =
      AOMMAX(cpi->rc.frames_since_key +
                 cpi->gf_group.arf_src_offset[cpi->gf_group.index],
             0);
  const int curframe_to_key =
      AOMMAX(cpi->rc.frames_to_key -
                 cpi->gf_group.arf_src_offset[cpi->gf_group.index] - 1,
             0);

  // Number of buffered frames before the to-filter frame.
  const int max_before =
      AOMMIN(filter_frame_lookahead_idx < -1 ? -filter_frame_lookahead_idx + 1
                                             : filter_frame_lookahead_idx + 1,
             key_to_curframe);
  // Number of buffered frames after the to-filter frame.
  const int max_after = AOMMIN(lookahead_depth - max_before, curframe_to_key);

  const int filter_frame_offset = filter_frame_lookahead_idx < -1
                                      ? -filter_frame_lookahead_idx
                                      : filter_frame_lookahead_idx;

  // Estimate noises for each plane.
  const struct lookahead_entry *to_filter_buf = av1_lookahead_peek(
      cpi->lookahead, filter_frame_offset, cpi->compressor_stage);
  assert(to_filter_buf != NULL);
  const YV12_BUFFER_CONFIG *to_filter_frame = &to_filter_buf->img;
  const int num_planes = av1_num_planes(&cpi->common);
  for (int plane = 0; plane < num_planes; ++plane) {
    noise_levels[plane] = av1_estimate_noise_from_single_plane(
        to_filter_frame, plane, cpi->common.seq_params.bit_depth);
  }
  // Get quantization factor.
  const int q = get_q(cpi);

  // Adjust number of filtering frames based on noise and quantization factor.
  // Basically, we would like to use more frames to filter low-noise frame such
  // that the filtered frame can provide better predictions for more frames.
  // Also, when the quantization factor is small enough (lossless compression),
  // we will not change the number of frames for key frame filtering, which is
  // to avoid visual quality drop.
  int adjust_num = 0;
  if (num_frames == 1) {  // `arnr_max_frames = 1` is used to disable filtering.
    adjust_num = 0;
  } else if (filter_frame_lookahead_idx < 0 && q <= 10) {
    adjust_num = 0;
  } else if (noise_levels[0] < 0.5) {
    adjust_num = 6;
  } else if (noise_levels[0] < 1.0) {
    adjust_num = 4;
  } else if (noise_levels[0] < 2.0) {
    adjust_num = 2;
  }
  num_frames = AOMMIN(num_frames + adjust_num, lookahead_depth + 1);

  if (filter_frame_lookahead_idx == -1 ||
      filter_frame_lookahead_idx == 0) {  // Key frame.
    num_before = 0;
    num_after = AOMMIN(num_frames - 1, max_after);
  } else if (filter_frame_lookahead_idx < -1) {  // Key frame in one-pass mode.
    num_before = AOMMIN(num_frames - 1, max_before);
    num_after = 0;
  } else {
    num_frames = AOMMIN(num_frames, cpi->rc.gfu_boost / 150);
    num_frames += !(num_frames & 1);  // Make the number odd.
    // Only use 2 neighbours for the second ARF.
    if (is_second_arf) num_frames = AOMMIN(num_frames, 3);
    num_before = AOMMIN(num_frames >> 1, max_before);
    num_after = AOMMIN(num_frames >> 1, max_after);
  }
  num_frames = num_before + 1 + num_after;

  // Setup the frame buffer.
  for (int frame = 0; frame < num_frames; ++frame) {
    const int lookahead_idx = frame - num_before + filter_frame_offset;
    struct lookahead_entry *buf = av1_lookahead_peek(
        cpi->lookahead, lookahead_idx, cpi->compressor_stage);
    assert(buf != NULL);
    frames[frame] = &buf->img;
  }
  *num_frames_for_filtering = num_frames;
  *filter_frame_idx = num_before;
  assert(frames[*filter_frame_idx] == to_filter_frame);

  av1_setup_src_planes(&cpi->td.mb, &to_filter_buf->img, 0, 0, num_planes,
                       cpi->common.seq_params.sb_size);
  av1_setup_block_planes(&cpi->td.mb.e_mbd,
                         cpi->common.seq_params.subsampling_x,
                         cpi->common.seq_params.subsampling_y, num_planes);
}

/*!\cond */

// A constant number, sqrt(pi / 2),  used for noise estimation.
static const double SQRT_PI_BY_2 = 1.25331413732;

double av1_estimate_noise_from_single_plane(const YV12_BUFFER_CONFIG *frame,
                                            const int plane,
                                            const int bit_depth) {
  const int is_y_plane = (plane == 0);
  const int height = frame->crop_heights[is_y_plane ? 0 : 1];
  const int width = frame->crop_widths[is_y_plane ? 0 : 1];
  const int stride = frame->strides[is_y_plane ? 0 : 1];
  const uint8_t *src = frame->buffers[plane];
  const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
  const int is_high_bitdepth = is_frame_high_bitdepth(frame);

  int64_t accum = 0;
  int count = 0;
  for (int i = 1; i < height - 1; ++i) {
    for (int j = 1; j < width - 1; ++j) {
      // Setup a small 3x3 matrix.
      const int center_idx = i * stride + j;
      int mat[3][3];
      for (int ii = -1; ii <= 1; ++ii) {
        for (int jj = -1; jj <= 1; ++jj) {
          const int idx = center_idx + ii * stride + jj;
          mat[ii + 1][jj + 1] = is_high_bitdepth ? src16[idx] : src[idx];
        }
      }
      // Compute sobel gradients.
      const int Gx = (mat[0][0] - mat[0][2]) + (mat[2][0] - mat[2][2]) +
                     2 * (mat[1][0] - mat[1][2]);
      const int Gy = (mat[0][0] - mat[2][0]) + (mat[0][2] - mat[2][2]) +
                     2 * (mat[0][1] - mat[2][1]);
      const int Ga = ROUND_POWER_OF_TWO(abs(Gx) + abs(Gy), bit_depth - 8);
      // Accumulate Laplacian.
      if (Ga < NOISE_ESTIMATION_EDGE_THRESHOLD) {  // Only count smooth pixels.
        const int v = 4 * mat[1][1] -
                      2 * (mat[0][1] + mat[2][1] + mat[1][0] + mat[1][2]) +
                      (mat[0][0] + mat[0][2] + mat[2][0] + mat[2][2]);
        accum += ROUND_POWER_OF_TWO(abs(v), bit_depth - 8);
        ++count;
      }
    }
  }

  // Return -1.0 (unreliable estimation) if there are too few smooth pixels.
  return (count < 16) ? -1.0 : (double)accum / (6 * count) * SQRT_PI_BY_2;
}

int av1_temporal_filter(AV1_COMP *cpi, const int filter_frame_lookahead_idx,
                        int *show_existing_arf) {
  // Basic informaton of the current frame.
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const uint8_t group_idx = gf_group->index;
  const FRAME_UPDATE_TYPE update_type = gf_group->update_type[group_idx];
  // Filter one more ARF if the lookahead index is leq 7 (w.r.t. 9-th frame).
  // This frame is ALWAYS a show existing frame.
  const int is_second_arf =
      (update_type == INTNL_ARF_UPDATE) && (filter_frame_lookahead_idx >= 7);
  // TODO(anyone): Currently, we enforce the filtering strength on internal
  // ARFs except the second ARF to be zero. We should investigate in which case
  // it is more beneficial to use non-zero strength filtering.
  if (update_type == INTNL_ARF_UPDATE && !is_second_arf) {
    return 0;
  }

  // TODO(yunqing): For INTNL_ARF_UPDATE type, the following me initialization
  // is used somewhere unexpectedly. Should be resolved later.
  // Initialize errorperbit and sadperbit
  const int rdmult = av1_compute_rd_mult_based_on_qindex(cpi, TF_QINDEX);
  MvCosts *mv_costs = &cpi->td.mb.mv_costs;
  av1_set_error_per_bit(mv_costs, rdmult);
  av1_set_sad_per_bit(cpi, mv_costs, TF_QINDEX);
  av1_fill_mv_costs(cpi->common.fc,
                    cpi->common.features.cur_frame_force_integer_mv,
                    cpi->common.features.allow_high_precision_mv, mv_costs);

  // Setup frame buffer for filtering.
  YV12_BUFFER_CONFIG *frames[MAX_LAG_BUFFERS] = { NULL };
  int num_frames_for_filtering = 0;
  int filter_frame_idx = -1;
  double noise_levels[MAX_MB_PLANE] = { 0 };
  tf_setup_filtering_buffer(cpi, filter_frame_lookahead_idx, is_second_arf,
                            frames, &num_frames_for_filtering,
                            &filter_frame_idx, noise_levels);
  assert(num_frames_for_filtering > 0);
  assert(filter_frame_idx < num_frames_for_filtering);

  // Set showable frame.
  if (filter_frame_lookahead_idx >= 0) {
    cpi->common.showable_frame = num_frames_for_filtering == 1 ||
                                 is_second_arf ||
                                 (cpi->oxcf.algo_cfg.enable_overlay == 0);
  }

  // Do filtering.
  // Check show existing condition for non-keyframes. For KFs, only check when
  // KF overlay is enabled.
  const int check_show_existing =
      !(filter_frame_lookahead_idx <= 0) ||
      cpi->oxcf.kf_cfg.enable_keyframe_filtering > 1;
  // Setup scaling factors. Scaling on each of the arnr frames is not
  // supported.
  // ARF is produced at the native frame size and resized when coded.
  struct scale_factors sf;
  av1_setup_scale_factors_for_frame(
      &sf, frames[0]->y_crop_width, frames[0]->y_crop_height,
      frames[0]->y_crop_width, frames[0]->y_crop_height);

  // Allocate temporal filter buffers.
  TemporalFilterData *tf_data = &cpi->td.tf_data;
  MACROBLOCKD *mbd = &cpi->td.mb.e_mbd;
  const BLOCK_SIZE block_size = TF_BLOCK_SIZE;
  const int mb_width = block_size_wide[block_size];
  const int mb_height = block_size_high[block_size];
  const int mb_pels = mb_width * mb_height;
  const int is_highbitdepth = is_frame_high_bitdepth(frames[filter_frame_idx]);
  const int num_planes = av1_num_planes(&cpi->common);
  int num_pels = 0;
  for (int i = 0; i < num_planes; i++) {
    const int subsampling_x = mbd->plane[i].subsampling_x;
    const int subsampling_y = mbd->plane[i].subsampling_y;
    num_pels += mb_pels >> (subsampling_x + subsampling_y);
  }
  tf_alloc_and_reset_data(tf_data, num_pels, is_highbitdepth);

  // Perform temporal filtering process.
  tf_do_filtering(cpi, frames, num_frames_for_filtering, filter_frame_idx,
                  check_show_existing, &sf, noise_levels, num_pels);

  // Deallocate temporal filter buffers.
  tf_dealloc_data(tf_data, is_highbitdepth);

  const FRAME_DIFF *diff = &tf_data->diff;

  if (!check_show_existing) return 1;

  if (show_existing_arf != NULL || is_second_arf) {
    const int frame_height = frames[filter_frame_idx]->y_crop_height;
    const int frame_width = frames[filter_frame_idx]->y_crop_width;
    const int block_height = block_size_high[TF_BLOCK_SIZE];
    const int block_width = block_size_wide[TF_BLOCK_SIZE];
    const int mb_rows = get_num_blocks(frame_height, block_height);
    const int mb_cols = get_num_blocks(frame_width, block_width);
    const int num_mbs = AOMMAX(1, mb_rows * mb_cols);
    const float mean = (float)diff->sum / num_mbs;
    const float std = (float)sqrt((float)diff->sse / num_mbs - mean * mean);

    aom_clear_system_state();
    // TODO(yunqing): This can be combined with TPL q calculation later.
    cpi->rc.base_frame_target = gf_group->bit_allocation[group_idx];
    av1_set_target_rate(cpi, cpi->common.width, cpi->common.height);
    int top_index = 0;
    int bottom_index = 0;
    const int q = av1_rc_pick_q_and_bounds(
        cpi, &cpi->rc, cpi->oxcf.frm_dim_cfg.width,
        cpi->oxcf.frm_dim_cfg.height, group_idx, &bottom_index, &top_index);
    const int ac_q = av1_ac_quant_QTX(q, 0, cpi->common.seq_params.bit_depth);
    const float threshold = 0.7f * ac_q * ac_q;

    if (!is_second_arf) {
      *show_existing_arf = 0;
      if (mean < threshold && std < mean * 1.2) {
        *show_existing_arf = 1;
      }
      cpi->common.showable_frame |= *show_existing_arf;
    } else {
      // Use source frame if the filtered frame becomes very different.
      if (!(mean < threshold && std < mean * 1.2)) {
        return 0;
      }
    }
  }

  return 1;
}
/*!\endcond */
