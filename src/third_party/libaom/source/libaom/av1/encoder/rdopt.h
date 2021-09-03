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

#ifndef AOM_AV1_ENCODER_RDOPT_H_
#define AOM_AV1_ENCODER_RDOPT_H_

#include <stdbool.h>

#include "av1/common/blockd.h"
#include "av1/common/txb_common.h"

#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/rdopt_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COMP_TYPE_RD_THRESH_SCALE 11
#define COMP_TYPE_RD_THRESH_SHIFT 4
#define MAX_WINNER_MOTION_MODES 10

struct TileInfo;
struct macroblock;
struct RD_STATS;

/*!\brief AV1 intra mode selection for intra frames.
 *
 * \ingroup intra_mode_search
 * \callgraph
 * Top level function for rd-based intra mode selection during intra frame
 * encoding. This function will first search for the best luma prediction by
 * calling av1_rd_pick_intra_sby_mode, then it searches for chroma prediction
 * with av1_rd_pick_intra_sbuv_mode. If applicable, this function ends the
 * search with an evaluation for intrabc.
 *
 * \param[in]    cpi            Top-level encoder structure.
 * \param[in]    x              Pointer to structure holding all the data for
                                the current macroblock.
 * \param[in]    rd_cost        Struct to keep track of the RD information.
 * \param[in]    bsize          Current block size.
 * \param[in]    ctx            Structure to hold snapshot of coding context
                                during the mode picking process.
 * \param[in]    best_rd Best   RD seen for this block so far.
 *
 * \return Nothing is returned. Instead, the MB_MODE_INFO struct inside x
 * is modified to store information about the best mode computed
 * in this function. The rd_cost struct is also updated with the RD stats
 * corresponding to the best mode found.
 */
void av1_rd_pick_intra_mode_sb(const struct AV1_COMP *cpi, struct macroblock *x,
                               struct RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd);

/*!\brief AV1 inter mode selection.
 *
 * \ingroup inter_mode_search
 * \callgraph
 * Top level function for inter mode selection. This function will loop over
 * all possible inter modes and select the best one for the current block by
 * computing the RD cost. The mode search and RD are computed in
 * handle_inter_mode(), which is called from this function within the main
 * loop.
 *
 * \param[in]    cpi            Top-level encoder structure
 * \param[in]    tile_data      Pointer to struct holding adaptive
                                data/contexts/models for the tile during
                                encoding
 * \param[in]    x              Pointer to structure holding all the data for
                                the current macroblock
 * \param[in]    rd_cost        Struct to keep track of the RD information
 * \param[in]    bsize          Current block size
 * \param[in]    ctx            Structure to hold snapshot of coding context
                                during the mode picking process
 * \param[in]    best_rd_so_far Best RD seen for this block so far
 *
 * \return Nothing is returned. Instead, the MB_MODE_INFO struct inside x
 * is modified to store information about the best mode computed
 * in this function. The rd_cost struct is also updated with the RD stats
 * corresponding to the best mode found.
 */
void av1_rd_pick_inter_mode(struct AV1_COMP *cpi, struct TileDataEnc *tile_data,
                            struct macroblock *x, struct RD_STATS *rd_cost,
                            BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                            int64_t best_rd_so_far);

/*!\brief AV1 intra mode selection based on Non-RD optimized model.
 *
 * \ingroup nonrd_mode_search
 * \callgraph
 * \callergraph
 * Top level function for Non-RD optimized intra mode selection.
 * This finction will loop over subset of intra modes and select the best one
 * based on calculated modelled RD cost. Only 4 intra modes are checked as
 * specified in \c intra_mode_list. When calculating RD cost Hadamard transform
 * of residual is used to calculate rate. Estmation of RD cost is performed
 * in \c estimate_block_intra which is called from this function
 *
 * \param[in]    cpi            Top-level encoder structure
 * \param[in]    x              Pointer to structure holding all the data for
                                the current macroblock
 * \param[in]    rd_cost        Struct to keep track of the RD information
 * \param[in]    bsize          Current block size
 * \param[in]    ctx            Structure to hold snapshot of coding context
                                during the mode picking process
 *
 * \return Nothing is returned. Instead, the MB_MODE_INFO struct inside x
 * is modified to store information about the best mode computed
 * in this function. The rd_cost struct is also updated with the RD stats
 * corresponding to the best mode found.
 */
void av1_nonrd_pick_intra_mode(AV1_COMP *cpi, MACROBLOCK *x, RD_STATS *rd_cost,
                               BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx);

/*!\brief AV1 inter mode selection based on Non-RD optimized model.
 *
 * \ingroup nonrd_mode_search
 * \callgraph
 * Top level function for Non-RD optimized inter mode selection.
 * This finction will loop over subset of inter modes and select the best one
 * based on calculated modelled RD cost. While making decisions which modes to
 * check, this function applies heuristics based on previously checked modes,
 * block residual variance, block size, and other factors to prune certain
 * modes and reference frames. Currently only single reference frame modes
 * are checked. Additional heuristics are applied to decide if intra modes
 *  need to be checked.
 *  *
 * \param[in]    cpi            Top-level encoder structure
 * \param[in]    tile_data      Pointer to struct holding adaptive
                                data/contexts/models for the tile during
                                encoding
 * \param[in]    x              Pointer to structure holding all the data for
                                the current macroblock
 * \param[in]    rd_cost        Struct to keep track of the RD information
 * \param[in]    bsize          Current block size
 * \param[in]    ctx            Structure to hold snapshot of coding context
                                during the mode picking process
 *
 * \return Nothing is returned. Instead, the MB_MODE_INFO struct inside x
 * is modified to store information about the best mode computed
 * in this function. The rd_cost struct is also updated with the RD stats
 * corresponding to the best mode found.
 */
void av1_nonrd_pick_inter_mode_sb(struct AV1_COMP *cpi,
                                  struct TileDataEnc *tile_data,
                                  struct macroblock *x,
                                  struct RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                  PICK_MODE_CONTEXT *ctx);

void av1_rd_pick_inter_mode_sb_seg_skip(
    const struct AV1_COMP *cpi, struct TileDataEnc *tile_data,
    struct macroblock *x, int mi_row, int mi_col, struct RD_STATS *rd_cost,
    BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

// TODO(any): The defs below could potentially be moved to rdopt_utils.h instead
// because they are not the main rdopt functions.
/*!\cond */
// The best edge strength seen in the block, as well as the best x and y
// components of edge strength seen.
typedef struct {
  uint16_t magnitude;
  uint16_t x;
  uint16_t y;
} EdgeInfo;
/*!\endcond */

/** Returns an integer indicating the strength of the edge.
 * 0 means no edge found, 556 is the strength of a solid black/white edge,
 * and the number may range higher if the signal is even stronger (e.g., on a
 * corner). high_bd is a bool indicating the source should be treated
 * as a 16-bit array. bd is the bit depth.
 */
EdgeInfo av1_edge_exists(const uint8_t *src, int src_stride, int w, int h,
                         bool high_bd, int bd);

/** Applies a Gaussian blur with sigma = 1.3. Used by av1_edge_exists and
 * tests.
 */
void av1_gaussian_blur(const uint8_t *src, int src_stride, int w, int h,
                       uint8_t *dst, bool high_bd, int bd);

/*!\cond */
/* Applies standard 3x3 Sobel matrix. */
typedef struct {
  int16_t x;
  int16_t y;
} sobel_xy;
/*!\endcond */

sobel_xy av1_sobel(const uint8_t *input, int stride, int i, int j,
                   bool high_bd);

void av1_inter_mode_data_init(struct TileDataEnc *tile_data);
void av1_inter_mode_data_fit(TileDataEnc *tile_data, int rdmult);

static INLINE int coded_to_superres_mi(int mi_col, int denom) {
  return (mi_col * denom + SCALE_NUMERATOR / 2) / SCALE_NUMERATOR;
}

static INLINE int av1_encoder_get_relative_dist(int a, int b) {
  assert(a >= 0 && b >= 0);
  return (a - b);
}

// This function will return number of mi's in a superblock.
static INLINE int av1_get_sb_mi_size(const AV1_COMMON *const cm) {
  const int mi_alloc_size_1d = mi_size_wide[cm->mi_params.mi_alloc_bsize];
  int sb_mi_rows =
      (mi_size_wide[cm->seq_params->sb_size] + mi_alloc_size_1d - 1) /
      mi_alloc_size_1d;
  assert(mi_size_wide[cm->seq_params->sb_size] ==
         mi_size_high[cm->seq_params->sb_size]);
  int sb_mi_size = sb_mi_rows * sb_mi_rows;

  return sb_mi_size;
}

// This function will copy usable ref_mv_stack[ref_frame][4] and
// weight[ref_frame][4] information from ref_mv_stack[ref_frame][8] and
// weight[ref_frame][8].
static INLINE void av1_copy_usable_ref_mv_stack_and_weight(
    const MACROBLOCKD *xd, MB_MODE_INFO_EXT *const mbmi_ext,
    MV_REFERENCE_FRAME ref_frame) {
  memcpy(mbmi_ext->weight[ref_frame], xd->weight[ref_frame],
         USABLE_REF_MV_STACK_SIZE * sizeof(xd->weight[0][0]));
  memcpy(mbmi_ext->ref_mv_stack[ref_frame], xd->ref_mv_stack[ref_frame],
         USABLE_REF_MV_STACK_SIZE * sizeof(xd->ref_mv_stack[0][0]));
}

// This function prunes the mode if either of the reference frame falls in the
// pruning list
static INLINE int prune_ref(const MV_REFERENCE_FRAME *const ref_frame,
                            const unsigned int *const ref_display_order_hint,
                            const unsigned int frame_display_order_hint,
                            const int *ref_frame_list) {
  for (int i = 0; i < 2; i++) {
    if (ref_frame_list[i] == NONE_FRAME) continue;

    if (ref_frame[0] == ref_frame_list[i] ||
        ref_frame[1] == ref_frame_list[i]) {
      if (av1_encoder_get_relative_dist(
              ref_display_order_hint[ref_frame_list[i] - LAST_FRAME],
              frame_display_order_hint) < 0)
        return 1;
    }
  }
  return 0;
}

static INLINE int prune_ref_by_selective_ref_frame(
    const AV1_COMP *const cpi, const MACROBLOCK *const x,
    const MV_REFERENCE_FRAME *const ref_frame,
    const unsigned int *const ref_display_order_hint) {
  const SPEED_FEATURES *const sf = &cpi->sf;
  if (!sf->inter_sf.selective_ref_frame) return 0;

  const int comp_pred = ref_frame[1] > INTRA_FRAME;

  if (sf->inter_sf.selective_ref_frame >= 2 ||
      (sf->inter_sf.selective_ref_frame == 1 && comp_pred)) {
    int ref_frame_list[2] = { LAST3_FRAME, LAST2_FRAME };

    if (x != NULL) {
      // Disable pruning if either tpl suggests that we keep the frame or
      // the pred_mv gives us the best sad
      if (x->tpl_keep_ref_frame[LAST3_FRAME] ||
          x->pred_mv_sad[LAST3_FRAME] == x->best_pred_mv_sad) {
        ref_frame_list[0] = NONE_FRAME;
      }
      if (x->tpl_keep_ref_frame[LAST2_FRAME] ||
          x->pred_mv_sad[LAST2_FRAME] == x->best_pred_mv_sad) {
        ref_frame_list[1] = NONE_FRAME;
      }
    }

    if (prune_ref(ref_frame, ref_display_order_hint,
                  ref_display_order_hint[GOLDEN_FRAME - LAST_FRAME],
                  ref_frame_list))
      return 1;
  }

  if (sf->inter_sf.selective_ref_frame >= 3) {
    int ref_frame_list[2] = { ALTREF2_FRAME, BWDREF_FRAME };

    if (x != NULL) {
      // Disable pruning if either tpl suggests that we keep the frame or
      // the pred_mv gives us the best sad
      if (x->tpl_keep_ref_frame[ALTREF2_FRAME] ||
          x->pred_mv_sad[ALTREF2_FRAME] == x->best_pred_mv_sad) {
        ref_frame_list[0] = NONE_FRAME;
      }
      if (x->tpl_keep_ref_frame[BWDREF_FRAME] ||
          x->pred_mv_sad[BWDREF_FRAME] == x->best_pred_mv_sad) {
        ref_frame_list[1] = NONE_FRAME;
      }
    }

    if (prune_ref(ref_frame, ref_display_order_hint,
                  ref_display_order_hint[LAST_FRAME - LAST_FRAME],
                  ref_frame_list))
      return 1;
  }

  return 0;
}

// This function will copy the best reference mode information from
// MB_MODE_INFO_EXT to MB_MODE_INFO_EXT_FRAME.
static INLINE void av1_copy_mbmi_ext_to_mbmi_ext_frame(
    MB_MODE_INFO_EXT_FRAME *mbmi_ext_best,
    const MB_MODE_INFO_EXT *const mbmi_ext, uint8_t ref_frame_type) {
  memcpy(mbmi_ext_best->ref_mv_stack, mbmi_ext->ref_mv_stack[ref_frame_type],
         sizeof(mbmi_ext->ref_mv_stack[USABLE_REF_MV_STACK_SIZE]));
  memcpy(mbmi_ext_best->weight, mbmi_ext->weight[ref_frame_type],
         sizeof(mbmi_ext->weight[USABLE_REF_MV_STACK_SIZE]));
  mbmi_ext_best->mode_context = mbmi_ext->mode_context[ref_frame_type];
  mbmi_ext_best->ref_mv_count = mbmi_ext->ref_mv_count[ref_frame_type];
  memcpy(mbmi_ext_best->global_mvs, mbmi_ext->global_mvs,
         sizeof(mbmi_ext->global_mvs));
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_RDOPT_H_
