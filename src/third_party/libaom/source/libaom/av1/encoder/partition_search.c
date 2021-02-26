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

#include "aom_ports/system_state.h"

#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/reconintra.h"

#include "av1/encoder/aq_complexity.h"
#include "av1/encoder/aq_variance.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodeframe_utils.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/motion_search_facade.h"
#include "av1/encoder/partition_search.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/tokenize.h"
#include "av1/encoder/var_based_part.h"
#include "av1/encoder/av1_ml_partition_models.h"

#if CONFIG_TUNE_VMAF
#include "av1/encoder/tune_vmaf.h"
#endif

static void update_txfm_count(MACROBLOCK *x, MACROBLOCKD *xd,
                              FRAME_COUNTS *counts, TX_SIZE tx_size, int depth,
                              int blk_row, int blk_col,
                              uint8_t allow_update_cdf) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->bsize;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                   xd->left_txfm_context + blk_row, mbmi->bsize,
                                   tx_size);
  const int txb_size_index = av1_get_txb_size_index(bsize, blk_row, blk_col);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[txb_size_index];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;
  assert(tx_size > TX_4X4);

  if (depth == MAX_VARTX_DEPTH) {
    // Don't add to counts in this case
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

  if (tx_size == plane_tx_size) {
#if CONFIG_ENTROPY_STATS
    ++counts->txfm_partition[ctx][0];
#endif
    if (allow_update_cdf)
      update_cdf(xd->tile_ctx->txfm_partition_cdf[ctx], 0, 2);
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];

#if CONFIG_ENTROPY_STATS
    ++counts->txfm_partition[ctx][1];
#endif
    if (allow_update_cdf)
      update_cdf(xd->tile_ctx->txfm_partition_cdf[ctx], 1, 2);
    ++x->txfm_search_info.txb_split_count;

    if (sub_txs == TX_4X4) {
      mbmi->inter_tx_size[txb_size_index] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }

    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        int offsetr = row;
        int offsetc = col;

        update_txfm_count(x, xd, counts, sub_txs, depth + 1, blk_row + offsetr,
                          blk_col + offsetc, allow_update_cdf);
      }
    }
  }
}

static void tx_partition_count_update(const AV1_COMMON *const cm, MACROBLOCK *x,
                                      BLOCK_SIZE plane_bsize,
                                      FRAME_COUNTS *td_counts,
                                      uint8_t allow_update_cdf) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int mi_width = mi_size_wide[plane_bsize];
  const int mi_height = mi_size_high[plane_bsize];
  const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];

  xd->above_txfm_context =
      cm->above_contexts.txfm[xd->tile.tile_row] + xd->mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (xd->mi_row & MAX_MIB_MASK);

  for (int idy = 0; idy < mi_height; idy += bh) {
    for (int idx = 0; idx < mi_width; idx += bw) {
      update_txfm_count(x, xd, td_counts, max_tx_size, 0, idy, idx,
                        allow_update_cdf);
    }
  }
}

static void set_txfm_context(MACROBLOCKD *xd, TX_SIZE tx_size, int blk_row,
                             int blk_col) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->bsize;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int txb_size_index = av1_get_txb_size_index(bsize, blk_row, blk_col);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[txb_size_index];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (tx_size == plane_tx_size) {
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);

  } else {
    if (tx_size == TX_8X8) {
      mbmi->inter_tx_size[txb_size_index] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        const int offsetr = blk_row + row;
        const int offsetc = blk_col + col;
        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;
        set_txfm_context(xd, sub_txs, offsetr, offsetc);
      }
    }
  }
}

static void tx_partition_set_contexts(const AV1_COMMON *const cm,
                                      MACROBLOCKD *xd, BLOCK_SIZE plane_bsize) {
  const int mi_width = mi_size_wide[plane_bsize];
  const int mi_height = mi_size_high[plane_bsize];
  const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];

  xd->above_txfm_context =
      cm->above_contexts.txfm[xd->tile.tile_row] + xd->mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (xd->mi_row & MAX_MIB_MASK);

  for (int idy = 0; idy < mi_height; idy += bh) {
    for (int idx = 0; idx < mi_width; idx += bw) {
      set_txfm_context(xd, max_tx_size, idy, idx);
    }
  }
}

static void update_zeromv_cnt(const AV1_COMP *const cpi,
                              const MB_MODE_INFO *const mi, int mi_row,
                              int mi_col, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  MV mv = mi->mv[0].as_mv;
  const int bw = mi_size_wide[bsize] >> 1;
  const int bh = mi_size_high[bsize] >> 1;
  const int xmis = AOMMIN((cm->mi_params.mi_cols - mi_col) >> 1, bw);
  const int ymis = AOMMIN((cm->mi_params.mi_rows - mi_row) >> 1, bh);
  const int block_index =
      (mi_row >> 1) * (cm->mi_params.mi_cols >> 1) + (mi_col >> 1);
  for (int y = 0; y < ymis; y++)
    for (int x = 0; x < xmis; x++) {
      // consec_zero_mv is in the scale of 8x8 blocks
      const int map_offset = block_index + y * (cm->mi_params.mi_cols >> 1) + x;
      if (mi->ref_frame[0] == LAST_FRAME && is_inter_block(mi) &&
          mi->segment_id <= CR_SEGMENT_ID_BOOST2) {
        if (abs(mv.row) < 10 && abs(mv.col) < 10) {
          if (cpi->consec_zero_mv[map_offset] < 255)
            cpi->consec_zero_mv[map_offset]++;
        } else {
          cpi->consec_zero_mv[map_offset] = 0;
        }
      }
    }
}

static void encode_superblock(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                              ThreadData *td, TokenExtra **t, RUN_TYPE dry_run,
                              BLOCK_SIZE bsize, int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO **mi_4x4 = xd->mi;
  MB_MODE_INFO *mbmi = mi_4x4[0];
  const int seg_skip =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  const int mis = cm->mi_params.mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int is_inter = is_inter_block(mbmi);

  // Initialize tx_mode and tx_size_search_method
  TxfmSearchParams *txfm_params = &x->txfm_search_params;
  set_tx_size_search_method(
      cm, &cpi->winner_mode_params, txfm_params,
      cpi->sf.winner_mode_sf.enable_winner_mode_for_tx_size_srch, 1);

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  if (!is_inter) {
    xd->cfl.store_y = store_cfl_required(cm, xd);
    mbmi->skip_txfm = 1;
    for (int plane = 0; plane < num_planes; ++plane) {
      av1_encode_intra_block_plane(cpi, x, bsize, plane, dry_run,
                                   cpi->optimize_seg_arr[mbmi->segment_id]);
    }

    // If there is at least one lossless segment, force the skip for intra
    // block to be 0, in order to avoid the segment_id to be changed by in
    // write_segment_id().
    if (!cpi->common.seg.segid_preskip && cpi->common.seg.update_map &&
        cpi->enc_seg.has_lossless_segment)
      mbmi->skip_txfm = 0;

    xd->cfl.store_y = 0;
    if (av1_allow_palette(cm->features.allow_screen_content_tools, bsize)) {
      for (int plane = 0; plane < AOMMIN(2, num_planes); ++plane) {
        if (mbmi->palette_mode_info.palette_size[plane] > 0) {
          if (!dry_run) {
            av1_tokenize_color_map(x, plane, t, bsize, mbmi->tx_size,
                                   PALETTE_MAP, tile_data->allow_update_cdf,
                                   td->counts);
          } else if (dry_run == DRY_RUN_COSTCOEFFS) {
            rate +=
                av1_cost_color_map(x, plane, bsize, mbmi->tx_size, PALETTE_MAP);
          }
        }
      }
    }

    av1_update_intra_mb_txb_context(cpi, td, dry_run, bsize,
                                    tile_data->allow_update_cdf);
  } else {
    int ref;
    const int is_compound = has_second_ref(mbmi);

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      const YV12_BUFFER_CONFIG *cfg =
          get_ref_frame_yv12_buf(cm, mbmi->ref_frame[ref]);
      assert(IMPLIES(!is_intrabc_block(mbmi), cfg));
      av1_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                           xd->block_ref_scale_factors[ref], num_planes);
    }
    int start_plane = (cpi->sf.rt_sf.reuse_inter_pred_nonrd) ? 1 : 0;
    av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize,
                                  start_plane, av1_num_planes(cm) - 1);
    if (mbmi->motion_mode == OBMC_CAUSAL) {
      assert(cpi->oxcf.motion_mode_cfg.enable_obmc);
      av1_build_obmc_inter_predictors_sb(cm, xd);
    }

#if CONFIG_MISMATCH_DEBUG
    if (dry_run == OUTPUT_ENABLED) {
      for (int plane = 0; plane < num_planes; ++plane) {
        const struct macroblockd_plane *pd = &xd->plane[plane];
        int pixel_c, pixel_r;
        mi_to_pixel_loc(&pixel_c, &pixel_r, mi_col, mi_row, 0, 0,
                        pd->subsampling_x, pd->subsampling_y);
        if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                                 pd->subsampling_y))
          continue;
        mismatch_record_block_pre(pd->dst.buf, pd->dst.stride,
                                  cm->current_frame.order_hint, plane, pixel_c,
                                  pixel_r, pd->width, pd->height,
                                  xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);
      }
    }
#else
    (void)num_planes;
#endif

    av1_encode_sb(cpi, x, bsize, dry_run);
    av1_tokenize_sb_vartx(cpi, td, dry_run, bsize, rate,
                          tile_data->allow_update_cdf);
  }

  if (!dry_run) {
    if (av1_allow_intrabc(cm) && is_intrabc_block(mbmi)) td->intrabc_used = 1;
    if (txfm_params->tx_mode_search_type == TX_MODE_SELECT &&
        !xd->lossless[mbmi->segment_id] && mbmi->bsize > BLOCK_4X4 &&
        !(is_inter && (mbmi->skip_txfm || seg_skip))) {
      if (is_inter) {
        tx_partition_count_update(cm, x, bsize, td->counts,
                                  tile_data->allow_update_cdf);
      } else {
        if (mbmi->tx_size != max_txsize_rect_lookup[bsize])
          ++x->txfm_search_info.txb_split_count;
        if (block_signals_txsize(bsize)) {
          const int tx_size_ctx = get_tx_size_context(xd);
          const int32_t tx_size_cat = bsize_to_tx_size_cat(bsize);
          const int depth = tx_size_to_depth(mbmi->tx_size, bsize);
          const int max_depths = bsize_to_max_depth(bsize);

          if (tile_data->allow_update_cdf)
            update_cdf(xd->tile_ctx->tx_size_cdf[tx_size_cat][tx_size_ctx],
                       depth, max_depths + 1);
#if CONFIG_ENTROPY_STATS
          ++td->counts->intra_tx_size[tx_size_cat][tx_size_ctx][depth];
#endif
        }
      }
      assert(IMPLIES(is_rect_tx(mbmi->tx_size), is_rect_tx_allowed(xd, mbmi)));
    } else {
      int i, j;
      TX_SIZE intra_tx_size;
      // The new intra coding scheme requires no change of transform size
      if (is_inter) {
        if (xd->lossless[mbmi->segment_id]) {
          intra_tx_size = TX_4X4;
        } else {
          intra_tx_size =
              tx_size_from_tx_mode(bsize, txfm_params->tx_mode_search_type);
        }
      } else {
        intra_tx_size = mbmi->tx_size;
      }

      for (j = 0; j < mi_height; j++)
        for (i = 0; i < mi_width; i++)
          if (mi_col + i < cm->mi_params.mi_cols &&
              mi_row + j < cm->mi_params.mi_rows)
            mi_4x4[mis * j + i]->tx_size = intra_tx_size;

      if (intra_tx_size != max_txsize_rect_lookup[bsize])
        ++x->txfm_search_info.txb_split_count;
    }
  }

  if (txfm_params->tx_mode_search_type == TX_MODE_SELECT &&
      block_signals_txsize(mbmi->bsize) && is_inter &&
      !(mbmi->skip_txfm || seg_skip) && !xd->lossless[mbmi->segment_id]) {
    if (dry_run) tx_partition_set_contexts(cm, xd, bsize);
  } else {
    TX_SIZE tx_size = mbmi->tx_size;
    // The new intra coding scheme requires no change of transform size
    if (is_inter) {
      if (xd->lossless[mbmi->segment_id]) {
        tx_size = TX_4X4;
      } else {
        tx_size = tx_size_from_tx_mode(bsize, txfm_params->tx_mode_search_type);
      }
    } else {
      tx_size = (bsize > BLOCK_4X4) ? tx_size : TX_4X4;
    }
    mbmi->tx_size = tx_size;
    set_txfm_ctxs(tx_size, xd->width, xd->height,
                  (mbmi->skip_txfm || seg_skip) && is_inter_block(mbmi), xd);
  }

  if (is_inter_block(mbmi) && !xd->is_chroma_ref && is_cfl_allowed(xd)) {
    cfl_store_block(xd, mbmi->bsize, mbmi->tx_size);
  }
  if (!dry_run) {
    if (cpi->oxcf.pass == 0 && cpi->svc.temporal_layer_id == 0 &&
        cpi->sf.rt_sf.use_temporal_noise_estimate &&
        (!cpi->use_svc ||
         (cpi->use_svc &&
          !cpi->svc.layer_context[cpi->svc.temporal_layer_id].is_key_frame &&
          cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1)))
      update_zeromv_cnt(cpi, mbmi, mi_row, mi_col, bsize);
  }
}

static void setup_block_rdmult(const AV1_COMP *const cpi, MACROBLOCK *const x,
                               int mi_row, int mi_col, BLOCK_SIZE bsize,
                               AQ_MODE aq_mode, MB_MODE_INFO *mbmi) {
  x->rdmult = cpi->rd.RDMULT;

  if (aq_mode != NO_AQ) {
    assert(mbmi != NULL);
    if (aq_mode == VARIANCE_AQ) {
      if (cpi->vaq_refresh) {
        const int energy = bsize <= BLOCK_16X16
                               ? x->mb_energy
                               : av1_log_block_var(cpi, x, bsize);
        mbmi->segment_id = energy;
      }
      x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
    } else if (aq_mode == COMPLEXITY_AQ) {
      x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
    } else if (aq_mode == CYCLIC_REFRESH_AQ) {
      // If segment is boosted, use rdmult for that segment.
      if (cyclic_refresh_segment_id_boosted(mbmi->segment_id))
        x->rdmult = av1_cyclic_refresh_get_rdmult(cpi->cyclic_refresh);
    }
  }

  const AV1_COMMON *const cm = &cpi->common;
  if (cm->delta_q_info.delta_q_present_flag &&
      !cpi->sf.rt_sf.use_nonrd_pick_mode) {
    x->rdmult =
        av1_get_hier_tpl_rdmult(cpi, x, bsize, mi_row, mi_col, x->rdmult);
  }

  if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIM) {
    av1_set_ssim_rdmult(cpi, &x->mv_costs, bsize, mi_row, mi_col, &x->rdmult);
  }
#if CONFIG_TUNE_VMAF
  if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_VMAF_WITHOUT_PREPROCESSING ||
      cpi->oxcf.tune_cfg.tuning == AOM_TUNE_VMAF_MAX_GAIN ||
      cpi->oxcf.tune_cfg.tuning == AOM_TUNE_VMAF_NEG_MAX_GAIN) {
    av1_set_vmaf_rdmult(cpi, x, bsize, mi_row, mi_col, &x->rdmult);
  }
#endif
}

void av1_set_offsets_without_segment_id(const AV1_COMP *const cpi,
                                        const TileInfo *const tile,
                                        MACROBLOCK *const x, int mi_row,
                                        int mi_col, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  assert(bsize < BLOCK_SIZES_ALL);
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  set_mode_info_offsets(&cpi->common.mi_params, &cpi->mbmi_ext_info, x, xd,
                        mi_row, mi_col);

  set_entropy_context(xd, mi_row, mi_col, num_planes);
  xd->above_txfm_context = cm->above_contexts.txfm[tile->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  // Set up destination pointers.
  av1_setup_dst_planes(xd->plane, bsize, &cm->cur_frame->buf, mi_row, mi_col, 0,
                       num_planes);

  // Set up limit values for MV components.
  // Mv beyond the range do not produce new/different prediction block.
  av1_set_mv_limits(&cm->mi_params, &x->mv_limits, mi_row, mi_col, mi_height,
                    mi_width, cpi->oxcf.border_in_pixels);

  set_plane_n4(xd, mi_width, mi_height, num_planes);

  // Set up distance of MB to edge of frame in 1/8th pel units.
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width,
                 cm->mi_params.mi_rows, cm->mi_params.mi_cols);

  // Set up source buffers.
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes, bsize);

  // required by av1_append_sub8x8_mvs_for_idx() and av1_find_best_ref_mvs()
  xd->tile = *tile;
}

void av1_set_offsets(const AV1_COMP *const cpi, const TileInfo *const tile,
                     MACROBLOCK *const x, int mi_row, int mi_col,
                     BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  const struct segmentation *const seg = &cm->seg;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;

  av1_set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize);

  // Setup segment ID.
  mbmi = xd->mi[0];
  mbmi->segment_id = 0;
  if (seg->enabled) {
    if (seg->enabled && !cpi->vaq_refresh) {
      const uint8_t *const map =
          seg->update_map ? cpi->enc_seg.map : cm->last_frame_seg_map;
      mbmi->segment_id =
          map ? get_segment_id(&cm->mi_params, map, bsize, mi_row, mi_col) : 0;
    }
    av1_init_plane_quantizers(cpi, x, mbmi->segment_id);
  }
}

/*!\brief Hybrid intra mode search.
 *
 * \ingroup intra_mode_search
 * \callgraph
 * \callergraph
 * This is top level function for mode srarch for intra frames in non-RD
 * optimized case. Depending on speed feature, rate control mode and block
 * size it calls either non-RD or RD optimized intra mode search
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

static AOM_INLINE void hybrid_intra_mode_search(AV1_COMP *cpi,
                                                MACROBLOCK *const x,
                                                RD_STATS *rd_cost,
                                                BLOCK_SIZE bsize,
                                                PICK_MODE_CONTEXT *ctx) {
  // TODO(jianj): Investigate the failure of ScalabilityTest in AOM_Q mode,
  // which sets base_qindex to 0 on keyframe.
  if (cpi->oxcf.rc_cfg.mode != AOM_CBR ||
      !cpi->sf.rt_sf.hybrid_intra_pickmode || bsize < BLOCK_16X16)
    av1_rd_pick_intra_mode_sb(cpi, x, rd_cost, bsize, ctx, INT64_MAX);
  else
    av1_nonrd_pick_intra_mode(cpi, x, rd_cost, bsize, ctx);
}

/*!\brief Interface for AV1 mode search for an individual coding block
 *
 * \ingroup partition_search
 * \callgraph
 * \callergraph
 * Searches prediction modes, transform, and coefficient coding modes for an
 * individual coding block. This function is the top-level interface that
 * directs the encoder to the proper mode search function, among these
 * implemented for inter/intra + rd/non-rd + non-skip segment/skip segment.
 *
 * \param[in]    cpi            Top-level encoder structure
 * \param[in]    tile_data      Pointer to struct holding adaptive
 *                              data/contexts/models for the tile during
 *                              encoding
 * \param[in]    x              Pointer to structure holding all the data for
 *                              the current macroblock
 * \param[in]    mi_row         Row coordinate of the block in a step size of
 *                              MI_SIZE
 * \param[in]    mi_col         Column coordinate of the block in a step size of
 *                              MI_SIZE
 * \param[in]    rd_cost        Pointer to structure holding rate and distortion
 *                              stats for the current block
 * \param[in]    partition      Partition mode of the parent block
 * \param[in]    bsize          Current block size
 * \param[in]    ctx            Pointer to structure holding coding contexts and
 *                              chosen modes for the current block
 * \param[in]    best_rd        Upper bound of rd cost of a valid partition
 *
 * \return Nothing is returned. Instead, the chosen modes and contexts necessary
 * for reconstruction are stored in ctx, the rate-distortion stats are stored in
 * rd_cost. If no valid mode leading to rd_cost <= best_rd, the status will be
 * signalled by an INT64_MAX rd_cost->rdcost.
 */
static void pick_sb_modes(AV1_COMP *const cpi, TileDataEnc *tile_data,
                          MACROBLOCK *const x, int mi_row, int mi_col,
                          RD_STATS *rd_cost, PARTITION_TYPE partition,
                          BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                          RD_STATS best_rd) {
  if (best_rd.rdcost < 0) {
    ctx->rd_stats.rdcost = INT64_MAX;
    ctx->rd_stats.skip_txfm = 0;
    av1_invalid_rd_stats(rd_cost);
    return;
  }

  av1_set_offsets(cpi, &tile_data->tile_info, x, mi_row, mi_col, bsize);

  if (ctx->rd_mode_is_ready) {
    assert(ctx->mic.bsize == bsize);
    assert(ctx->mic.partition == partition);
    rd_cost->rate = ctx->rd_stats.rate;
    rd_cost->dist = ctx->rd_stats.dist;
    rd_cost->rdcost = ctx->rd_stats.rdcost;
    return;
  }

  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const AQ_MODE aq_mode = cpi->oxcf.q_cfg.aq_mode;
  TxfmSearchInfo *txfm_info = &x->txfm_search_info;

  int i;

#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, rd_pick_sb_modes_time);
#endif

  aom_clear_system_state();

  mbmi = xd->mi[0];
  mbmi->bsize = bsize;
  mbmi->partition = partition;

#if CONFIG_RD_DEBUG
  mbmi->mi_row = mi_row;
  mbmi->mi_col = mi_col;
#endif

  // Sets up the tx_type_map buffer in MACROBLOCKD.
  xd->tx_type_map = txfm_info->tx_type_map_;
  xd->tx_type_map_stride = mi_size_wide[bsize];

  for (i = 0; i < num_planes; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    p[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
  }

  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];

  ctx->skippable = 0;
  // Set to zero to make sure we do not use the previous encoded frame stats
  mbmi->skip_txfm = 0;
  // Reset skip mode flag.
  mbmi->skip_mode = 0;

  if (is_cur_buf_hbd(xd)) {
    x->source_variance = av1_high_get_sby_perpixel_variance(
        cpi, &x->plane[0].src, bsize, xd->bd);
  } else {
    x->source_variance =
        av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
  }

  // Initialize default mode evaluation params
  set_mode_eval_params(cpi, x, DEFAULT_EVAL);

  // Save rdmult before it might be changed, so it can be restored later.
  const int orig_rdmult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, aq_mode, mbmi);
  // Set error per bit for current rdmult
  av1_set_error_per_bit(&x->mv_costs, x->rdmult);
  av1_rd_cost_update(x->rdmult, &best_rd);

  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, av1_rd_pick_intra_mode_sb_time);
#endif
    av1_rd_pick_intra_mode_sb(cpi, x, rd_cost, bsize, ctx, best_rd.rdcost);
#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, av1_rd_pick_intra_mode_sb_time);
#endif
  } else {
#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, av1_rd_pick_inter_mode_sb_time);
#endif
    if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      av1_rd_pick_inter_mode_sb_seg_skip(cpi, tile_data, x, mi_row, mi_col,
                                         rd_cost, bsize, ctx, best_rd.rdcost);
    } else {
      av1_rd_pick_inter_mode_sb(cpi, tile_data, x, rd_cost, bsize, ctx,
                                best_rd.rdcost);
    }
#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, av1_rd_pick_inter_mode_sb_time);
#endif
  }

  // Examine the resulting rate and for AQ mode 2 make a segment choice.
  if (rd_cost->rate != INT_MAX && aq_mode == COMPLEXITY_AQ &&
      bsize >= BLOCK_16X16) {
    av1_caq_select_segment(cpi, x, bsize, mi_row, mi_col, rd_cost->rate);
  }

  x->rdmult = orig_rdmult;

  // TODO(jingning) The rate-distortion optimization flow needs to be
  // refactored to provide proper exit/return handle.
  if (rd_cost->rate == INT_MAX) rd_cost->rdcost = INT64_MAX;

  ctx->rd_stats.rate = rd_cost->rate;
  ctx->rd_stats.dist = rd_cost->dist;
  ctx->rd_stats.rdcost = rd_cost->rdcost;

#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, rd_pick_sb_modes_time);
#endif
}

static void update_stats(const AV1_COMMON *const cm, ThreadData *td) {
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const MB_MODE_INFO_EXT *const mbmi_ext = &x->mbmi_ext;
  const CurrentFrame *const current_frame = &cm->current_frame;
  const BLOCK_SIZE bsize = mbmi->bsize;
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const int seg_ref_active =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_REF_FRAME);

  if (current_frame->skip_mode_info.skip_mode_flag && !seg_ref_active &&
      is_comp_ref_allowed(bsize)) {
    const int skip_mode_ctx = av1_get_skip_mode_context(xd);
#if CONFIG_ENTROPY_STATS
    td->counts->skip_mode[skip_mode_ctx][mbmi->skip_mode]++;
#endif
    update_cdf(fc->skip_mode_cdfs[skip_mode_ctx], mbmi->skip_mode, 2);
  }

  if (!mbmi->skip_mode && !seg_ref_active) {
    const int skip_ctx = av1_get_skip_txfm_context(xd);
#if CONFIG_ENTROPY_STATS
    td->counts->skip_txfm[skip_ctx][mbmi->skip_txfm]++;
#endif
    update_cdf(fc->skip_txfm_cdfs[skip_ctx], mbmi->skip_txfm, 2);
  }

#if CONFIG_ENTROPY_STATS
  // delta quant applies to both intra and inter
  const int super_block_upper_left =
      ((xd->mi_row & (cm->seq_params.mib_size - 1)) == 0) &&
      ((xd->mi_col & (cm->seq_params.mib_size - 1)) == 0);
  const DeltaQInfo *const delta_q_info = &cm->delta_q_info;
  if (delta_q_info->delta_q_present_flag &&
      (bsize != cm->seq_params.sb_size || !mbmi->skip_txfm) &&
      super_block_upper_left) {
    const int dq = (mbmi->current_qindex - xd->current_base_qindex) /
                   delta_q_info->delta_q_res;
    const int absdq = abs(dq);
    for (int i = 0; i < AOMMIN(absdq, DELTA_Q_SMALL); ++i) {
      td->counts->delta_q[i][1]++;
    }
    if (absdq < DELTA_Q_SMALL) td->counts->delta_q[absdq][0]++;
    if (delta_q_info->delta_lf_present_flag) {
      if (delta_q_info->delta_lf_multi) {
        const int frame_lf_count =
            av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
        for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
          const int delta_lf = (mbmi->delta_lf[lf_id] - xd->delta_lf[lf_id]) /
                               delta_q_info->delta_lf_res;
          const int abs_delta_lf = abs(delta_lf);
          for (int i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
            td->counts->delta_lf_multi[lf_id][i][1]++;
          }
          if (abs_delta_lf < DELTA_LF_SMALL)
            td->counts->delta_lf_multi[lf_id][abs_delta_lf][0]++;
        }
      } else {
        const int delta_lf =
            (mbmi->delta_lf_from_base - xd->delta_lf_from_base) /
            delta_q_info->delta_lf_res;
        const int abs_delta_lf = abs(delta_lf);
        for (int i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
          td->counts->delta_lf[i][1]++;
        }
        if (abs_delta_lf < DELTA_LF_SMALL)
          td->counts->delta_lf[abs_delta_lf][0]++;
      }
    }
  }
#endif

  if (!is_inter_block(mbmi)) {
    av1_sum_intra_stats(cm, td->counts, xd, mbmi, xd->above_mbmi, xd->left_mbmi,
                        frame_is_intra_only(cm));
  }

  if (av1_allow_intrabc(cm)) {
    update_cdf(fc->intrabc_cdf, is_intrabc_block(mbmi), 2);
#if CONFIG_ENTROPY_STATS
    ++td->counts->intrabc[is_intrabc_block(mbmi)];
#endif  // CONFIG_ENTROPY_STATS
  }

  if (frame_is_intra_only(cm) || mbmi->skip_mode) return;

  FRAME_COUNTS *const counts = td->counts;
  const int inter_block = is_inter_block(mbmi);

  if (!seg_ref_active) {
#if CONFIG_ENTROPY_STATS
    counts->intra_inter[av1_get_intra_inter_context(xd)][inter_block]++;
#endif
    update_cdf(fc->intra_inter_cdf[av1_get_intra_inter_context(xd)],
               inter_block, 2);
    // If the segment reference feature is enabled we have only a single
    // reference frame allowed for the segment so exclude it from
    // the reference frame counts used to work out probabilities.
    if (inter_block) {
      const MV_REFERENCE_FRAME ref0 = mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME ref1 = mbmi->ref_frame[1];
      if (current_frame->reference_mode == REFERENCE_MODE_SELECT) {
        if (is_comp_ref_allowed(bsize)) {
#if CONFIG_ENTROPY_STATS
          counts->comp_inter[av1_get_reference_mode_context(xd)]
                            [has_second_ref(mbmi)]++;
#endif  // CONFIG_ENTROPY_STATS
          update_cdf(av1_get_reference_mode_cdf(xd), has_second_ref(mbmi), 2);
        }
      }

      if (has_second_ref(mbmi)) {
        const COMP_REFERENCE_TYPE comp_ref_type = has_uni_comp_refs(mbmi)
                                                      ? UNIDIR_COMP_REFERENCE
                                                      : BIDIR_COMP_REFERENCE;
        update_cdf(av1_get_comp_reference_type_cdf(xd), comp_ref_type,
                   COMP_REFERENCE_TYPES);
#if CONFIG_ENTROPY_STATS
        counts->comp_ref_type[av1_get_comp_reference_type_context(xd)]
                             [comp_ref_type]++;
#endif  // CONFIG_ENTROPY_STATS

        if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
          const int bit = (ref0 == BWDREF_FRAME);
          update_cdf(av1_get_pred_cdf_uni_comp_ref_p(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
          counts
              ->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p(xd)][0][bit]++;
#endif  // CONFIG_ENTROPY_STATS
          if (!bit) {
            const int bit1 = (ref1 == LAST3_FRAME || ref1 == GOLDEN_FRAME);
            update_cdf(av1_get_pred_cdf_uni_comp_ref_p1(xd), bit1, 2);
#if CONFIG_ENTROPY_STATS
            counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p1(xd)][1]
                                [bit1]++;
#endif  // CONFIG_ENTROPY_STATS
            if (bit1) {
              update_cdf(av1_get_pred_cdf_uni_comp_ref_p2(xd),
                         ref1 == GOLDEN_FRAME, 2);
#if CONFIG_ENTROPY_STATS
              counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p2(xd)][2]
                                  [ref1 == GOLDEN_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
          }
        } else {
          const int bit = (ref0 == GOLDEN_FRAME || ref0 == LAST3_FRAME);
          update_cdf(av1_get_pred_cdf_comp_ref_p(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
          counts->comp_ref[av1_get_pred_context_comp_ref_p(xd)][0][bit]++;
#endif  // CONFIG_ENTROPY_STATS
          if (!bit) {
            update_cdf(av1_get_pred_cdf_comp_ref_p1(xd), ref0 == LAST2_FRAME,
                       2);
#if CONFIG_ENTROPY_STATS
            counts->comp_ref[av1_get_pred_context_comp_ref_p1(xd)][1]
                            [ref0 == LAST2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
          } else {
            update_cdf(av1_get_pred_cdf_comp_ref_p2(xd), ref0 == GOLDEN_FRAME,
                       2);
#if CONFIG_ENTROPY_STATS
            counts->comp_ref[av1_get_pred_context_comp_ref_p2(xd)][2]
                            [ref0 == GOLDEN_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
          }
          update_cdf(av1_get_pred_cdf_comp_bwdref_p(xd), ref1 == ALTREF_FRAME,
                     2);
#if CONFIG_ENTROPY_STATS
          counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p(xd)][0]
                             [ref1 == ALTREF_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
          if (ref1 != ALTREF_FRAME) {
            update_cdf(av1_get_pred_cdf_comp_bwdref_p1(xd),
                       ref1 == ALTREF2_FRAME, 2);
#if CONFIG_ENTROPY_STATS
            counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p1(xd)][1]
                               [ref1 == ALTREF2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
          }
        }
      } else {
        const int bit = (ref0 >= BWDREF_FRAME);
        update_cdf(av1_get_pred_cdf_single_ref_p1(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
        counts->single_ref[av1_get_pred_context_single_ref_p1(xd)][0][bit]++;
#endif  // CONFIG_ENTROPY_STATS
        if (bit) {
          assert(ref0 <= ALTREF_FRAME);
          update_cdf(av1_get_pred_cdf_single_ref_p2(xd), ref0 == ALTREF_FRAME,
                     2);
#if CONFIG_ENTROPY_STATS
          counts->single_ref[av1_get_pred_context_single_ref_p2(xd)][1]
                            [ref0 == ALTREF_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
          if (ref0 != ALTREF_FRAME) {
            update_cdf(av1_get_pred_cdf_single_ref_p6(xd),
                       ref0 == ALTREF2_FRAME, 2);
#if CONFIG_ENTROPY_STATS
            counts->single_ref[av1_get_pred_context_single_ref_p6(xd)][5]
                              [ref0 == ALTREF2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
          }
        } else {
          const int bit1 = !(ref0 == LAST2_FRAME || ref0 == LAST_FRAME);
          update_cdf(av1_get_pred_cdf_single_ref_p3(xd), bit1, 2);
#if CONFIG_ENTROPY_STATS
          counts->single_ref[av1_get_pred_context_single_ref_p3(xd)][2][bit1]++;
#endif  // CONFIG_ENTROPY_STATS
          if (!bit1) {
            update_cdf(av1_get_pred_cdf_single_ref_p4(xd), ref0 != LAST_FRAME,
                       2);
#if CONFIG_ENTROPY_STATS
            counts->single_ref[av1_get_pred_context_single_ref_p4(xd)][3]
                              [ref0 != LAST_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
          } else {
            update_cdf(av1_get_pred_cdf_single_ref_p5(xd), ref0 != LAST3_FRAME,
                       2);
#if CONFIG_ENTROPY_STATS
            counts->single_ref[av1_get_pred_context_single_ref_p5(xd)][4]
                              [ref0 != LAST3_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
          }
        }
      }

      if (cm->seq_params.enable_interintra_compound &&
          is_interintra_allowed(mbmi)) {
        const int bsize_group = size_group_lookup[bsize];
        if (mbmi->ref_frame[1] == INTRA_FRAME) {
#if CONFIG_ENTROPY_STATS
          counts->interintra[bsize_group][1]++;
#endif
          update_cdf(fc->interintra_cdf[bsize_group], 1, 2);
#if CONFIG_ENTROPY_STATS
          counts->interintra_mode[bsize_group][mbmi->interintra_mode]++;
#endif
          update_cdf(fc->interintra_mode_cdf[bsize_group],
                     mbmi->interintra_mode, INTERINTRA_MODES);
          if (av1_is_wedge_used(bsize)) {
#if CONFIG_ENTROPY_STATS
            counts->wedge_interintra[bsize][mbmi->use_wedge_interintra]++;
#endif
            update_cdf(fc->wedge_interintra_cdf[bsize],
                       mbmi->use_wedge_interintra, 2);
            if (mbmi->use_wedge_interintra) {
#if CONFIG_ENTROPY_STATS
              counts->wedge_idx[bsize][mbmi->interintra_wedge_index]++;
#endif
              update_cdf(fc->wedge_idx_cdf[bsize], mbmi->interintra_wedge_index,
                         16);
            }
          }
        } else {
#if CONFIG_ENTROPY_STATS
          counts->interintra[bsize_group][0]++;
#endif
          update_cdf(fc->interintra_cdf[bsize_group], 0, 2);
        }
      }

      const MOTION_MODE motion_allowed =
          cm->features.switchable_motion_mode
              ? motion_mode_allowed(xd->global_motion, xd, mbmi,
                                    cm->features.allow_warped_motion)
              : SIMPLE_TRANSLATION;
      if (mbmi->ref_frame[1] != INTRA_FRAME) {
        if (motion_allowed == WARPED_CAUSAL) {
#if CONFIG_ENTROPY_STATS
          counts->motion_mode[bsize][mbmi->motion_mode]++;
#endif
          update_cdf(fc->motion_mode_cdf[bsize], mbmi->motion_mode,
                     MOTION_MODES);
        } else if (motion_allowed == OBMC_CAUSAL) {
#if CONFIG_ENTROPY_STATS
          counts->obmc[bsize][mbmi->motion_mode == OBMC_CAUSAL]++;
#endif
          update_cdf(fc->obmc_cdf[bsize], mbmi->motion_mode == OBMC_CAUSAL, 2);
        }
      }

      if (has_second_ref(mbmi)) {
        assert(current_frame->reference_mode != SINGLE_REFERENCE &&
               is_inter_compound_mode(mbmi->mode) &&
               mbmi->motion_mode == SIMPLE_TRANSLATION);

        const int masked_compound_used = is_any_masked_compound_used(bsize) &&
                                         cm->seq_params.enable_masked_compound;
        if (masked_compound_used) {
          const int comp_group_idx_ctx = get_comp_group_idx_context(xd);
#if CONFIG_ENTROPY_STATS
          ++counts->comp_group_idx[comp_group_idx_ctx][mbmi->comp_group_idx];
#endif
          update_cdf(fc->comp_group_idx_cdf[comp_group_idx_ctx],
                     mbmi->comp_group_idx, 2);
        }

        if (mbmi->comp_group_idx == 0) {
          const int comp_index_ctx = get_comp_index_context(cm, xd);
#if CONFIG_ENTROPY_STATS
          ++counts->compound_index[comp_index_ctx][mbmi->compound_idx];
#endif
          update_cdf(fc->compound_index_cdf[comp_index_ctx], mbmi->compound_idx,
                     2);
        } else {
          assert(masked_compound_used);
          if (is_interinter_compound_used(COMPOUND_WEDGE, bsize)) {
#if CONFIG_ENTROPY_STATS
            ++counts->compound_type[bsize][mbmi->interinter_comp.type -
                                           COMPOUND_WEDGE];
#endif
            update_cdf(fc->compound_type_cdf[bsize],
                       mbmi->interinter_comp.type - COMPOUND_WEDGE,
                       MASKED_COMPOUND_TYPES);
          }
        }
      }
      if (mbmi->interinter_comp.type == COMPOUND_WEDGE) {
        if (is_interinter_compound_used(COMPOUND_WEDGE, bsize)) {
#if CONFIG_ENTROPY_STATS
          counts->wedge_idx[bsize][mbmi->interinter_comp.wedge_index]++;
#endif
          update_cdf(fc->wedge_idx_cdf[bsize],
                     mbmi->interinter_comp.wedge_index, 16);
        }
      }
    }
  }

  if (inter_block && cm->features.interp_filter == SWITCHABLE &&
      mbmi->motion_mode != WARPED_CAUSAL &&
      !is_nontrans_global_motion(xd, mbmi)) {
    update_filter_type_cdf(xd, mbmi);
  }
  if (inter_block &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    const PREDICTION_MODE mode = mbmi->mode;
    const int16_t mode_ctx =
        av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame);
    if (has_second_ref(mbmi)) {
#if CONFIG_ENTROPY_STATS
      ++counts->inter_compound_mode[mode_ctx][INTER_COMPOUND_OFFSET(mode)];
#endif
      update_cdf(fc->inter_compound_mode_cdf[mode_ctx],
                 INTER_COMPOUND_OFFSET(mode), INTER_COMPOUND_MODES);
    } else {
      av1_update_inter_mode_stats(fc, counts, mode, mode_ctx);
    }

    const int new_mv = mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV;
    if (new_mv) {
      const uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
      for (int idx = 0; idx < 2; ++idx) {
        if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
          const uint8_t drl_ctx =
              av1_drl_ctx(mbmi_ext->weight[ref_frame_type], idx);
          update_cdf(fc->drl_cdf[drl_ctx], mbmi->ref_mv_idx != idx, 2);
#if CONFIG_ENTROPY_STATS
          ++counts->drl_mode[drl_ctx][mbmi->ref_mv_idx != idx];
#endif
          if (mbmi->ref_mv_idx == idx) break;
        }
      }
    }

    if (have_nearmv_in_inter_mode(mbmi->mode)) {
      const uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
      for (int idx = 1; idx < 3; ++idx) {
        if (mbmi_ext->ref_mv_count[ref_frame_type] > idx + 1) {
          const uint8_t drl_ctx =
              av1_drl_ctx(mbmi_ext->weight[ref_frame_type], idx);
          update_cdf(fc->drl_cdf[drl_ctx], mbmi->ref_mv_idx != idx - 1, 2);
#if CONFIG_ENTROPY_STATS
          ++counts->drl_mode[drl_ctx][mbmi->ref_mv_idx != idx - 1];
#endif
          if (mbmi->ref_mv_idx == idx - 1) break;
        }
      }
    }
    if (have_newmv_in_inter_mode(mbmi->mode)) {
      const int allow_hp = cm->features.cur_frame_force_integer_mv
                               ? MV_SUBPEL_NONE
                               : cm->features.allow_high_precision_mv;
      if (new_mv) {
        for (int ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
          const int_mv ref_mv = av1_get_ref_mv(x, ref);
          av1_update_mv_stats(&mbmi->mv[ref].as_mv, &ref_mv.as_mv, &fc->nmvc,
                              allow_hp);
        }
      } else if (mbmi->mode == NEAREST_NEWMV || mbmi->mode == NEAR_NEWMV) {
        const int ref = 1;
        const int_mv ref_mv = av1_get_ref_mv(x, ref);
        av1_update_mv_stats(&mbmi->mv[ref].as_mv, &ref_mv.as_mv, &fc->nmvc,
                            allow_hp);
      } else if (mbmi->mode == NEW_NEARESTMV || mbmi->mode == NEW_NEARMV) {
        const int ref = 0;
        const int_mv ref_mv = av1_get_ref_mv(x, ref);
        av1_update_mv_stats(&mbmi->mv[ref].as_mv, &ref_mv.as_mv, &fc->nmvc,
                            allow_hp);
      }
    }
  }
}

/*!\brief Reconstructs an individual coding block
 *
 * \ingroup partition_search
 * Reconstructs an individual coding block by applying the chosen modes stored
 * in ctx, also updates mode counts and entropy models.
 *
 * \param[in]    cpi       Top-level encoder structure
 * \param[in]    tile_data Pointer to struct holding adaptive
 *                         data/contexts/models for the tile during encoding
 * \param[in]    td        Pointer to thread data
 * \param[in]    tp        Pointer to the starting token
 * \param[in]    mi_row    Row coordinate of the block in a step size of MI_SIZE
 * \param[in]    mi_col    Column coordinate of the block in a step size of
 *                         MI_SIZE
 * \param[in]    dry_run   A code indicating whether it is part of the final
 *                         pass for reconstructing the superblock
 * \param[in]    bsize     Current block size
 * \param[in]    partition Partition mode of the parent block
 * \param[in]    ctx       Pointer to structure holding coding contexts and the
 *                         chosen modes for the current block
 * \param[in]    rate      Pointer to the total rate for the current block
 *
 * \return Nothing is returned. Instead, reconstructions (w/o in-loop filters)
 * will be updated in the pixel buffers in td->mb.e_mbd. Also, the chosen modes
 * will be stored in the MB_MODE_INFO buffer td->mb.e_mbd.mi[0].
 */
static void encode_b(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                     ThreadData *td, TokenExtra **tp, int mi_row, int mi_col,
                     RUN_TYPE dry_run, BLOCK_SIZE bsize,
                     PARTITION_TYPE partition, PICK_MODE_CONTEXT *const ctx,
                     int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  const int subsampling_x = cm->seq_params.subsampling_x;
  const int subsampling_y = cm->seq_params.subsampling_y;

  av1_set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize);
  const int origin_mult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, NO_AQ, NULL);
  MB_MODE_INFO *mbmi = xd->mi[0];
  mbmi->partition = partition;
  av1_update_state(cpi, td, ctx, mi_row, mi_col, bsize, dry_run);

  if (!dry_run) {
    set_cb_offsets(x->mbmi_ext_frame->cb_offset, x->cb_offset[PLANE_TYPE_Y],
                   x->cb_offset[PLANE_TYPE_UV]);
    assert(x->cb_offset[PLANE_TYPE_Y] <
           (1 << num_pels_log2_lookup[cpi->common.seq_params.sb_size]));
    assert(x->cb_offset[PLANE_TYPE_UV] <
           ((1 << num_pels_log2_lookup[cpi->common.seq_params.sb_size]) >>
            (subsampling_x + subsampling_y)));
  }

  encode_superblock(cpi, tile_data, td, tp, dry_run, bsize, rate);

  if (!dry_run) {
    update_cb_offsets(x, bsize, subsampling_x, subsampling_y);
    if (bsize == cpi->common.seq_params.sb_size && mbmi->skip_txfm == 1 &&
        cm->delta_q_info.delta_lf_present_flag) {
      const int frame_lf_count =
          av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
      for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id)
        mbmi->delta_lf[lf_id] = xd->delta_lf[lf_id];
      mbmi->delta_lf_from_base = xd->delta_lf_from_base;
    }
    if (has_second_ref(mbmi)) {
      if (mbmi->compound_idx == 0 ||
          mbmi->interinter_comp.type == COMPOUND_AVERAGE)
        mbmi->comp_group_idx = 0;
      else
        mbmi->comp_group_idx = 1;
    }

    // delta quant applies to both intra and inter
    const int super_block_upper_left =
        ((mi_row & (cm->seq_params.mib_size - 1)) == 0) &&
        ((mi_col & (cm->seq_params.mib_size - 1)) == 0);
    const DeltaQInfo *const delta_q_info = &cm->delta_q_info;
    if (delta_q_info->delta_q_present_flag &&
        (bsize != cm->seq_params.sb_size || !mbmi->skip_txfm) &&
        super_block_upper_left) {
      xd->current_base_qindex = mbmi->current_qindex;
      if (delta_q_info->delta_lf_present_flag) {
        if (delta_q_info->delta_lf_multi) {
          const int frame_lf_count =
              av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
          for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
            xd->delta_lf[lf_id] = mbmi->delta_lf[lf_id];
          }
        } else {
          xd->delta_lf_from_base = mbmi->delta_lf_from_base;
        }
      }
    }

    RD_COUNTS *rdc = &td->rd_counts;
    if (mbmi->skip_mode) {
      assert(!frame_is_intra_only(cm));
      rdc->skip_mode_used_flag = 1;
      if (cm->current_frame.reference_mode == REFERENCE_MODE_SELECT) {
        assert(has_second_ref(mbmi));
        rdc->compound_ref_used_flag = 1;
      }
      set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    } else {
      const int seg_ref_active =
          segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_REF_FRAME);
      if (!seg_ref_active) {
        // If the segment reference feature is enabled we have only a single
        // reference frame allowed for the segment so exclude it from
        // the reference frame counts used to work out probabilities.
        if (is_inter_block(mbmi)) {
          av1_collect_neighbors_ref_counts(xd);
          if (cm->current_frame.reference_mode == REFERENCE_MODE_SELECT) {
            if (has_second_ref(mbmi)) {
              // This flag is also updated for 4x4 blocks
              rdc->compound_ref_used_flag = 1;
            }
          }
          set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
        }
      }
    }

    if (tile_data->allow_update_cdf) update_stats(&cpi->common, td);

    // Gather obmc and warped motion count to update the probability.
    if ((!cpi->sf.inter_sf.disable_obmc &&
         cpi->sf.inter_sf.prune_obmc_prob_thresh > 0) ||
        (cm->features.allow_warped_motion &&
         cpi->sf.inter_sf.prune_warped_prob_thresh > 0)) {
      const int inter_block = is_inter_block(mbmi);
      const int seg_ref_active =
          segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_REF_FRAME);
      if (!seg_ref_active && inter_block) {
        const MOTION_MODE motion_allowed =
            cm->features.switchable_motion_mode
                ? motion_mode_allowed(xd->global_motion, xd, mbmi,
                                      cm->features.allow_warped_motion)
                : SIMPLE_TRANSLATION;

        if (mbmi->ref_frame[1] != INTRA_FRAME) {
          if (motion_allowed >= OBMC_CAUSAL) {
            td->rd_counts.obmc_used[bsize][mbmi->motion_mode == OBMC_CAUSAL]++;
          }
          if (motion_allowed == WARPED_CAUSAL) {
            td->rd_counts.warped_used[mbmi->motion_mode == WARPED_CAUSAL]++;
          }
        }
      }
    }
  }
  // TODO(Ravi/Remya): Move this copy function to a better logical place
  // This function will copy the best mode information from block
  // level (x->mbmi_ext) to frame level (cpi->mbmi_ext_info.frame_base). This
  // frame level buffer (cpi->mbmi_ext_info.frame_base) will be used during
  // bitstream preparation.
  av1_copy_mbmi_ext_to_mbmi_ext_frame(x->mbmi_ext_frame, &x->mbmi_ext,
                                      av1_ref_frame_type(xd->mi[0]->ref_frame));
  x->rdmult = origin_mult;
}

/*!\brief Reconstructs a partition (may contain multiple coding blocks)
 *
 * \ingroup partition_search
 * Reconstructs a sub-partition of the superblock by applying the chosen modes
 * and partition trees stored in pc_tree.
 *
 * \param[in]    cpi       Top-level encoder structure
 * \param[in]    td        Pointer to thread data
 * \param[in]    tile_data Pointer to struct holding adaptive
 *                         data/contexts/models for the tile during encoding
 * \param[in]    tp        Pointer to the starting token
 * \param[in]    mi_row    Row coordinate of the block in a step size of MI_SIZE
 * \param[in]    mi_col    Column coordinate of the block in a step size of
 *                         MI_SIZE
 * \param[in]    dry_run   A code indicating whether it is part of the final
 *                         pass for reconstructing the superblock
 * \param[in]    bsize     Current block size
 * \param[in]    pc_tree   Pointer to the PC_TREE node storing the picked
 *                         partitions and mode info for the current block
 * \param[in]    rate      Pointer to the total rate for the current block
 *
 * \return Nothing is returned. Instead, reconstructions (w/o in-loop filters)
 * will be updated in the pixel buffers in td->mb.e_mbd.
 */
static void encode_sb(const AV1_COMP *const cpi, ThreadData *td,
                      TileDataEnc *tile_data, TokenExtra **tp, int mi_row,
                      int mi_col, RUN_TYPE dry_run, BLOCK_SIZE bsize,
                      PC_TREE *pc_tree, int *rate) {
  assert(bsize < BLOCK_SIZES_ALL);
  const AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  assert(bsize < BLOCK_SIZES_ALL);
  const int hbs = mi_size_wide[bsize] / 2;
  const int is_partition_root = bsize >= BLOCK_8X8;
  const int ctx = is_partition_root
                      ? partition_plane_context(xd, mi_row, mi_col, bsize)
                      : -1;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
  int quarter_step = mi_size_wide[bsize] / 4;
  int i;
  BLOCK_SIZE bsize2 = get_partition_subsize(bsize, PARTITION_SPLIT);

  if (mi_row >= mi_params->mi_rows || mi_col >= mi_params->mi_cols) return;
  if (subsize == BLOCK_INVALID) return;

  if (!dry_run && ctx >= 0) {
    const int has_rows = (mi_row + hbs) < mi_params->mi_rows;
    const int has_cols = (mi_col + hbs) < mi_params->mi_cols;

    if (has_rows && has_cols) {
#if CONFIG_ENTROPY_STATS
      td->counts->partition[ctx][partition]++;
#endif

      if (tile_data->allow_update_cdf) {
        FRAME_CONTEXT *fc = xd->tile_ctx;
        update_cdf(fc->partition_cdf[ctx], partition,
                   partition_cdf_length(bsize));
      }
    }
  }

  switch (partition) {
    case PARTITION_NONE:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->none, rate);
      break;
    case PARTITION_VERT:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->vertical[0], rate);
      if (mi_col + hbs < mi_params->mi_cols) {
        encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
                 partition, pc_tree->vertical[1], rate);
      }
      break;
    case PARTITION_HORZ:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->horizontal[0], rate);
      if (mi_row + hbs < mi_params->mi_rows) {
        encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
                 partition, pc_tree->horizontal[1], rate);
      }
      break;
    case PARTITION_SPLIT:
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, subsize,
                pc_tree->split[0], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + hbs, dry_run, subsize,
                pc_tree->split[1], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs, mi_col, dry_run, subsize,
                pc_tree->split[2], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs, mi_col + hbs, dry_run,
                subsize, pc_tree->split[3], rate);
      break;

    case PARTITION_HORZ_A:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, bsize2,
               partition, pc_tree->horizontala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, bsize2,
               partition, pc_tree->horizontala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, subsize,
               partition, pc_tree->horizontala[2], rate);
      break;
    case PARTITION_HORZ_B:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->horizontalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, bsize2,
               partition, pc_tree->horizontalb[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col + hbs, dry_run,
               bsize2, partition, pc_tree->horizontalb[2], rate);
      break;
    case PARTITION_VERT_A:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, bsize2,
               partition, pc_tree->verticala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, dry_run, bsize2,
               partition, pc_tree->verticala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, subsize,
               partition, pc_tree->verticala[2], rate);

      break;
    case PARTITION_VERT_B:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->verticalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, dry_run, bsize2,
               partition, pc_tree->verticalb[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col + hbs, dry_run,
               bsize2, partition, pc_tree->verticalb[2], rate);
      break;
    case PARTITION_HORZ_4:
      for (i = 0; i < SUB_PARTITIONS_PART4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= mi_params->mi_rows) break;

        encode_b(cpi, tile_data, td, tp, this_mi_row, mi_col, dry_run, subsize,
                 partition, pc_tree->horizontal4[i], rate);
      }
      break;
    case PARTITION_VERT_4:
      for (i = 0; i < SUB_PARTITIONS_PART4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= mi_params->mi_cols) break;
        encode_b(cpi, tile_data, td, tp, mi_row, this_mi_col, dry_run, subsize,
                 partition, pc_tree->vertical4[i], rate);
      }
      break;
    default: assert(0 && "Invalid partition type."); break;
  }

  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
}

/*!\brief AV1 block partition search (partition estimation and partial search).
*
* \ingroup partition_search
* Encode the block by applying pre-calculated partition patterns that are
* represented by coding block sizes stored in the mbmi array. Minor partition
* adjustments are tested and applied if they lead to lower rd costs. The
* partition types are limited to a basic set: none, horz, vert, and split.
*
* \param[in]    cpi       Top-level encoder structure
* \param[in]    td        Pointer to thread data
* \param[in]    tile_data Pointer to struct holding adaptive
data/contexts/models for the tile during encoding
* \param[in]    mib       Array representing MB_MODE_INFO pointers for mi
blocks starting from the first pixel of the current
block
* \param[in]    tp        Pointer to the starting token
* \param[in]    mi_row    Row coordinate of the block in a step size of MI_SIZE
* \param[in]    mi_col    Column coordinate of the block in a step size of
MI_SIZE
* \param[in]    bsize     Current block size
* \param[in]    rate      Pointer to the final rate for encoding the current
block
* \param[in]    dist      Pointer to the final distortion of the current block
* \param[in]    do_recon  Whether the reconstruction function needs to be run,
either for finalizing a superblock or providing
reference for future sub-partitions
* \param[in]    pc_tree   Pointer to the PC_TREE node holding the picked
partitions and mode info for the current block
*
* \return Nothing is returned. The pc_tree struct is modified to store the
* picked partition and modes. The rate and dist are also updated with those
* corresponding to the best partition found.
*/
void av1_rd_use_partition(AV1_COMP *cpi, ThreadData *td, TileDataEnc *tile_data,
                          MB_MODE_INFO **mib, TokenExtra **tp, int mi_row,
                          int mi_col, BLOCK_SIZE bsize, int *rate,
                          int64_t *dist, int do_recon, PC_TREE *pc_tree) {
  AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const int num_planes = av1_num_planes(cm);
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const ModeCosts *mode_costs = &x->mode_costs;
  const int bs = mi_size_wide[bsize];
  const int hbs = bs / 2;
  const int pl = (bsize >= BLOCK_8X8)
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;
  const PARTITION_TYPE partition =
      (bsize >= BLOCK_8X8) ? get_partition(cm, mi_row, mi_col, bsize)
                           : PARTITION_NONE;
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  RD_STATS last_part_rdc, none_rdc, chosen_rdc, invalid_rdc;
  BLOCK_SIZE sub_subsize = BLOCK_4X4;
  int splits_below = 0;
  BLOCK_SIZE bs_type = mib[0]->bsize;

  if (pc_tree->none == NULL) {
    pc_tree->none = av1_alloc_pmc(cm, bsize, &td->shared_coeff_buf);
  }
  PICK_MODE_CONTEXT *ctx_none = pc_tree->none;

  if (mi_row >= mi_params->mi_rows || mi_col >= mi_params->mi_cols) return;

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  av1_invalid_rd_stats(&last_part_rdc);
  av1_invalid_rd_stats(&none_rdc);
  av1_invalid_rd_stats(&chosen_rdc);
  av1_invalid_rd_stats(&invalid_rdc);

  pc_tree->partitioning = partition;

  xd->above_txfm_context =
      cm->above_contexts.txfm[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  av1_save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh) {
    av1_set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);
    x->mb_energy = av1_log_block_var(cpi, x, bsize);
  }

  // Save rdmult before it might be changed, so it can be restored later.
  const int orig_rdmult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, NO_AQ, NULL);

  if (cpi->sf.part_sf.partition_search_type == VAR_BASED_PARTITION &&
      ((cpi->sf.part_sf.adjust_var_based_rd_partitioning == 2 &&
        bsize <= BLOCK_32X32) ||
       (cpi->sf.part_sf.adjust_var_based_rd_partitioning == 1 &&
        cm->quant_params.base_qindex > 190 && bsize <= BLOCK_32X32 &&
        !frame_is_intra_only(cm)))) {
    // Check if any of the sub blocks are further split.
    if (partition == PARTITION_SPLIT && subsize > BLOCK_8X8) {
      sub_subsize = get_partition_subsize(subsize, PARTITION_SPLIT);
      splits_below = 1;
      for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
        int jj = i >> 1, ii = i & 0x01;
        MB_MODE_INFO *this_mi = mib[jj * hbs * mi_params->mi_stride + ii * hbs];
        if (this_mi && this_mi->bsize >= sub_subsize) {
          splits_below = 0;
        }
      }
    }

    // If partition is not none try none unless each of the 4 splits are split
    // even further..
    if (partition != PARTITION_NONE && !splits_below &&
        mi_row + hbs < mi_params->mi_rows &&
        mi_col + hbs < mi_params->mi_cols) {
      pc_tree->partitioning = PARTITION_NONE;
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &none_rdc,
                    PARTITION_NONE, bsize, ctx_none, invalid_rdc);

      if (none_rdc.rate < INT_MAX) {
        none_rdc.rate += mode_costs->partition_cost[pl][PARTITION_NONE];
        none_rdc.rdcost = RDCOST(x->rdmult, none_rdc.rate, none_rdc.dist);
      }

      av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      mib[0]->bsize = bs_type;
      pc_tree->partitioning = partition;
    }
  }

  for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
    pc_tree->split[i] = av1_alloc_pc_tree_node(subsize);
    pc_tree->split[i]->index = i;
  }
  switch (partition) {
    case PARTITION_NONE:
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                    PARTITION_NONE, bsize, ctx_none, invalid_rdc);
      break;
    case PARTITION_HORZ:
      for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
        pc_tree->horizontal[i] =
            av1_alloc_pmc(cm, subsize, &td->shared_coeff_buf);
      }
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                    PARTITION_HORZ, subsize, pc_tree->horizontal[0],
                    invalid_rdc);
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_row + hbs < mi_params->mi_rows) {
        RD_STATS tmp_rdc;
        const PICK_MODE_CONTEXT *const ctx_h = pc_tree->horizontal[0];
        av1_init_rd_stats(&tmp_rdc);
        av1_update_state(cpi, td, ctx_h, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, subsize,
                          NULL);
        pick_sb_modes(cpi, tile_data, x, mi_row + hbs, mi_col, &tmp_rdc,
                      PARTITION_HORZ, subsize, pc_tree->horizontal[1],
                      invalid_rdc);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
      }
      break;
    case PARTITION_VERT:
      for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
        pc_tree->vertical[i] =
            av1_alloc_pmc(cm, subsize, &td->shared_coeff_buf);
      }
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                    PARTITION_VERT, subsize, pc_tree->vertical[0], invalid_rdc);
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_col + hbs < mi_params->mi_cols) {
        RD_STATS tmp_rdc;
        const PICK_MODE_CONTEXT *const ctx_v = pc_tree->vertical[0];
        av1_init_rd_stats(&tmp_rdc);
        av1_update_state(cpi, td, ctx_v, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, subsize,
                          NULL);
        pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + hbs, &tmp_rdc,
                      PARTITION_VERT, subsize,
                      pc_tree->vertical[bsize > BLOCK_8X8], invalid_rdc);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
      }
      break;
    case PARTITION_SPLIT:
      if (cpi->sf.part_sf.adjust_var_based_rd_partitioning == 1 &&
          none_rdc.rate < INT_MAX && none_rdc.skip_txfm == 1) {
        av1_invalid_rd_stats(&last_part_rdc);
        break;
      }
      last_part_rdc.rate = 0;
      last_part_rdc.dist = 0;
      last_part_rdc.rdcost = 0;
      for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
        int x_idx = (i & 1) * hbs;
        int y_idx = (i >> 1) * hbs;
        int jj = i >> 1, ii = i & 0x01;
        RD_STATS tmp_rdc;
        if ((mi_row + y_idx >= mi_params->mi_rows) ||
            (mi_col + x_idx >= mi_params->mi_cols))
          continue;

        av1_init_rd_stats(&tmp_rdc);
        av1_rd_use_partition(
            cpi, td, tile_data,
            mib + jj * hbs * mi_params->mi_stride + ii * hbs, tp,
            mi_row + y_idx, mi_col + x_idx, subsize, &tmp_rdc.rate,
            &tmp_rdc.dist, i != (SUB_PARTITIONS_SPLIT - 1), pc_tree->split[i]);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
      }
      break;
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
    case PARTITION_HORZ_4:
    case PARTITION_VERT_4:
      assert(0 && "Cannot handle extended partition types");
    default: assert(0); break;
  }

  if (last_part_rdc.rate < INT_MAX) {
    last_part_rdc.rate += mode_costs->partition_cost[pl][partition];
    last_part_rdc.rdcost =
        RDCOST(x->rdmult, last_part_rdc.rate, last_part_rdc.dist);
  }

  if ((cpi->sf.part_sf.partition_search_type == VAR_BASED_PARTITION &&
       cpi->sf.part_sf.adjust_var_based_rd_partitioning > 2) &&
      partition != PARTITION_SPLIT && bsize > BLOCK_8X8 &&
      (mi_row + bs < mi_params->mi_rows ||
       mi_row + hbs == mi_params->mi_rows) &&
      (mi_col + bs < mi_params->mi_cols ||
       mi_col + hbs == mi_params->mi_cols)) {
    BLOCK_SIZE split_subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    chosen_rdc.rate = 0;
    chosen_rdc.dist = 0;

    av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
    pc_tree->partitioning = PARTITION_SPLIT;

    // Split partition.
    for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
      int x_idx = (i & 1) * hbs;
      int y_idx = (i >> 1) * hbs;
      RD_STATS tmp_rdc;

      if ((mi_row + y_idx >= mi_params->mi_rows) ||
          (mi_col + x_idx >= mi_params->mi_cols))
        continue;

      av1_save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      pc_tree->split[i]->partitioning = PARTITION_NONE;
      if (pc_tree->split[i]->none == NULL)
        pc_tree->split[i]->none =
            av1_alloc_pmc(cm, split_subsize, &td->shared_coeff_buf);
      pick_sb_modes(cpi, tile_data, x, mi_row + y_idx, mi_col + x_idx, &tmp_rdc,
                    PARTITION_SPLIT, split_subsize, pc_tree->split[i]->none,
                    invalid_rdc);

      av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
        av1_invalid_rd_stats(&chosen_rdc);
        break;
      }

      chosen_rdc.rate += tmp_rdc.rate;
      chosen_rdc.dist += tmp_rdc.dist;

      if (i != SUB_PARTITIONS_SPLIT - 1)
        encode_sb(cpi, td, tile_data, tp, mi_row + y_idx, mi_col + x_idx,
                  OUTPUT_ENABLED, split_subsize, pc_tree->split[i], NULL);

      chosen_rdc.rate += mode_costs->partition_cost[pl][PARTITION_NONE];
    }
    if (chosen_rdc.rate < INT_MAX) {
      chosen_rdc.rate += mode_costs->partition_cost[pl][PARTITION_SPLIT];
      chosen_rdc.rdcost = RDCOST(x->rdmult, chosen_rdc.rate, chosen_rdc.dist);
    }
  }

  // If last_part is better set the partitioning to that.
  if (last_part_rdc.rdcost < chosen_rdc.rdcost) {
    mib[0]->bsize = bsize;
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = partition;
    chosen_rdc = last_part_rdc;
  }
  // If none was better set the partitioning to that.
  if (none_rdc.rdcost < chosen_rdc.rdcost) {
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = PARTITION_NONE;
    chosen_rdc = none_rdc;
  }

  av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  // We must have chosen a partitioning and encoding or we'll fail later on.
  // No other opportunities for success.
  if (bsize == cm->seq_params.sb_size)
    assert(chosen_rdc.rate < INT_MAX && chosen_rdc.dist < INT64_MAX);

  if (do_recon) {
    if (bsize == cm->seq_params.sb_size) {
      // NOTE: To get estimate for rate due to the tokens, use:
      // int rate_coeffs = 0;
      // encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_COSTCOEFFS,
      //           bsize, pc_tree, &rate_coeffs);
      set_cb_offsets(x->cb_offset, 0, 0);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, NULL);
    } else {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

  *rate = chosen_rdc.rate;
  *dist = chosen_rdc.dist;
  x->rdmult = orig_rdmult;
}

static void encode_b_nonrd(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                           ThreadData *td, TokenExtra **tp, int mi_row,
                           int mi_col, RUN_TYPE dry_run, BLOCK_SIZE bsize,
                           PARTITION_TYPE partition,
                           PICK_MODE_CONTEXT *const ctx, int *rate) {
  TileInfo *const tile = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  av1_set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize);
  const int origin_mult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, NO_AQ, NULL);
  MB_MODE_INFO *mbmi = xd->mi[0];
  mbmi->partition = partition;
  // Nonrd pickmode does not currently support second/combined reference.
  assert(!has_second_ref(mbmi));
  av1_update_state(cpi, td, ctx, mi_row, mi_col, bsize, dry_run);
  const int subsampling_x = cpi->common.seq_params.subsampling_x;
  const int subsampling_y = cpi->common.seq_params.subsampling_y;
  if (!dry_run) {
    set_cb_offsets(x->mbmi_ext_frame->cb_offset, x->cb_offset[PLANE_TYPE_Y],
                   x->cb_offset[PLANE_TYPE_UV]);
    assert(x->cb_offset[PLANE_TYPE_Y] <
           (1 << num_pels_log2_lookup[cpi->common.seq_params.sb_size]));
    assert(x->cb_offset[PLANE_TYPE_UV] <
           ((1 << num_pels_log2_lookup[cpi->common.seq_params.sb_size]) >>
            (subsampling_x + subsampling_y)));
  }
  encode_superblock(cpi, tile_data, td, tp, dry_run, bsize, rate);
  if (!dry_run) {
    update_cb_offsets(x, bsize, subsampling_x, subsampling_y);
    if (tile_data->allow_update_cdf) update_stats(&cpi->common, td);
  }
  // TODO(Ravi/Remya): Move this copy function to a better logical place
  // This function will copy the best mode information from block
  // level (x->mbmi_ext) to frame level (cpi->mbmi_ext_info.frame_base). This
  // frame level buffer (cpi->mbmi_ext_info.frame_base) will be used during
  // bitstream preparation.
  av1_copy_mbmi_ext_to_mbmi_ext_frame(x->mbmi_ext_frame, &x->mbmi_ext,
                                      av1_ref_frame_type(xd->mi[0]->ref_frame));
  x->rdmult = origin_mult;
}
/*!\brief Top level function to pick block mode for non-RD optimized case
 *
 * \ingroup partition_search
 * \callgraph
 * \callergraph
 * Searches prediction modes, transform, and coefficient coding modes for an
 * individual coding block. This function is the top-level function that is
 * used for non-RD optimized mode search (controlled by
 * \c cpi->sf.rt_sf.use_nonrd_pick_mode). Depending on frame type it calls
 * inter/skip/hybrid-intra mode search functions
 *
 * \param[in]    cpi            Top-level encoder structure
 * \param[in]    tile_data      Pointer to struct holding adaptive
 *                              data/contexts/models for the tile during
 *                              encoding
 * \param[in]    x              Pointer to structure holding all the data for
 *                              the current macroblock
 * \param[in]    mi_row         Row coordinate of the block in a step size of
 *                              MI_SIZE
 * \param[in]    mi_col         Column coordinate of the block in a step size of
 *                              MI_SIZE
 * \param[in]    rd_cost        Pointer to structure holding rate and distortion
 *                              stats for the current block
 * \param[in]    bsize          Current block size
 * \param[in]    ctx            Pointer to structure holding coding contexts and
 *                              chosen modes for the current block
 *
 * \return Nothing is returned. Instead, the chosen modes and contexts necessary
 * for reconstruction are stored in ctx, the rate-distortion stats are stored in
 * rd_cost. If no valid mode leading to rd_cost <= best_rd, the status will be
 * signalled by an INT64_MAX rd_cost->rdcost.
 */
static void pick_sb_modes_nonrd(AV1_COMP *const cpi, TileDataEnc *tile_data,
                                MACROBLOCK *const x, int mi_row, int mi_col,
                                RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                PICK_MODE_CONTEXT *ctx) {
  av1_set_offsets(cpi, &tile_data->tile_info, x, mi_row, mi_col, bsize);
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const AQ_MODE aq_mode = cpi->oxcf.q_cfg.aq_mode;
  TxfmSearchInfo *txfm_info = &x->txfm_search_info;
  int i;
#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, rd_pick_sb_modes_time);
#endif
  aom_clear_system_state();
  // Sets up the tx_type_map buffer in MACROBLOCKD.
  xd->tx_type_map = txfm_info->tx_type_map_;
  xd->tx_type_map_stride = mi_size_wide[bsize];
  for (i = 0; i < num_planes; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    p[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
  }
  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];
  if (is_cur_buf_hbd(xd)) {
    x->source_variance = av1_high_get_sby_perpixel_variance(
        cpi, &x->plane[0].src, bsize, xd->bd);
  } else {
    x->source_variance =
        av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
  }
  // Save rdmult before it might be changed, so it can be restored later.
  const int orig_rdmult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, aq_mode, mbmi);
  // Set error per bit for current rdmult
  av1_set_error_per_bit(&x->mv_costs, x->rdmult);
  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, av1_rd_pick_intra_mode_sb_time);
#endif
    hybrid_intra_mode_search(cpi, x, rd_cost, bsize, ctx);
#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, av1_rd_pick_intra_mode_sb_time);
#endif
  } else {
#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, av1_rd_pick_inter_mode_sb_time);
#endif
    if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      RD_STATS invalid_rd;
      av1_invalid_rd_stats(&invalid_rd);
      // TODO(kyslov): add av1_nonrd_pick_inter_mode_sb_seg_skip
      av1_rd_pick_inter_mode_sb_seg_skip(cpi, tile_data, x, mi_row, mi_col,
                                         rd_cost, bsize, ctx,
                                         invalid_rd.rdcost);
    } else {
      av1_nonrd_pick_inter_mode_sb(cpi, tile_data, x, rd_cost, bsize, ctx);
    }
#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, av1_rd_pick_inter_mode_sb_time);
#endif
  }
  x->rdmult = orig_rdmult;
  ctx->rd_stats.rate = rd_cost->rate;
  ctx->rd_stats.dist = rd_cost->dist;
  ctx->rd_stats.rdcost = rd_cost->rdcost;
#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, rd_pick_sb_modes_time);
#endif
}

/*!\brief AV1 block partition application (minimal RD search).
*
* \ingroup partition_search
* \callgraph
* \callergraph
* Encode the block by applying pre-calculated partition patterns that are
* represented by coding block sizes stored in the mbmi array. The only
* partition adjustment allowed is merging leaf split nodes if it leads to a
* lower rd cost. The partition types are limited to a basic set: none, horz,
* vert, and split. This function is only used in the real-time mode.
*
* \param[in]    cpi       Top-level encoder structure
* \param[in]    td        Pointer to thread data
* \param[in]    tile_data Pointer to struct holding adaptive
data/contexts/models for the tile during encoding
* \param[in]    mib       Array representing MB_MODE_INFO pointers for mi
blocks starting from the first pixel of the current
block
* \param[in]    tp        Pointer to the starting token
* \param[in]    mi_row    Row coordinate of the block in a step size of MI_SIZE
* \param[in]    mi_col    Column coordinate of the block in a step size of
MI_SIZE
* \param[in]    bsize     Current block size
* \param[in]    pc_tree   Pointer to the PC_TREE node holding the picked
partitions and mode info for the current block
*
* \return Nothing is returned. The pc_tree struct is modified to store the
* picked partition and modes.
*/
void av1_nonrd_use_partition(AV1_COMP *cpi, ThreadData *td,
                             TileDataEnc *tile_data, MB_MODE_INFO **mib,
                             TokenExtra **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, PC_TREE *pc_tree) {
  AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const ModeCosts *mode_costs = &x->mode_costs;
  // Only square blocks from 8x8 to 128x128 are supported
  assert(bsize >= BLOCK_8X8 && bsize <= BLOCK_128X128);
  const int bs = mi_size_wide[bsize];
  const int hbs = bs / 2;
  const PARTITION_TYPE partition =
      (bsize >= BLOCK_8X8) ? get_partition(cm, mi_row, mi_col, bsize)
                           : PARTITION_NONE;
  BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
  assert(subsize <= BLOCK_LARGEST);
  const int pl = (bsize >= BLOCK_8X8)
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;

  RD_STATS dummy_cost;
  av1_invalid_rd_stats(&dummy_cost);

  if (mi_row >= mi_params->mi_rows || mi_col >= mi_params->mi_cols) return;

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  pc_tree->partitioning = partition;

  xd->above_txfm_context =
      cm->above_contexts.txfm[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  // Initialize default mode evaluation params
  set_mode_eval_params(cpi, x, DEFAULT_EVAL);

  switch (partition) {
    case PARTITION_NONE:
      pc_tree->none = av1_alloc_pmc(cm, bsize, &td->shared_coeff_buf);
      if (cpi->sf.rt_sf.nonrd_check_partition_split && do_slipt_check(bsize) &&
          !frame_is_intra_only(cm)) {
        RD_STATS split_rdc, none_rdc, block_rdc;
        RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;

        av1_init_rd_stats(&split_rdc);
        av1_invalid_rd_stats(&none_rdc);

        av1_save_context(x, &x_ctx, mi_row, mi_col, bsize, 3);
        subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
        pick_sb_modes_nonrd(cpi, tile_data, x, mi_row, mi_col, &none_rdc, bsize,
                            pc_tree->none);
        none_rdc.rate += mode_costs->partition_cost[pl][PARTITION_NONE];
        none_rdc.rdcost = RDCOST(x->rdmult, none_rdc.rate, none_rdc.dist);
        av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, 3);

        for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
          av1_invalid_rd_stats(&block_rdc);
          const int x_idx = (i & 1) * hbs;
          const int y_idx = (i >> 1) * hbs;
          if (mi_row + y_idx >= mi_params->mi_rows ||
              mi_col + x_idx >= mi_params->mi_cols)
            continue;
          xd->above_txfm_context =
              cm->above_contexts.txfm[tile_info->tile_row] + mi_col + x_idx;
          xd->left_txfm_context =
              xd->left_txfm_context_buffer + ((mi_row + y_idx) & MAX_MIB_MASK);
          pc_tree->split[i]->partitioning = PARTITION_NONE;
          pick_sb_modes_nonrd(cpi, tile_data, x, mi_row + y_idx, mi_col + x_idx,
                              &block_rdc, subsize, pc_tree->split[i]->none);
          split_rdc.rate += block_rdc.rate;
          split_rdc.dist += block_rdc.dist;

          encode_b_nonrd(cpi, tile_data, td, tp, mi_row + y_idx, mi_col + x_idx,
                         1, subsize, PARTITION_NONE, pc_tree->split[i]->none,
                         NULL);
        }
        split_rdc.rate += mode_costs->partition_cost[pl][PARTITION_SPLIT];
        split_rdc.rdcost = RDCOST(x->rdmult, split_rdc.rate, split_rdc.dist);
        av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, 3);

        if (none_rdc.rdcost < split_rdc.rdcost) {
          mib[0]->bsize = bsize;
          pc_tree->partitioning = PARTITION_NONE;
          encode_b_nonrd(cpi, tile_data, td, tp, mi_row, mi_col, 0, bsize,
                         partition, pc_tree->none, NULL);
        } else {
          mib[0]->bsize = subsize;
          pc_tree->partitioning = PARTITION_SPLIT;
          for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
            const int x_idx = (i & 1) * hbs;
            const int y_idx = (i >> 1) * hbs;
            if (mi_row + y_idx >= mi_params->mi_rows ||
                mi_col + x_idx >= mi_params->mi_cols)
              continue;
            encode_b_nonrd(cpi, tile_data, td, tp, mi_row + y_idx,
                           mi_col + x_idx, 0, subsize, PARTITION_NONE,
                           pc_tree->split[i]->none, NULL);
          }
        }

      } else {
        pick_sb_modes_nonrd(cpi, tile_data, x, mi_row, mi_col, &dummy_cost,
                            bsize, pc_tree->none);
        encode_b_nonrd(cpi, tile_data, td, tp, mi_row, mi_col, 0, bsize,
                       partition, pc_tree->none, NULL);
      }
      break;
    case PARTITION_VERT:
      for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
        pc_tree->vertical[i] =
            av1_alloc_pmc(cm, subsize, &td->shared_coeff_buf);
      }
      pick_sb_modes_nonrd(cpi, tile_data, x, mi_row, mi_col, &dummy_cost,
                          subsize, pc_tree->vertical[0]);
      encode_b_nonrd(cpi, tile_data, td, tp, mi_row, mi_col, 0, subsize,
                     PARTITION_VERT, pc_tree->vertical[0], NULL);
      if (mi_col + hbs < mi_params->mi_cols && bsize > BLOCK_8X8) {
        pick_sb_modes_nonrd(cpi, tile_data, x, mi_row, mi_col + hbs,
                            &dummy_cost, subsize, pc_tree->vertical[1]);
        encode_b_nonrd(cpi, tile_data, td, tp, mi_row, mi_col + hbs, 0, subsize,
                       PARTITION_VERT, pc_tree->vertical[1], NULL);
      }
      break;
    case PARTITION_HORZ:
      for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
        pc_tree->horizontal[i] =
            av1_alloc_pmc(cm, subsize, &td->shared_coeff_buf);
      }
      pick_sb_modes_nonrd(cpi, tile_data, x, mi_row, mi_col, &dummy_cost,
                          subsize, pc_tree->horizontal[0]);
      encode_b_nonrd(cpi, tile_data, td, tp, mi_row, mi_col, 0, subsize,
                     PARTITION_HORZ, pc_tree->horizontal[0], NULL);

      if (mi_row + hbs < mi_params->mi_rows && bsize > BLOCK_8X8) {
        pick_sb_modes_nonrd(cpi, tile_data, x, mi_row + hbs, mi_col,
                            &dummy_cost, subsize, pc_tree->horizontal[1]);
        encode_b_nonrd(cpi, tile_data, td, tp, mi_row + hbs, mi_col, 0, subsize,
                       PARTITION_HORZ, pc_tree->horizontal[1], NULL);
      }
      break;
    case PARTITION_SPLIT:
      for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
        pc_tree->split[i] = av1_alloc_pc_tree_node(subsize);
        pc_tree->split[i]->index = i;
      }
      if (cpi->sf.rt_sf.nonrd_check_partition_merge_mode &&
          av1_is_leaf_split_partition(cm, mi_row, mi_col, bsize) &&
          !frame_is_intra_only(cm) && bsize <= BLOCK_64X64) {
        RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
        RD_STATS split_rdc, none_rdc;
        av1_invalid_rd_stats(&split_rdc);
        av1_invalid_rd_stats(&none_rdc);
        av1_save_context(x, &x_ctx, mi_row, mi_col, bsize, 3);
        xd->above_txfm_context =
            cm->above_contexts.txfm[tile_info->tile_row] + mi_col;
        xd->left_txfm_context =
            xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
        pc_tree->partitioning = PARTITION_NONE;
        pc_tree->none = av1_alloc_pmc(cm, bsize, &td->shared_coeff_buf);
        pick_sb_modes_nonrd(cpi, tile_data, x, mi_row, mi_col, &none_rdc, bsize,
                            pc_tree->none);
        none_rdc.rate += mode_costs->partition_cost[pl][PARTITION_NONE];
        none_rdc.rdcost = RDCOST(x->rdmult, none_rdc.rate, none_rdc.dist);
        av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, 3);
        if (cpi->sf.rt_sf.nonrd_check_partition_merge_mode != 2 ||
            none_rdc.skip_txfm != 1 || pc_tree->none->mic.mode == NEWMV) {
          av1_init_rd_stats(&split_rdc);
          for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
            RD_STATS block_rdc;
            av1_invalid_rd_stats(&block_rdc);
            int x_idx = (i & 1) * hbs;
            int y_idx = (i >> 1) * hbs;
            if ((mi_row + y_idx >= mi_params->mi_rows) ||
                (mi_col + x_idx >= mi_params->mi_cols))
              continue;
            xd->above_txfm_context =
                cm->above_contexts.txfm[tile_info->tile_row] + mi_col + x_idx;
            xd->left_txfm_context = xd->left_txfm_context_buffer +
                                    ((mi_row + y_idx) & MAX_MIB_MASK);
            if (pc_tree->split[i]->none == NULL)
              pc_tree->split[i]->none =
                  av1_alloc_pmc(cm, subsize, &td->shared_coeff_buf);
            pc_tree->split[i]->partitioning = PARTITION_NONE;
            pick_sb_modes_nonrd(cpi, tile_data, x, mi_row + y_idx,
                                mi_col + x_idx, &block_rdc, subsize,
                                pc_tree->split[i]->none);
            split_rdc.rate += block_rdc.rate;
            split_rdc.dist += block_rdc.dist;

            encode_b_nonrd(cpi, tile_data, td, tp, mi_row + y_idx,
                           mi_col + x_idx, 1, subsize, PARTITION_NONE,
                           pc_tree->split[i]->none, NULL);
          }
          av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, 3);
          split_rdc.rate += mode_costs->partition_cost[pl][PARTITION_SPLIT];
          split_rdc.rdcost = RDCOST(x->rdmult, split_rdc.rate, split_rdc.dist);
        }
        if (none_rdc.rdcost < split_rdc.rdcost) {
          mib[0]->bsize = bsize;
          pc_tree->partitioning = PARTITION_NONE;
          encode_b_nonrd(cpi, tile_data, td, tp, mi_row, mi_col, 0, bsize,
                         partition, pc_tree->none, NULL);
        } else {
          mib[0]->bsize = subsize;
          pc_tree->partitioning = PARTITION_SPLIT;
          for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
            int x_idx = (i & 1) * hbs;
            int y_idx = (i >> 1) * hbs;
            if ((mi_row + y_idx >= mi_params->mi_rows) ||
                (mi_col + x_idx >= mi_params->mi_cols))
              continue;

            if (pc_tree->split[i]->none == NULL)
              pc_tree->split[i]->none =
                  av1_alloc_pmc(cm, subsize, &td->shared_coeff_buf);
            encode_b_nonrd(cpi, tile_data, td, tp, mi_row + y_idx,
                           mi_col + x_idx, 0, subsize, PARTITION_NONE,
                           pc_tree->split[i]->none, NULL);
          }
        }
      } else {
        for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
          int x_idx = (i & 1) * hbs;
          int y_idx = (i >> 1) * hbs;
          int jj = i >> 1, ii = i & 0x01;
          if ((mi_row + y_idx >= mi_params->mi_rows) ||
              (mi_col + x_idx >= mi_params->mi_cols))
            continue;
          av1_nonrd_use_partition(
              cpi, td, tile_data,
              mib + jj * hbs * mi_params->mi_stride + ii * hbs, tp,
              mi_row + y_idx, mi_col + x_idx, subsize, pc_tree->split[i]);
        }
      }
      break;
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
    case PARTITION_HORZ_4:
    case PARTITION_VERT_4:
      assert(0 && "Cannot handle extended partition types");
    default: assert(0); break;
  }
}

#if !CONFIG_REALTIME_ONLY
// Try searching for an encoding for the given subblock. Returns zero if the
// rdcost is already too high (to tell the caller not to bother searching for
// encodings of further subblocks).
static int rd_try_subblock(AV1_COMP *const cpi, ThreadData *td,
                           TileDataEnc *tile_data, TokenExtra **tp, int is_last,
                           int mi_row, int mi_col, BLOCK_SIZE subsize,
                           RD_STATS best_rdcost, RD_STATS *sum_rdc,
                           PARTITION_TYPE partition,
                           PICK_MODE_CONTEXT *this_ctx) {
  MACROBLOCK *const x = &td->mb;
  const int orig_mult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, subsize, NO_AQ, NULL);

  av1_rd_cost_update(x->rdmult, &best_rdcost);

  RD_STATS rdcost_remaining;
  av1_rd_stats_subtraction(x->rdmult, &best_rdcost, sum_rdc, &rdcost_remaining);
  RD_STATS this_rdc;
  pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc, partition,
                subsize, this_ctx, rdcost_remaining);

  if (this_rdc.rate == INT_MAX) {
    sum_rdc->rdcost = INT64_MAX;
  } else {
    sum_rdc->rate += this_rdc.rate;
    sum_rdc->dist += this_rdc.dist;
    av1_rd_cost_update(x->rdmult, sum_rdc);
  }

  if (sum_rdc->rdcost >= best_rdcost.rdcost) {
    x->rdmult = orig_mult;
    return 0;
  }

  if (!is_last) {
    av1_update_state(cpi, td, this_ctx, mi_row, mi_col, subsize, 1);
    encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, subsize, NULL);
  }

  x->rdmult = orig_mult;
  return 1;
}

// Tests an AB partition, and updates the encoder status, the pick mode
// contexts, the best rdcost, and the best partition.
static bool rd_test_partition3(AV1_COMP *const cpi, ThreadData *td,
                               TileDataEnc *tile_data, TokenExtra **tp,
                               PC_TREE *pc_tree, RD_STATS *best_rdc,
                               PICK_MODE_CONTEXT *ctxs[SUB_PARTITIONS_AB],
                               int mi_row, int mi_col, BLOCK_SIZE bsize,
                               PARTITION_TYPE partition,
                               const BLOCK_SIZE ab_subsize[SUB_PARTITIONS_AB],
                               const int ab_mi_pos[SUB_PARTITIONS_AB][2],
                               const MB_MODE_INFO **mode_cache) {
  MACROBLOCK *const x = &td->mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
  RD_STATS sum_rdc;
  av1_init_rd_stats(&sum_rdc);
  sum_rdc.rate = x->mode_costs.partition_cost[pl][partition];
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);
  // Loop over sub-partitions in AB partition type.
  for (int i = 0; i < SUB_PARTITIONS_AB; i++) {
    if (mode_cache && mode_cache[i]) {
      x->use_intermode_cache = 1;
      x->intermode_cache = mode_cache[i];
    }
    const int mode_search_success =
        rd_try_subblock(cpi, td, tile_data, tp, i == SUB_PARTITIONS_AB - 1,
                        ab_mi_pos[i][0], ab_mi_pos[i][1], ab_subsize[i],
                        *best_rdc, &sum_rdc, partition, ctxs[i]);
    x->use_intermode_cache = 0;
    x->intermode_cache = NULL;
    if (!mode_search_success) {
      return false;
    }
  }

  av1_rd_cost_update(x->rdmult, &sum_rdc);
  if (sum_rdc.rdcost >= best_rdc->rdcost) return false;
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
  if (sum_rdc.rdcost >= best_rdc->rdcost) return false;

  *best_rdc = sum_rdc;
  pc_tree->partitioning = partition;
  return true;
}

// Initialize state variables of partition search used in
// av1_rd_pick_partition().
static void init_partition_search_state_params(
    MACROBLOCK *x, AV1_COMP *const cpi, PartitionSearchState *part_search_state,
    int mi_row, int mi_col, BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const AV1_COMMON *const cm = &cpi->common;
  PartitionBlkParams *blk_params = &part_search_state->part_blk_params;
  const CommonModeInfoParams *const mi_params = &cpi->common.mi_params;

  // Initialization of block size related parameters.
  blk_params->mi_step = mi_size_wide[bsize] / 2;
  blk_params->mi_row = mi_row;
  blk_params->mi_col = mi_col;
  blk_params->mi_row_edge = mi_row + blk_params->mi_step;
  blk_params->mi_col_edge = mi_col + blk_params->mi_step;
  blk_params->width = block_size_wide[bsize];
  blk_params->min_partition_size_1d =
      block_size_wide[x->sb_enc.min_partition_size];
  blk_params->subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
  blk_params->split_bsize2 = blk_params->subsize;
  blk_params->bsize_at_least_8x8 = (bsize >= BLOCK_8X8);
  blk_params->bsize = bsize;

  // Check if the partition corresponds to edge block.
  blk_params->has_rows = (blk_params->mi_row_edge < mi_params->mi_rows);
  blk_params->has_cols = (blk_params->mi_col_edge < mi_params->mi_cols);

  // Update intra partitioning related info.
  part_search_state->intra_part_info = &x->part_search_info;
  // Prepare for segmentation CNN-based partitioning for intra-frame.
  if (frame_is_intra_only(cm) && bsize == BLOCK_64X64) {
    part_search_state->intra_part_info->quad_tree_idx = 0;
    part_search_state->intra_part_info->cnn_output_valid = 0;
  }

  // Set partition plane context index.
  part_search_state->pl_ctx_idx =
      blk_params->bsize_at_least_8x8
          ? partition_plane_context(xd, mi_row, mi_col, bsize)
          : 0;

  // Partition cost buffer update
  ModeCosts *mode_costs = &x->mode_costs;
  part_search_state->partition_cost =
      mode_costs->partition_cost[part_search_state->pl_ctx_idx];

  // Initialize HORZ and VERT win flags as true for all split partitions.
  for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
    part_search_state->split_part_rect_win[i].rect_part_win[HORZ] = true;
    part_search_state->split_part_rect_win[i].rect_part_win[VERT] = true;
  }

  // Initialize the rd cost.
  av1_init_rd_stats(&part_search_state->this_rdc);

  // Initialize RD costs for partition types to 0.
  part_search_state->none_rd = 0;
  av1_zero(part_search_state->split_rd);
  av1_zero(part_search_state->rect_part_rd);

  // Initialize SPLIT partition to be not ready.
  av1_zero(part_search_state->is_split_ctx_is_ready);
  // Initialize HORZ and VERT partitions to be not ready.
  av1_zero(part_search_state->is_rect_ctx_is_ready);

  // Chroma subsampling.
  part_search_state->ss_x = x->e_mbd.plane[1].subsampling_x;
  part_search_state->ss_y = x->e_mbd.plane[1].subsampling_y;

  // Initialize partition search flags to defaults.
  part_search_state->terminate_partition_search = 0;
  part_search_state->do_square_split = blk_params->bsize_at_least_8x8;
  part_search_state->do_rectangular_split =
      cpi->oxcf.part_cfg.enable_rect_partitions;
  av1_zero(part_search_state->prune_rect_part);

  // Initialize allowed partition types for the partition block.
  part_search_state->partition_none_allowed =
      blk_params->has_rows && blk_params->has_cols;
  part_search_state->partition_rect_allowed[HORZ] =
      blk_params->has_cols && blk_params->bsize_at_least_8x8 &&
      cpi->oxcf.part_cfg.enable_rect_partitions &&
      get_plane_block_size(get_partition_subsize(bsize, PARTITION_HORZ),
                           part_search_state->ss_x,
                           part_search_state->ss_y) != BLOCK_INVALID;
  part_search_state->partition_rect_allowed[VERT] =
      blk_params->has_rows && blk_params->bsize_at_least_8x8 &&
      cpi->oxcf.part_cfg.enable_rect_partitions &&
      get_plane_block_size(get_partition_subsize(bsize, PARTITION_VERT),
                           part_search_state->ss_x,
                           part_search_state->ss_y) != BLOCK_INVALID;

  // Reset the flag indicating whether a partition leading to a rdcost lower
  // than the bound best_rdc has been found.
  part_search_state->found_best_partition = false;
}

// Override partition cost buffer for the edge blocks.
static void set_partition_cost_for_edge_blk(
    AV1_COMMON const *cm, PartitionSearchState *part_search_state) {
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  assert(blk_params.bsize_at_least_8x8 && part_search_state->pl_ctx_idx >= 0);
  const aom_cdf_prob *partition_cdf =
      cm->fc->partition_cdf[part_search_state->pl_ctx_idx];
  const int max_cost = av1_cost_symbol(0);
  for (PARTITION_TYPE i = 0; i < PARTITION_TYPES; ++i)
    part_search_state->tmp_partition_cost[i] = max_cost;
  if (blk_params.has_cols) {
    // At the bottom, the two possibilities are HORZ and SPLIT.
    aom_cdf_prob bot_cdf[2];
    partition_gather_vert_alike(bot_cdf, partition_cdf, blk_params.bsize);
    static const int bot_inv_map[2] = { PARTITION_HORZ, PARTITION_SPLIT };
    av1_cost_tokens_from_cdf(part_search_state->tmp_partition_cost, bot_cdf,
                             bot_inv_map);
  } else if (blk_params.has_rows) {
    // At the right, the two possibilities are VERT and SPLIT.
    aom_cdf_prob rhs_cdf[2];
    partition_gather_horz_alike(rhs_cdf, partition_cdf, blk_params.bsize);
    static const int rhs_inv_map[2] = { PARTITION_VERT, PARTITION_SPLIT };
    av1_cost_tokens_from_cdf(part_search_state->tmp_partition_cost, rhs_cdf,
                             rhs_inv_map);
  } else {
    // At the bottom right, we always split.
    part_search_state->tmp_partition_cost[PARTITION_SPLIT] = 0;
  }
  // Override the partition cost buffer.
  part_search_state->partition_cost = part_search_state->tmp_partition_cost;
}

// Reset the partition search state flags when
// must_find_valid_partition is equal to 1.
static AOM_INLINE void reset_part_limitations(
    AV1_COMP *const cpi, PartitionSearchState *part_search_state) {
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  const int is_rect_part_allowed =
      blk_params.bsize_at_least_8x8 &&
      cpi->oxcf.part_cfg.enable_rect_partitions &&
      (blk_params.width > blk_params.min_partition_size_1d);
  part_search_state->do_square_split =
      blk_params.bsize_at_least_8x8 &&
      (blk_params.width > blk_params.min_partition_size_1d);
  part_search_state->partition_none_allowed =
      blk_params.has_rows && blk_params.has_cols &&
      (blk_params.width >= blk_params.min_partition_size_1d);
  part_search_state->partition_rect_allowed[HORZ] =
      blk_params.has_cols && is_rect_part_allowed &&
      get_plane_block_size(
          get_partition_subsize(blk_params.bsize, PARTITION_HORZ),
          part_search_state->ss_x, part_search_state->ss_y) != BLOCK_INVALID;
  part_search_state->partition_rect_allowed[VERT] =
      blk_params.has_rows && is_rect_part_allowed &&
      get_plane_block_size(
          get_partition_subsize(blk_params.bsize, PARTITION_VERT),
          part_search_state->ss_x, part_search_state->ss_y) != BLOCK_INVALID;
  part_search_state->terminate_partition_search = 0;
}

// Rectangular partitions evaluation at sub-block level.
static void rd_pick_rect_partition(AV1_COMP *const cpi, TileDataEnc *tile_data,
                                   MACROBLOCK *x,
                                   PICK_MODE_CONTEXT *cur_partition_ctx,
                                   PartitionSearchState *part_search_state,
                                   RD_STATS *best_rdc, const int idx,
                                   int mi_row, int mi_col, BLOCK_SIZE bsize,
                                   PARTITION_TYPE partition_type) {
  // Obtain the remainder from the best rd cost
  // for further processing of partition.
  RD_STATS best_remain_rdcost;
  av1_rd_stats_subtraction(x->rdmult, best_rdc, &part_search_state->sum_rdc,
                           &best_remain_rdcost);

  // Obtain the best mode for the partition sub-block.
  pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &part_search_state->this_rdc,
                partition_type, bsize, cur_partition_ctx, best_remain_rdcost);
  av1_rd_cost_update(x->rdmult, &part_search_state->this_rdc);

  // Update the partition rd cost with the current sub-block rd.
  if (part_search_state->this_rdc.rate == INT_MAX) {
    part_search_state->sum_rdc.rdcost = INT64_MAX;
  } else {
    part_search_state->sum_rdc.rate += part_search_state->this_rdc.rate;
    part_search_state->sum_rdc.dist += part_search_state->this_rdc.dist;
    av1_rd_cost_update(x->rdmult, &part_search_state->sum_rdc);
  }
  const RECT_PART_TYPE rect_part =
      partition_type == PARTITION_HORZ ? HORZ : VERT;
  part_search_state->rect_part_rd[rect_part][idx] =
      part_search_state->this_rdc.rdcost;
}

typedef int (*active_edge_info)(const AV1_COMP *cpi, int mi_col, int mi_step);

// Checks if HORZ / VERT partition search is allowed.
static AOM_INLINE int is_rect_part_allowed(
    const AV1_COMP *cpi, PartitionSearchState *part_search_state,
    active_edge_info *active_edge, RECT_PART_TYPE rect_part, const int mi_pos) {
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  const int is_part_allowed =
      (!part_search_state->terminate_partition_search &&
       part_search_state->partition_rect_allowed[rect_part] &&
       !part_search_state->prune_rect_part[rect_part] &&
       (part_search_state->do_rectangular_split ||
        active_edge[rect_part](cpi, mi_pos, blk_params.mi_step)));
  return is_part_allowed;
}

static void rectangular_partition_search(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    TokenExtra **tp, MACROBLOCK *x, PC_TREE *pc_tree,
    RD_SEARCH_MACROBLOCK_CONTEXT *x_ctx,
    PartitionSearchState *part_search_state, RD_STATS *best_rdc,
    RD_RECT_PART_WIN_INFO *rect_part_win_info) {
  const AV1_COMMON *const cm = &cpi->common;
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  RD_STATS *sum_rdc = &part_search_state->sum_rdc;
  const int rect_partition_type[NUM_RECT_PARTS] = { PARTITION_HORZ,
                                                    PARTITION_VERT };

  // mi_pos_rect[NUM_RECT_PARTS][SUB_PARTITIONS_RECT][0]: mi_row postion of
  //                                           HORZ and VERT partition types.
  // mi_pos_rect[NUM_RECT_PARTS][SUB_PARTITIONS_RECT][1]: mi_col postion of
  //                                           HORZ and VERT partition types.
  const int mi_pos_rect[NUM_RECT_PARTS][SUB_PARTITIONS_RECT][2] = {
    { { blk_params.mi_row, blk_params.mi_col },
      { blk_params.mi_row_edge, blk_params.mi_col } },
    { { blk_params.mi_row, blk_params.mi_col },
      { blk_params.mi_row, blk_params.mi_col_edge } }
  };

  // Initialize active edge_type function pointer
  // for HOZR and VERT partition types.
  active_edge_info active_edge_type[NUM_RECT_PARTS] = { av1_active_h_edge,
                                                        av1_active_v_edge };

  // Indicates edge blocks for HORZ and VERT partition types.
  const int is_not_edge_block[NUM_RECT_PARTS] = { blk_params.has_rows,
                                                  blk_params.has_cols };

  // Initialize pc tree context for HORZ and VERT partition types.
  PICK_MODE_CONTEXT **cur_ctx[NUM_RECT_PARTS][SUB_PARTITIONS_RECT] = {
    { &pc_tree->horizontal[0], &pc_tree->horizontal[1] },
    { &pc_tree->vertical[0], &pc_tree->vertical[1] }
  };

  // Loop over rectangular partition types.
  for (RECT_PART_TYPE i = HORZ; i < NUM_RECT_PARTS; i++) {
    assert(IMPLIES(!cpi->oxcf.part_cfg.enable_rect_partitions,
                   !part_search_state->partition_rect_allowed[i]));

    // Check if the HORZ / VERT partition search is to be performed.
    if (!is_rect_part_allowed(cpi, part_search_state, active_edge_type, i,
                              mi_pos_rect[i][0][i]))
      continue;

    // Sub-partition idx.
    int sub_part_idx = 0;
    PARTITION_TYPE partition_type = rect_partition_type[i];
    blk_params.subsize =
        get_partition_subsize(blk_params.bsize, partition_type);
    assert(blk_params.subsize <= BLOCK_LARGEST);
    av1_init_rd_stats(sum_rdc);
    for (int j = 0; j < SUB_PARTITIONS_RECT; j++) {
      if (cur_ctx[i][j][0] == NULL) {
        cur_ctx[i][j][0] =
            av1_alloc_pmc(cm, blk_params.subsize, &td->shared_coeff_buf);
      }
    }
    sum_rdc->rate = part_search_state->partition_cost[partition_type];
    sum_rdc->rdcost = RDCOST(x->rdmult, sum_rdc->rate, 0);
#if CONFIG_COLLECT_PARTITION_STATS
    if (best_rdc.rdcost - sum_rdc->rdcost >= 0) {
      partition_attempts[partition_type] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif

    // First sub-partition evaluation in HORZ / VERT partition type.
    rd_pick_rect_partition(
        cpi, tile_data, x, cur_ctx[i][sub_part_idx][0], part_search_state,
        best_rdc, 0, mi_pos_rect[i][sub_part_idx][0],
        mi_pos_rect[i][sub_part_idx][1], blk_params.subsize, partition_type);

    // Start of second sub-partition evaluation.
    // Evaluate second sub-partition if the first sub-partition cost
    // is less than the best cost and if it is not an edge block.
    if (sum_rdc->rdcost < best_rdc->rdcost && is_not_edge_block[i]) {
      const MB_MODE_INFO *const mbmi = &cur_ctx[i][sub_part_idx][0]->mic;
      const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
      // Neither palette mode nor cfl predicted.
      if (pmi->palette_size[PLANE_TYPE_Y] == 0 &&
          pmi->palette_size[PLANE_TYPE_UV] == 0) {
        if (mbmi->uv_mode != UV_CFL_PRED)
          part_search_state->is_rect_ctx_is_ready[i] = 1;
      }
      av1_update_state(cpi, td, cur_ctx[i][sub_part_idx][0], blk_params.mi_row,
                       blk_params.mi_col, blk_params.subsize, DRY_RUN_NORMAL);
      encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL,
                        blk_params.subsize, NULL);

      // Second sub-partition evaluation in HORZ / VERT partition type.
      sub_part_idx = 1;
      rd_pick_rect_partition(
          cpi, tile_data, x, cur_ctx[i][sub_part_idx][0], part_search_state,
          best_rdc, 1, mi_pos_rect[i][sub_part_idx][0],
          mi_pos_rect[i][sub_part_idx][1], blk_params.subsize, partition_type);
    }
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[partition_type] += time;
      partition_timer_on = 0;
    }
#endif
    // Update HORZ / VERT best partition.
    if (sum_rdc->rdcost < best_rdc->rdcost) {
      sum_rdc->rdcost = RDCOST(x->rdmult, sum_rdc->rate, sum_rdc->dist);
      if (sum_rdc->rdcost < best_rdc->rdcost) {
        *best_rdc = *sum_rdc;
        part_search_state->found_best_partition = true;
        pc_tree->partitioning = partition_type;
      }
    } else {
      // Update HORZ / VERT win flag.
      if (rect_part_win_info != NULL)
        rect_part_win_info->rect_part_win[i] = false;
    }
    av1_restore_context(x, x_ctx, blk_params.mi_row, blk_params.mi_col,
                        blk_params.bsize, av1_num_planes(cm));
  }
}

// AB partition type evaluation.
static void rd_pick_ab_part(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    TokenExtra **tp, MACROBLOCK *x, RD_SEARCH_MACROBLOCK_CONTEXT *x_ctx,
    PC_TREE *pc_tree, PICK_MODE_CONTEXT *dst_ctxs[SUB_PARTITIONS_AB],
    PartitionSearchState *part_search_state, RD_STATS *best_rdc,
    const BLOCK_SIZE ab_subsize[SUB_PARTITIONS_AB],
    const int ab_mi_pos[SUB_PARTITIONS_AB][2], const PARTITION_TYPE part_type,
    const MB_MODE_INFO **mode_cache) {
  const AV1_COMMON *const cm = &cpi->common;
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  const int mi_row = blk_params.mi_row;
  const int mi_col = blk_params.mi_col;
  const int bsize = blk_params.bsize;

#if CONFIG_COLLECT_PARTITION_STATS
  {
    RD_STATS tmp_sum_rdc;
    av1_init_rd_stats(&tmp_sum_rdc);
    tmp_sum_rdc.rate =
        x->partition_cost[part_search_state->pl_ctx_idx][part_type];
    tmp_sum_rdc.rdcost = RDCOST(x->rdmult, tmp_sum_rdc.rate, 0);
    if (best_rdc->rdcost - tmp_sum_rdc.rdcost >= 0) {
      partition_attempts[part_type] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
  }
#endif

  // Test this partition and update the best partition.
  part_search_state->found_best_partition |= rd_test_partition3(
      cpi, td, tile_data, tp, pc_tree, best_rdc, dst_ctxs, mi_row, mi_col,
      bsize, part_type, ab_subsize, ab_mi_pos, mode_cache);

#if CONFIG_COLLECT_PARTITION_STATS
  if (partition_timer_on) {
    aom_usec_timer_mark(&partition_timer);
    int64_t time = aom_usec_timer_elapsed(&partition_timer);
    partition_times[part_type] += time;
    partition_timer_on = 0;
  }
#endif
  av1_restore_context(x, x_ctx, mi_row, mi_col, bsize, av1_num_planes(cm));
}

// Check if AB partitions search is allowed.
static AOM_INLINE int is_ab_part_allowed(
    PartitionSearchState *part_search_state,
    const int ab_partitions_allowed[NUM_AB_PARTS], const int ab_part_type) {
  const int is_horz_ab = (ab_part_type >> 1);
  const int is_part_allowed =
      (!part_search_state->terminate_partition_search &&
       part_search_state->partition_rect_allowed[is_horz_ab] &&
       ab_partitions_allowed[ab_part_type]);
  return is_part_allowed;
}

// Set mode search context.
static AOM_INLINE void set_mode_search_ctx(
    PC_TREE *pc_tree, const int is_ctx_ready[NUM_AB_PARTS][2],
    PICK_MODE_CONTEXT **mode_srch_ctx[NUM_AB_PARTS][2]) {
  mode_srch_ctx[HORZ_B][0] = &pc_tree->horizontal[0];
  mode_srch_ctx[VERT_B][0] = &pc_tree->vertical[0];

  if (is_ctx_ready[HORZ_A][0])
    mode_srch_ctx[HORZ_A][0] = &pc_tree->split[0]->none;

  if (is_ctx_ready[VERT_A][0])
    mode_srch_ctx[VERT_A][0] = &pc_tree->split[0]->none;

  if (is_ctx_ready[HORZ_A][1])
    mode_srch_ctx[HORZ_A][1] = &pc_tree->split[1]->none;
}

static AOM_INLINE void copy_partition_mode_from_mode_context(
    const MB_MODE_INFO **dst_mode, const PICK_MODE_CONTEXT *ctx) {
  if (ctx && ctx->rd_stats.rate < INT_MAX) {
    *dst_mode = &ctx->mic;
  } else {
    *dst_mode = NULL;
  }
}

static AOM_INLINE void copy_partition_mode_from_pc_tree(
    const MB_MODE_INFO **dst_mode, const PC_TREE *pc_tree) {
  if (pc_tree) {
    copy_partition_mode_from_mode_context(dst_mode, pc_tree->none);
  } else {
    *dst_mode = NULL;
  }
}

static AOM_INLINE void set_mode_cache_for_partition_ab(
    const MB_MODE_INFO **mode_cache, const PC_TREE *pc_tree,
    AB_PART_TYPE ab_part_type) {
  switch (ab_part_type) {
    case HORZ_A:
      copy_partition_mode_from_pc_tree(&mode_cache[0], pc_tree->split[0]);
      copy_partition_mode_from_pc_tree(&mode_cache[1], pc_tree->split[1]);
      copy_partition_mode_from_mode_context(&mode_cache[2],
                                            pc_tree->horizontal[1]);
      break;
    case HORZ_B:
      copy_partition_mode_from_mode_context(&mode_cache[0],
                                            pc_tree->horizontal[0]);
      copy_partition_mode_from_pc_tree(&mode_cache[1], pc_tree->split[2]);
      copy_partition_mode_from_pc_tree(&mode_cache[2], pc_tree->split[3]);
      break;
    case VERT_A:
      copy_partition_mode_from_pc_tree(&mode_cache[0], pc_tree->split[0]);
      copy_partition_mode_from_pc_tree(&mode_cache[1], pc_tree->split[2]);
      copy_partition_mode_from_mode_context(&mode_cache[2],
                                            pc_tree->vertical[1]);
      break;
    case VERT_B:
      copy_partition_mode_from_mode_context(&mode_cache[0],
                                            pc_tree->vertical[0]);
      copy_partition_mode_from_pc_tree(&mode_cache[1], pc_tree->split[1]);
      copy_partition_mode_from_pc_tree(&mode_cache[2], pc_tree->split[3]);
      break;
    default: assert(0 && "Invalid ab partition type!\n");
  }
}

// AB Partitions type search.
static void ab_partitions_search(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    TokenExtra **tp, MACROBLOCK *x, RD_SEARCH_MACROBLOCK_CONTEXT *x_ctx,
    PC_TREE *pc_tree, PartitionSearchState *part_search_state,
    RD_STATS *best_rdc, RD_RECT_PART_WIN_INFO *rect_part_win_info,
    int pb_source_variance, int ext_partition_allowed) {
  const AV1_COMMON *const cm = &cpi->common;
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  const int mi_row = blk_params.mi_row;
  const int mi_col = blk_params.mi_col;
  const int bsize = blk_params.bsize;

  int ab_partitions_allowed[NUM_AB_PARTS] = { 1, 1, 1, 1 };
  // Prune AB partitions
  av1_prune_ab_partitions(
      cpi, x, pc_tree, bsize, pb_source_variance, best_rdc->rdcost,
      part_search_state->rect_part_rd, part_search_state->split_rd,
      rect_part_win_info, ext_partition_allowed,
      part_search_state->partition_rect_allowed[HORZ],
      part_search_state->partition_rect_allowed[VERT],
      &ab_partitions_allowed[HORZ_A], &ab_partitions_allowed[HORZ_B],
      &ab_partitions_allowed[VERT_A], &ab_partitions_allowed[VERT_B]);

  // Flags to indicate whether the mode search is done.
  const int is_ctx_ready[NUM_AB_PARTS][2] = {
    { part_search_state->is_split_ctx_is_ready[0],
      part_search_state->is_split_ctx_is_ready[1] },
    { part_search_state->is_rect_ctx_is_ready[HORZ], 0 },
    { part_search_state->is_split_ctx_is_ready[0], 0 },
    { part_search_state->is_rect_ctx_is_ready[VERT], 0 }
  };

  // Current partition context.
  PICK_MODE_CONTEXT **cur_part_ctxs[NUM_AB_PARTS] = { pc_tree->horizontala,
                                                      pc_tree->horizontalb,
                                                      pc_tree->verticala,
                                                      pc_tree->verticalb };

  // Context of already evaluted partition types.
  PICK_MODE_CONTEXT **mode_srch_ctx[NUM_AB_PARTS][2];
  // Set context of already evaluted partition types.
  set_mode_search_ctx(pc_tree, is_ctx_ready, mode_srch_ctx);

  // Array of sub-partition size of AB partition types.
  const BLOCK_SIZE ab_subsize[NUM_AB_PARTS][SUB_PARTITIONS_AB] = {
    { blk_params.split_bsize2, blk_params.split_bsize2,
      get_partition_subsize(bsize, PARTITION_HORZ_A) },
    { get_partition_subsize(bsize, PARTITION_HORZ_B), blk_params.split_bsize2,
      blk_params.split_bsize2 },
    { blk_params.split_bsize2, blk_params.split_bsize2,
      get_partition_subsize(bsize, PARTITION_VERT_A) },
    { get_partition_subsize(bsize, PARTITION_VERT_B), blk_params.split_bsize2,
      blk_params.split_bsize2 }
  };

  // Array of mi_row, mi_col positions corresponds to each sub-partition in AB
  // partition types.
  const int ab_mi_pos[NUM_AB_PARTS][SUB_PARTITIONS_AB][2] = {
    { { mi_row, mi_col },
      { mi_row, blk_params.mi_col_edge },
      { blk_params.mi_row_edge, mi_col } },
    { { mi_row, mi_col },
      { blk_params.mi_row_edge, mi_col },
      { blk_params.mi_row_edge, blk_params.mi_col_edge } },
    { { mi_row, mi_col },
      { blk_params.mi_row_edge, mi_col },
      { mi_row, blk_params.mi_col_edge } },
    { { mi_row, mi_col },
      { mi_row, blk_params.mi_col_edge },
      { blk_params.mi_row_edge, blk_params.mi_col_edge } }
  };

  // Loop over AB partition types.
  for (AB_PART_TYPE ab_part_type = 0; ab_part_type < NUM_AB_PARTS;
       ab_part_type++) {
    const PARTITION_TYPE part_type = ab_part_type + PARTITION_HORZ_A;

    // Check if the AB partition search is to be performed.
    if (!is_ab_part_allowed(part_search_state, ab_partitions_allowed,
                            ab_part_type))
      continue;

    blk_params.subsize = get_partition_subsize(bsize, part_type);
    for (int i = 0; i < SUB_PARTITIONS_AB; i++) {
      // Set AB partition context.
      cur_part_ctxs[ab_part_type][i] =
          av1_alloc_pmc(cm, ab_subsize[ab_part_type][i], &td->shared_coeff_buf);
      // Set mode as not ready.
      cur_part_ctxs[ab_part_type][i]->rd_mode_is_ready = 0;
    }

    // We can copy directly the mode search results if we have already searched
    // the current block and the contexts match.
    if (is_ctx_ready[ab_part_type][0]) {
      av1_copy_tree_context(cur_part_ctxs[ab_part_type][0],
                            mode_srch_ctx[ab_part_type][0][0]);
      cur_part_ctxs[ab_part_type][0]->mic.partition = part_type;
      cur_part_ctxs[ab_part_type][0]->rd_mode_is_ready = 1;
      if (is_ctx_ready[ab_part_type][1]) {
        av1_copy_tree_context(cur_part_ctxs[ab_part_type][1],
                              mode_srch_ctx[ab_part_type][1][0]);
        cur_part_ctxs[ab_part_type][1]->mic.partition = part_type;
        cur_part_ctxs[ab_part_type][1]->rd_mode_is_ready = 1;
      }
    }

    // Even if the contexts don't match, we can still speed up by reusing the
    // previous prediction mode.
    const MB_MODE_INFO *mode_cache[3] = { NULL, NULL, NULL };
    if (cpi->sf.inter_sf.reuse_best_prediction_for_part_ab) {
      set_mode_cache_for_partition_ab(mode_cache, pc_tree, ab_part_type);
    }

    // Evaluation of AB partition type.
    rd_pick_ab_part(cpi, td, tile_data, tp, x, x_ctx, pc_tree,
                    cur_part_ctxs[ab_part_type], part_search_state, best_rdc,
                    ab_subsize[ab_part_type], ab_mi_pos[ab_part_type],
                    part_type, mode_cache);
  }
}

// Set mi positions for HORZ4 / VERT4 sub-block partitions.
static void set_mi_pos_partition4(const int inc_step[NUM_PART4_TYPES],
                                  int mi_pos[SUB_PARTITIONS_PART4][2],
                                  const int mi_row, const int mi_col) {
  for (PART4_TYPES i = 0; i < SUB_PARTITIONS_PART4; i++) {
    mi_pos[i][0] = mi_row + i * inc_step[HORZ4];
    mi_pos[i][1] = mi_col + i * inc_step[VERT4];
  }
}

// Set context and RD cost for HORZ4 / VERT4 partition types.
static void set_4_part_ctx_and_rdcost(
    MACROBLOCK *x, const AV1_COMMON *const cm, ThreadData *td,
    PICK_MODE_CONTEXT *cur_part_ctx[SUB_PARTITIONS_PART4],
    PartitionSearchState *part_search_state, PARTITION_TYPE partition_type,
    BLOCK_SIZE bsize) {
  // Initialize sum_rdc RD cost structure.
  av1_init_rd_stats(&part_search_state->sum_rdc);
  const int subsize = get_partition_subsize(bsize, partition_type);
  part_search_state->sum_rdc.rate =
      part_search_state->partition_cost[partition_type];
  part_search_state->sum_rdc.rdcost =
      RDCOST(x->rdmult, part_search_state->sum_rdc.rate, 0);
  for (PART4_TYPES i = 0; i < SUB_PARTITIONS_PART4; ++i)
    cur_part_ctx[i] = av1_alloc_pmc(cm, subsize, &td->shared_coeff_buf);
}

// Partition search of HORZ4 / VERT4 partition types.
static void rd_pick_4partition(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    TokenExtra **tp, MACROBLOCK *x, RD_SEARCH_MACROBLOCK_CONTEXT *x_ctx,
    PC_TREE *pc_tree, PICK_MODE_CONTEXT *cur_part_ctx[SUB_PARTITIONS_PART4],
    PartitionSearchState *part_search_state, RD_STATS *best_rdc,
    const int inc_step[NUM_PART4_TYPES], PARTITION_TYPE partition_type) {
  const AV1_COMMON *const cm = &cpi->common;
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  // mi positions needed for HORZ4 and VERT4 partition types.
  int mi_pos_check[NUM_PART4_TYPES] = { cm->mi_params.mi_rows,
                                        cm->mi_params.mi_cols };
  const PART4_TYPES part4_idx = (partition_type != PARTITION_HORZ_4);
  int mi_pos[SUB_PARTITIONS_PART4][2];

  blk_params.subsize = get_partition_subsize(blk_params.bsize, partition_type);
  // Set partition context and RD cost.
  set_4_part_ctx_and_rdcost(x, cm, td, cur_part_ctx, part_search_state,
                            partition_type, blk_params.bsize);
  // Set mi positions for sub-block sizes.
  set_mi_pos_partition4(inc_step, mi_pos, blk_params.mi_row, blk_params.mi_col);
#if CONFIG_COLLECT_PARTITION_STATS
  if (best_rdc.rdcost - part_search_state->sum_rdc.rdcost >= 0) {
    partition_attempts[partition_type] += 1;
    aom_usec_timer_start(&partition_timer);
    partition_timer_on = 1;
  }
#endif
  // Loop over sub-block partitions.
  for (PART4_TYPES i = 0; i < SUB_PARTITIONS_PART4; ++i) {
    if (i > 0 && mi_pos[i][part4_idx] >= mi_pos_check[part4_idx]) break;

    // Sub-block evaluation of Horz4 / Vert4 partition type.
    cur_part_ctx[i]->rd_mode_is_ready = 0;
    if (!rd_try_subblock(
            cpi, td, tile_data, tp, (i == SUB_PARTITIONS_PART4 - 1),
            mi_pos[i][0], mi_pos[i][1], blk_params.subsize, *best_rdc,
            &part_search_state->sum_rdc, partition_type, cur_part_ctx[i])) {
      av1_invalid_rd_stats(&part_search_state->sum_rdc);
      break;
    }
  }

  // Calculate the total cost and update the best partition.
  av1_rd_cost_update(x->rdmult, &part_search_state->sum_rdc);
  if (part_search_state->sum_rdc.rdcost < best_rdc->rdcost) {
    *best_rdc = part_search_state->sum_rdc;
    part_search_state->found_best_partition = true;
    pc_tree->partitioning = partition_type;
  }
#if CONFIG_COLLECT_PARTITION_STATS
  if (partition_timer_on) {
    aom_usec_timer_mark(&partition_timer);
    int64_t time = aom_usec_timer_elapsed(&partition_timer);
    partition_times[partition_type] += time;
    partition_timer_on = 0;
  }
#endif
  av1_restore_context(x, x_ctx, blk_params.mi_row, blk_params.mi_col,
                      blk_params.bsize, av1_num_planes(cm));
}

// Prune 4-way partitions based on the number of horz/vert wins
// in the current block and sub-blocks in PARTITION_SPLIT.
static void prune_4_partition_using_split_info(
    AV1_COMP *const cpi, MACROBLOCK *x, PartitionSearchState *part_search_state,
    int part4_search_allowed[NUM_PART4_TYPES]) {
  PART4_TYPES cur_part[NUM_PART4_TYPES] = { HORZ4, VERT4 };
  // Count of child blocks in which HORZ or VERT partition has won
  int num_child_rect_win[NUM_RECT_PARTS] = { 0, 0 };
  // Prune HORZ4/VERT4 partitions based on number of HORZ/VERT winners of
  // split partiitons.
  // Conservative pruning for high quantizers.
  const int num_win_thresh = AOMMIN(3 * (MAXQ - x->qindex) / MAXQ + 1, 3);

  for (RECT_PART_TYPE i = HORZ; i < NUM_RECT_PARTS; i++) {
    if (!(cpi->sf.part_sf.prune_4_partition_using_split_info &&
          part4_search_allowed[cur_part[i]]))
      continue;
    // Loop over split partitions.
    // Get rectangular partitions winner info of split partitions.
    for (int idx = 0; idx < SUB_PARTITIONS_SPLIT; idx++)
      num_child_rect_win[i] +=
          (part_search_state->split_part_rect_win[idx].rect_part_win[i]) ? 1
                                                                         : 0;
    if (num_child_rect_win[i] < num_win_thresh) {
      part4_search_allowed[cur_part[i]] = 0;
    }
  }
}

// Prune 4-way partition search.
static void prune_4_way_partition_search(
    AV1_COMP *const cpi, MACROBLOCK *x, PC_TREE *pc_tree,
    PartitionSearchState *part_search_state, RD_STATS *best_rdc,
    int pb_source_variance, int ext_partition_allowed,
    int part4_search_allowed[NUM_PART4_TYPES]) {
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  const int mi_row = blk_params.mi_row;
  const int mi_col = blk_params.mi_col;
  const int bsize = blk_params.bsize;
  PARTITION_TYPE cur_part[NUM_PART4_TYPES] = { PARTITION_HORZ_4,
                                               PARTITION_VERT_4 };
  const PartitionCfg *const part_cfg = &cpi->oxcf.part_cfg;
  // partition4_allowed is 1 if we can use a PARTITION_HORZ_4 or
  // PARTITION_VERT_4 for this block. This is almost the same as
  // ext_partition_allowed, except that we don't allow 128x32 or 32x128
  // blocks, so we require that bsize is not BLOCK_128X128.
  const int partition4_allowed = part_cfg->enable_1to4_partitions &&
                                 ext_partition_allowed &&
                                 bsize != BLOCK_128X128;

  for (PART4_TYPES i = HORZ4; i < NUM_PART4_TYPES; i++) {
    part4_search_allowed[i] =
        partition4_allowed && part_search_state->partition_rect_allowed[i] &&
        get_plane_block_size(get_partition_subsize(bsize, cur_part[i]),
                             part_search_state->ss_x,
                             part_search_state->ss_y) != BLOCK_INVALID;
  }
  // Pruning: pruning out 4-way partitions based on the current best partition.
  if (cpi->sf.part_sf.prune_ext_partition_types_search_level == 2) {
    part4_search_allowed[HORZ4] &= (pc_tree->partitioning == PARTITION_HORZ ||
                                    pc_tree->partitioning == PARTITION_HORZ_A ||
                                    pc_tree->partitioning == PARTITION_HORZ_B ||
                                    pc_tree->partitioning == PARTITION_SPLIT ||
                                    pc_tree->partitioning == PARTITION_NONE);
    part4_search_allowed[VERT4] &= (pc_tree->partitioning == PARTITION_VERT ||
                                    pc_tree->partitioning == PARTITION_VERT_A ||
                                    pc_tree->partitioning == PARTITION_VERT_B ||
                                    pc_tree->partitioning == PARTITION_SPLIT ||
                                    pc_tree->partitioning == PARTITION_NONE);
  }

  // Pruning: pruning out some 4-way partitions using a DNN taking rd costs of
  // sub-blocks from basic partition types.
  if (cpi->sf.part_sf.ml_prune_4_partition && partition4_allowed &&
      part_search_state->partition_rect_allowed[HORZ] &&
      part_search_state->partition_rect_allowed[VERT]) {
    av1_ml_prune_4_partition(
        cpi, x, bsize, pc_tree->partitioning, best_rdc->rdcost,
        part_search_state->rect_part_rd, part_search_state->split_rd,
        &part4_search_allowed[HORZ4], &part4_search_allowed[VERT4],
        pb_source_variance, mi_row, mi_col);
  }

  // Pruning: pruning out 4-way partitions based on the number of horz/vert wins
  // in the current block and sub-blocks in PARTITION_SPLIT.
  prune_4_partition_using_split_info(cpi, x, part_search_state,
                                     part4_search_allowed);
}

// Set PARTITION_NONE allowed flag.
static AOM_INLINE void set_part_none_allowed_flag(
    AV1_COMP *const cpi, PartitionSearchState *part_search_state) {
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  if ((blk_params.width <= blk_params.min_partition_size_1d) &&
      blk_params.has_rows && blk_params.has_cols)
    part_search_state->partition_none_allowed = 1;
  assert(part_search_state->terminate_partition_search == 0);

  // Set PARTITION_NONE for screen content.
  if (cpi->use_screen_content_tools)
    part_search_state->partition_none_allowed =
        blk_params.has_rows && blk_params.has_cols;
}

// Set params needed for PARTITION_NONE search.
static void set_none_partition_params(const AV1_COMMON *const cm,
                                      ThreadData *td, MACROBLOCK *x,
                                      PC_TREE *pc_tree,
                                      PartitionSearchState *part_search_state,
                                      RD_STATS *best_remain_rdcost,
                                      RD_STATS *best_rdc, int *pt_cost) {
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  RD_STATS partition_rdcost;
  // Set PARTITION_NONE context.
  if (pc_tree->none == NULL)
    pc_tree->none = av1_alloc_pmc(cm, blk_params.bsize, &td->shared_coeff_buf);

  // Set PARTITION_NONE type cost.
  if (part_search_state->partition_none_allowed) {
    if (blk_params.bsize_at_least_8x8) {
      *pt_cost = part_search_state->partition_cost[PARTITION_NONE] < INT_MAX
                     ? part_search_state->partition_cost[PARTITION_NONE]
                     : 0;
    }

    // Initialize the RD stats structure.
    av1_init_rd_stats(&partition_rdcost);
    partition_rdcost.rate = *pt_cost;
    av1_rd_cost_update(x->rdmult, &partition_rdcost);
    av1_rd_stats_subtraction(x->rdmult, best_rdc, &partition_rdcost,
                             best_remain_rdcost);
  }
}

// Skip other partitions based on PARTITION_NONE rd cost.
static void prune_partitions_after_none(AV1_COMP *const cpi, MACROBLOCK *x,
                                        SIMPLE_MOTION_DATA_TREE *sms_tree,
                                        PICK_MODE_CONTEXT *ctx_none,
                                        PartitionSearchState *part_search_state,
                                        RD_STATS *best_rdc,
                                        unsigned int *pb_source_variance) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  RD_STATS *this_rdc = &part_search_state->this_rdc;
  const BLOCK_SIZE bsize = blk_params.bsize;
  assert(bsize < BLOCK_SIZES_ALL);

  if (!frame_is_intra_only(cm) &&
      (part_search_state->do_square_split ||
       part_search_state->do_rectangular_split) &&
      !x->e_mbd.lossless[xd->mi[0]->segment_id] && ctx_none->skippable) {
    const int use_ml_based_breakout =
        bsize <= cpi->sf.part_sf.use_square_partition_only_threshold &&
        bsize > BLOCK_4X4 && cpi->sf.part_sf.ml_predict_breakout_level >= 1;
    if (use_ml_based_breakout) {
      if (av1_ml_predict_breakout(cpi, bsize, x, this_rdc, *pb_source_variance,
                                  xd->bd)) {
        part_search_state->do_square_split = 0;
        part_search_state->do_rectangular_split = 0;
      }
    }

    // Adjust dist breakout threshold according to the partition size.
    const int64_t dist_breakout_thr =
        cpi->sf.part_sf.partition_search_breakout_dist_thr >>
        ((2 * (MAX_SB_SIZE_LOG2 - 2)) -
         (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]));
    const int rate_breakout_thr =
        cpi->sf.part_sf.partition_search_breakout_rate_thr *
        num_pels_log2_lookup[bsize];
    // If all y, u, v transform blocks in this partition are skippable,
    // and the dist & rate are within the thresholds, the partition
    // search is terminated for current branch of the partition search
    // tree. The dist & rate thresholds are set to 0 at speed 0 to
    // disable the early termination at that speed.
    if (best_rdc->dist < dist_breakout_thr &&
        best_rdc->rate < rate_breakout_thr) {
      part_search_state->do_square_split = 0;
      part_search_state->do_rectangular_split = 0;
    }
  }

  // Early termination: using simple_motion_search features and the
  // rate, distortion, and rdcost of PARTITION_NONE, a DNN will make a
  // decision on early terminating at PARTITION_NONE.
  if (cpi->sf.part_sf.simple_motion_search_early_term_none && cm->show_frame &&
      !frame_is_intra_only(cm) && bsize >= BLOCK_16X16 &&
      blk_params.mi_row_edge < mi_params->mi_rows &&
      blk_params.mi_col_edge < mi_params->mi_cols &&
      this_rdc->rdcost < INT64_MAX && this_rdc->rdcost >= 0 &&
      this_rdc->rate < INT_MAX && this_rdc->rate >= 0 &&
      (part_search_state->do_square_split ||
       part_search_state->do_rectangular_split)) {
    av1_simple_motion_search_early_term_none(
        cpi, x, sms_tree, blk_params.mi_row, blk_params.mi_col, bsize, this_rdc,
        &part_search_state->terminate_partition_search);
  }
}

// Decide early termination and rectangular partition pruning
// based on PARTITION_NONE and PARTITION_SPLIT costs.
static void prune_partitions_after_split(
    AV1_COMP *const cpi, MACROBLOCK *x, SIMPLE_MOTION_DATA_TREE *sms_tree,
    PartitionSearchState *part_search_state, RD_STATS *best_rdc,
    int64_t part_none_rd, int64_t part_split_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  const int mi_row = blk_params.mi_row;
  const int mi_col = blk_params.mi_col;
  const BLOCK_SIZE bsize = blk_params.bsize;
  assert(bsize < BLOCK_SIZES_ALL);

  // Early termination: using the rd costs of PARTITION_NONE and subblocks
  // from PARTITION_SPLIT to determine an early breakout.
  if (cpi->sf.part_sf.ml_early_term_after_part_split_level &&
      !frame_is_intra_only(cm) &&
      !part_search_state->terminate_partition_search &&
      part_search_state->do_rectangular_split &&
      (part_search_state->partition_rect_allowed[HORZ] ||
       part_search_state->partition_rect_allowed[VERT])) {
    av1_ml_early_term_after_split(
        cpi, x, sms_tree, bsize, best_rdc->rdcost, part_none_rd, part_split_rd,
        part_search_state->split_rd, mi_row, mi_col,
        &part_search_state->terminate_partition_search);
  }

  // Use the rd costs of PARTITION_NONE and subblocks from PARTITION_SPLIT
  // to prune out rectangular partitions in some directions.
  if (!cpi->sf.part_sf.ml_early_term_after_part_split_level &&
      cpi->sf.part_sf.ml_prune_rect_partition && !frame_is_intra_only(cm) &&
      (part_search_state->partition_rect_allowed[HORZ] ||
       part_search_state->partition_rect_allowed[VERT]) &&
      !(part_search_state->prune_rect_part[HORZ] ||
        part_search_state->prune_rect_part[VERT]) &&
      !part_search_state->terminate_partition_search) {
    av1_setup_src_planes(x, cpi->source, mi_row, mi_col, av1_num_planes(cm),
                         bsize);
    av1_ml_prune_rect_partition(
        cpi, x, bsize, best_rdc->rdcost, part_search_state->none_rd,
        part_search_state->split_rd, &part_search_state->prune_rect_part[HORZ],
        &part_search_state->prune_rect_part[VERT]);
  }
}

// PARTITION_NONE search.
static void none_partition_search(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data, MACROBLOCK *x,
    PC_TREE *pc_tree, SIMPLE_MOTION_DATA_TREE *sms_tree,
    RD_SEARCH_MACROBLOCK_CONTEXT *x_ctx,
    PartitionSearchState *part_search_state, RD_STATS *best_rdc,
    unsigned int *pb_source_variance, int64_t *none_rd, int64_t *part_none_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  RD_STATS *this_rdc = &part_search_state->this_rdc;
  const int mi_row = blk_params.mi_row;
  const int mi_col = blk_params.mi_col;
  const BLOCK_SIZE bsize = blk_params.bsize;
  assert(bsize < BLOCK_SIZES_ALL);

  // Set PARTITION_NONE allowed flag.
  set_part_none_allowed_flag(cpi, part_search_state);
  if (!part_search_state->partition_none_allowed) return;

  int pt_cost = 0;
  RD_STATS best_remain_rdcost;

  // Set PARTITION_NONE context and cost.
  set_none_partition_params(cm, td, x, pc_tree, part_search_state,
                            &best_remain_rdcost, best_rdc, &pt_cost);

#if CONFIG_COLLECT_PARTITION_STATS
  // Timer start for partition None.
  if (best_remain_rdcost >= 0) {
    partition_attempts[PARTITION_NONE] += 1;
    aom_usec_timer_start(&partition_timer);
    partition_timer_on = 1;
  }
#endif
  // PARTITION_NONE evaluation and cost update.
  pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, this_rdc, PARTITION_NONE,
                bsize, pc_tree->none, best_remain_rdcost);

  av1_rd_cost_update(x->rdmult, this_rdc);

#if CONFIG_COLLECT_PARTITION_STATS
  // Timer end for partition None.
  if (partition_timer_on) {
    aom_usec_timer_mark(&partition_timer);
    int64_t time = aom_usec_timer_elapsed(&partition_timer);
    partition_times[PARTITION_NONE] += time;
    partition_timer_on = 0;
  }
#endif
  *pb_source_variance = x->source_variance;
  if (none_rd) *none_rd = this_rdc->rdcost;
  part_search_state->none_rd = this_rdc->rdcost;
  if (this_rdc->rate != INT_MAX) {
    // Record picked ref frame to prune ref frames for other partition types.
    if (cpi->sf.inter_sf.prune_ref_frame_for_rect_partitions) {
      const int ref_type = av1_ref_frame_type(pc_tree->none->mic.ref_frame);
      av1_update_picked_ref_frames_mask(
          x, ref_type, bsize, cm->seq_params.mib_size, mi_row, mi_col);
    }

    // Calculate the total cost and update the best partition.
    if (blk_params.bsize_at_least_8x8) {
      this_rdc->rate += pt_cost;
      this_rdc->rdcost = RDCOST(x->rdmult, this_rdc->rate, this_rdc->dist);
    }
    *part_none_rd = this_rdc->rdcost;
    if (this_rdc->rdcost < best_rdc->rdcost) {
      *best_rdc = *this_rdc;
      part_search_state->found_best_partition = true;
      if (blk_params.bsize_at_least_8x8) {
        pc_tree->partitioning = PARTITION_NONE;
      }

      // Disable split and rectangular partition search
      // based on PARTITION_NONE cost.
      prune_partitions_after_none(cpi, x, sms_tree, pc_tree->none,
                                  part_search_state, best_rdc,
                                  pb_source_variance);
    }
  }
  av1_restore_context(x, x_ctx, mi_row, mi_col, bsize, av1_num_planes(cm));
}

// PARTITION_SPLIT search.
static void split_partition_search(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    TokenExtra **tp, MACROBLOCK *x, PC_TREE *pc_tree,
    SIMPLE_MOTION_DATA_TREE *sms_tree, RD_SEARCH_MACROBLOCK_CONTEXT *x_ctx,
    PartitionSearchState *part_search_state, RD_STATS *best_rdc,
    SB_MULTI_PASS_MODE multi_pass_mode, int64_t *part_split_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  PartitionBlkParams blk_params = part_search_state->part_blk_params;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const int mi_row = blk_params.mi_row;
  const int mi_col = blk_params.mi_col;
  const int bsize = blk_params.bsize;
  assert(bsize < BLOCK_SIZES_ALL);
  RD_STATS sum_rdc = part_search_state->sum_rdc;
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);

  // Check if partition split is allowed.
  if (part_search_state->terminate_partition_search ||
      !part_search_state->do_square_split)
    return;

  for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
    if (pc_tree->split[i] == NULL)
      pc_tree->split[i] = av1_alloc_pc_tree_node(subsize);
    pc_tree->split[i]->index = i;
  }

  // Initialization of this partition RD stats.
  av1_init_rd_stats(&sum_rdc);
  sum_rdc.rate = part_search_state->partition_cost[PARTITION_SPLIT];
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);

  int idx;
#if CONFIG_COLLECT_PARTITION_STATS
  if (best_rdc->rdcost - sum_rdc.rdcost >= 0) {
    partition_attempts[PARTITION_SPLIT] += 1;
    aom_usec_timer_start(&partition_timer);
    partition_timer_on = 1;
  }
#endif
  // Recursive partition search on 4 sub-blocks.
  for (idx = 0; idx < SUB_PARTITIONS_SPLIT && sum_rdc.rdcost < best_rdc->rdcost;
       ++idx) {
    const int x_idx = (idx & 1) * blk_params.mi_step;
    const int y_idx = (idx >> 1) * blk_params.mi_step;

    if (mi_row + y_idx >= mi_params->mi_rows ||
        mi_col + x_idx >= mi_params->mi_cols)
      continue;

    pc_tree->split[idx]->index = idx;
    int64_t *p_split_rd = &part_search_state->split_rd[idx];
    RD_STATS best_remain_rdcost;
    av1_rd_stats_subtraction(x->rdmult, best_rdc, &sum_rdc,
                             &best_remain_rdcost);

    int curr_quad_tree_idx = 0;
    if (frame_is_intra_only(cm) && bsize <= BLOCK_64X64) {
      curr_quad_tree_idx = part_search_state->intra_part_info->quad_tree_idx;
      part_search_state->intra_part_info->quad_tree_idx =
          4 * curr_quad_tree_idx + idx + 1;
    }
    // Split partition evaluation of corresponding idx.
    // If the RD cost exceeds the best cost then do not
    // evaluate other split sub-partitions.
    if (!av1_rd_pick_partition(
            cpi, td, tile_data, tp, mi_row + y_idx, mi_col + x_idx, subsize,
            &part_search_state->this_rdc, best_remain_rdcost,
            pc_tree->split[idx], sms_tree->split[idx], p_split_rd,
            multi_pass_mode, &part_search_state->split_part_rect_win[idx])) {
      av1_invalid_rd_stats(&sum_rdc);
      break;
    }
    if (frame_is_intra_only(cm) && bsize <= BLOCK_64X64) {
      part_search_state->intra_part_info->quad_tree_idx = curr_quad_tree_idx;
    }

    sum_rdc.rate += part_search_state->this_rdc.rate;
    sum_rdc.dist += part_search_state->this_rdc.dist;
    av1_rd_cost_update(x->rdmult, &sum_rdc);

    // Set split ctx as ready for use.
    if (idx <= 1 && (bsize <= BLOCK_8X8 ||
                     pc_tree->split[idx]->partitioning == PARTITION_NONE)) {
      const MB_MODE_INFO *const mbmi = &pc_tree->split[idx]->none->mic;
      const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
      // Neither palette mode nor cfl predicted.
      if (pmi->palette_size[0] == 0 && pmi->palette_size[1] == 0) {
        if (mbmi->uv_mode != UV_CFL_PRED)
          part_search_state->is_split_ctx_is_ready[idx] = 1;
      }
    }
  }
#if CONFIG_COLLECT_PARTITION_STATS
  if (partition_timer_on) {
    aom_usec_timer_mark(&partition_timer);
    int64_t time = aom_usec_timer_elapsed(&partition_timer);
    partition_times[PARTITION_SPLIT] += time;
    partition_timer_on = 0;
  }
#endif
  const int reached_last_index = (idx == SUB_PARTITIONS_SPLIT);

  // Calculate the total cost and update the best partition.
  *part_split_rd = sum_rdc.rdcost;
  if (reached_last_index && sum_rdc.rdcost < best_rdc->rdcost) {
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
    if (sum_rdc.rdcost < best_rdc->rdcost) {
      *best_rdc = sum_rdc;
      part_search_state->found_best_partition = true;
      pc_tree->partitioning = PARTITION_SPLIT;
    }
  } else if (cpi->sf.part_sf.less_rectangular_check_level > 0) {
    // Skip rectangular partition test when partition type none gives better
    // rd than partition type split.
    if (cpi->sf.part_sf.less_rectangular_check_level == 2 || idx <= 2) {
      const int partition_none_valid = part_search_state->none_rd > 0;
      const int partition_none_better =
          part_search_state->none_rd < sum_rdc.rdcost;
      part_search_state->do_rectangular_split &=
          !(partition_none_valid && partition_none_better);
    }
  }
  av1_restore_context(x, x_ctx, mi_row, mi_col, bsize, av1_num_planes(cm));
}

/*!\brief AV1 block partition search (full search).
*
* \ingroup partition_search
* \callgraph
* Searches for the best partition pattern for a block based on the
* rate-distortion cost, and returns a bool value to indicate whether a valid
* partition pattern is found. The partition can recursively go down to the
* smallest block size.
*
* \param[in]    cpi                Top-level encoder structure
* \param[in]    td                 Pointer to thread data
* \param[in]    tile_data          Pointer to struct holding adaptive
data/contexts/models for the tile during
encoding
* \param[in]    tp                 Pointer to the starting token
* \param[in]    mi_row             Row coordinate of the block in a step size
of MI_SIZE
* \param[in]    mi_col             Column coordinate of the block in a step
size of MI_SIZE
* \param[in]    bsize              Current block size
* \param[in]    rd_cost            Pointer to the final rd cost of the block
* \param[in]    best_rdc           Upper bound of rd cost of a valid partition
* \param[in]    pc_tree            Pointer to the PC_TREE node storing the
picked partitions and mode info for the
current block
* \param[in]    sms_tree           Pointer to struct holding simple motion
search data for the current block
* \param[in]    none_rd            Pointer to the rd cost in the case of not
splitting the current block
* \param[in]    multi_pass_mode    SB_SINGLE_PASS/SB_DRY_PASS/SB_WET_PASS
* \param[in]    rect_part_win_info Pointer to struct storing whether horz/vert
partition outperforms previously tested
partitions
*
* \return A bool value is returned indicating if a valid partition is found.
* The pc_tree struct is modified to store the picked partition and modes.
* The rd_cost struct is also updated with the RD stats corresponding to the
* best partition found.
*/
bool av1_rd_pick_partition(AV1_COMP *const cpi, ThreadData *td,
                           TileDataEnc *tile_data, TokenExtra **tp, int mi_row,
                           int mi_col, BLOCK_SIZE bsize, RD_STATS *rd_cost,
                           RD_STATS best_rdc, PC_TREE *pc_tree,
                           SIMPLE_MOTION_DATA_TREE *sms_tree, int64_t *none_rd,
                           SB_MULTI_PASS_MODE multi_pass_mode,
                           RD_RECT_PART_WIN_INFO *rect_part_win_info) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  const TokenExtra *const tp_orig = *tp;
  PartitionSearchState part_search_state;
  // Initialization of state variables used in partition search.
  init_partition_search_state_params(x, cpi, &part_search_state, mi_row, mi_col,
                                     bsize);
  PartitionBlkParams blk_params = part_search_state.part_blk_params;

  sms_tree->partitioning = PARTITION_NONE;
  if (best_rdc.rdcost < 0) {
    av1_invalid_rd_stats(rd_cost);
    return part_search_state.found_best_partition;
  }
  if (bsize == cm->seq_params.sb_size) x->must_find_valid_partition = 0;

  // Override skipping rectangular partition operations for edge blocks.
  if (none_rd) *none_rd = 0;
  (void)*tp_orig;

#if CONFIG_COLLECT_PARTITION_STATS
  int partition_decisions[EXT_PARTITION_TYPES] = { 0 };
  int partition_attempts[EXT_PARTITION_TYPES] = { 0 };
  int64_t partition_times[EXT_PARTITION_TYPES] = { 0 };
  struct aom_usec_timer partition_timer = { 0 };
  int partition_timer_on = 0;
#if CONFIG_COLLECT_PARTITION_STATS == 2
  PartitionStats *part_stats = &cpi->partition_stats;
#endif
#endif

  // Override partition costs at the edges of the frame in the same
  // way as in read_partition (see decodeframe.c).
  if (!(blk_params.has_rows && blk_params.has_cols))
    set_partition_cost_for_edge_blk(cm, &part_search_state);

  // Disable rectangular partitions for inner blocks when the current block is
  // forced to only use square partitions.
  if (bsize > cpi->sf.part_sf.use_square_partition_only_threshold) {
    part_search_state.partition_rect_allowed[HORZ] &= !blk_params.has_rows;
    part_search_state.partition_rect_allowed[VERT] &= !blk_params.has_cols;
  }

#ifndef NDEBUG
  // Nothing should rely on the default value of this array (which is just
  // leftover from encoding the previous block. Setting it to fixed pattern
  // when debugging.
  // bit 0, 1, 2 are blk_skip of each plane
  // bit 4, 5, 6 are initialization checking of each plane
  memset(x->txfm_search_info.blk_skip, 0x77,
         sizeof(x->txfm_search_info.blk_skip));
#endif  // NDEBUG

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  // Set buffers and offsets.
  av1_set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize);

  // Save rdmult before it might be changed, so it can be restored later.
  const int orig_rdmult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, NO_AQ, NULL);

  // Update rd cost of the bound using the current multiplier.
  av1_rd_cost_update(x->rdmult, &best_rdc);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh)
    x->mb_energy = av1_log_block_var(cpi, x, bsize);

  // Set the context.
  xd->above_txfm_context =
      cm->above_contexts.txfm[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  av1_save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  int *partition_horz_allowed = &part_search_state.partition_rect_allowed[HORZ];
  int *partition_vert_allowed = &part_search_state.partition_rect_allowed[VERT];
  int *prune_horz = &part_search_state.prune_rect_part[HORZ];
  int *prune_vert = &part_search_state.prune_rect_part[VERT];
  // Pruning: before searching any partition type, using source and simple
  // motion search results to prune out unlikely partitions.
  av1_prune_partitions_before_search(
      cpi, x, mi_row, mi_col, bsize, sms_tree,
      &part_search_state.partition_none_allowed, partition_horz_allowed,
      partition_vert_allowed, &part_search_state.do_rectangular_split,
      &part_search_state.do_square_split, prune_horz, prune_vert);

  // Pruning: eliminating partition types leading to coding block sizes outside
  // the min and max bsize limitations set from the encoder.
  av1_prune_partitions_by_max_min_bsize(
      &x->sb_enc, bsize, blk_params.has_rows && blk_params.has_cols,
      &part_search_state.partition_none_allowed, partition_horz_allowed,
      partition_vert_allowed, &part_search_state.do_square_split);

  // Partition search
BEGIN_PARTITION_SEARCH:
  // If a valid partition is required, usually when the first round cannot find
  // a valid one under the cost limit after pruning, reset the limitations on
  // partition types and intra cnn output.
  if (x->must_find_valid_partition) {
    reset_part_limitations(cpi, &part_search_state);
    // Invalidate intra cnn output for key frames.
    if (frame_is_intra_only(cm) && bsize == BLOCK_64X64) {
      part_search_state.intra_part_info->quad_tree_idx = 0;
      part_search_state.intra_part_info->cnn_output_valid = 0;
    }
  }
  // Partition block source pixel variance.
  unsigned int pb_source_variance = UINT_MAX;

  // PARTITION_NONE search stage.
  int64_t part_none_rd = INT64_MAX;
  none_partition_search(cpi, td, tile_data, x, pc_tree, sms_tree, &x_ctx,
                        &part_search_state, &best_rdc, &pb_source_variance,
                        none_rd, &part_none_rd);

  // PARTITION_SPLIT search stage.
  int64_t part_split_rd = INT64_MAX;
  split_partition_search(cpi, td, tile_data, tp, x, pc_tree, sms_tree, &x_ctx,
                         &part_search_state, &best_rdc, multi_pass_mode,
                         &part_split_rd);

  // Terminate partition search for child partition,
  // when NONE and SPLIT partition rd_costs are INT64_MAX.
  if (cpi->sf.part_sf.early_term_after_none_split &&
      part_none_rd == INT64_MAX && part_split_rd == INT64_MAX &&
      !x->must_find_valid_partition && (bsize != cm->seq_params.sb_size)) {
    part_search_state.terminate_partition_search = 1;
  }

  // Prune partitions based on PARTITION_NONE and PARTITION_SPLIT.
  prune_partitions_after_split(cpi, x, sms_tree, &part_search_state, &best_rdc,
                               part_none_rd, part_split_rd);

  // Rectangular partitions search stage.
  rectangular_partition_search(cpi, td, tile_data, tp, x, pc_tree, &x_ctx,
                               &part_search_state, &best_rdc,
                               rect_part_win_info);

  if (pb_source_variance == UINT_MAX) {
    av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes, bsize);
    if (is_cur_buf_hbd(xd)) {
      pb_source_variance = av1_high_get_sby_perpixel_variance(
          cpi, &x->plane[0].src, bsize, xd->bd);
    } else {
      pb_source_variance =
          av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
    }
  }

  assert(IMPLIES(!cpi->oxcf.part_cfg.enable_rect_partitions,
                 !part_search_state.do_rectangular_split));

  const int ext_partition_allowed =
      part_search_state.do_rectangular_split &&
      bsize > cpi->sf.part_sf.ext_partition_eval_thresh &&
      blk_params.has_rows && blk_params.has_cols;

  // AB partitions search stage.
  ab_partitions_search(cpi, td, tile_data, tp, x, &x_ctx, pc_tree,
                       &part_search_state, &best_rdc, rect_part_win_info,
                       pb_source_variance, ext_partition_allowed);

  // 4-way partitions search stage.
  int part4_search_allowed[NUM_PART4_TYPES] = { 1, 1 };

  // Disable 4-way partition search flags for width less than twice the minimum
  // width.
  if (blk_params.width < (blk_params.min_partition_size_1d << 2)) {
    part4_search_allowed[HORZ4] = 0;
    part4_search_allowed[VERT4] = 0;
  } else {
    // Prune 4-way partition search.
    prune_4_way_partition_search(cpi, x, pc_tree, &part_search_state, &best_rdc,
                                 pb_source_variance, ext_partition_allowed,
                                 part4_search_allowed);
  }

  // PARTITION_HORZ_4
  assert(IMPLIES(!cpi->oxcf.part_cfg.enable_rect_partitions,
                 !part4_search_allowed[HORZ4]));
  if (!part_search_state.terminate_partition_search &&
      part4_search_allowed[HORZ4] && blk_params.has_rows &&
      (part_search_state.do_rectangular_split ||
       av1_active_h_edge(cpi, mi_row, blk_params.mi_step))) {
    const int inc_step[NUM_PART4_TYPES] = { mi_size_high[blk_params.bsize] / 4,
                                            0 };
    // Evaluation of Horz4 partition type.
    rd_pick_4partition(cpi, td, tile_data, tp, x, &x_ctx, pc_tree,
                       pc_tree->horizontal4, &part_search_state, &best_rdc,
                       inc_step, PARTITION_HORZ_4);
  }

  // PARTITION_VERT_4
  assert(IMPLIES(!cpi->oxcf.part_cfg.enable_rect_partitions,
                 !part4_search_allowed[VERT4]));
  if (!part_search_state.terminate_partition_search &&
      part4_search_allowed[VERT4] && blk_params.has_cols &&
      (part_search_state.do_rectangular_split ||
       av1_active_v_edge(cpi, mi_row, blk_params.mi_step))) {
    const int inc_step[NUM_PART4_TYPES] = { 0, mi_size_wide[blk_params.bsize] /
                                                   4 };
    // Evaluation of Vert4 partition type.
    rd_pick_4partition(cpi, td, tile_data, tp, x, &x_ctx, pc_tree,
                       pc_tree->vertical4, &part_search_state, &best_rdc,
                       inc_step, PARTITION_VERT_4);
  }

  if (bsize == cm->seq_params.sb_size &&
      !part_search_state.found_best_partition) {
    // Did not find a valid partition, go back and search again, with less
    // constraint on which partition types to search.
    x->must_find_valid_partition = 1;
#if CONFIG_COLLECT_PARTITION_STATS == 2
    part_stats->partition_redo += 1;
#endif
    goto BEGIN_PARTITION_SEARCH;
  }

  // Store the final rd cost
  *rd_cost = best_rdc;

  // Also record the best partition in simple motion data tree because it is
  // necessary for the related speed features.
  sms_tree->partitioning = pc_tree->partitioning;

#if CONFIG_COLLECT_PARTITION_STATS
  if (best_rdc.rate < INT_MAX && best_rdc.dist < INT64_MAX) {
    partition_decisions[pc_tree->partitioning] += 1;
  }
#endif

#if CONFIG_COLLECT_PARTITION_STATS == 1
  // If CONFIG_COLLECT_PARTITION_STATS is 1, then print out the stats for each
  // prediction block.
  FILE *f = fopen("data.csv", "a");
  fprintf(f, "%d,%d,%d,", bsize, cm->show_frame, frame_is_intra_only(cm));
  for (int idx = 0; idx < EXT_PARTITION_TYPES; idx++) {
    fprintf(f, "%d,", partition_decisions[idx]);
  }
  for (int idx = 0; idx < EXT_PARTITION_TYPES; idx++) {
    fprintf(f, "%d,", partition_attempts[idx]);
  }
  for (int idx = 0; idx < EXT_PARTITION_TYPES; idx++) {
    fprintf(f, "%ld,", partition_times[idx]);
  }
  fprintf(f, "\n");
  fclose(f);
#endif

#if CONFIG_COLLECT_PARTITION_STATS == 2
  // If CONFIG_COLLECTION_PARTITION_STATS is 2, then we print out the stats for
  // the whole clip. So we need to pass the information upstream to the encoder.
  const int bsize_idx = av1_get_bsize_idx_for_part_stats(bsize);
  int *agg_attempts = part_stats->partition_attempts[bsize_idx];
  int *agg_decisions = part_stats->partition_decisions[bsize_idx];
  int64_t *agg_times = part_stats->partition_times[bsize_idx];
  for (int idx = 0; idx < EXT_PARTITION_TYPES; idx++) {
    agg_attempts[idx] += partition_attempts[idx];
    agg_decisions[idx] += partition_decisions[idx];
    agg_times[idx] += partition_times[idx];
  }
#endif

  // Reset the PC_TREE deallocation flag.
  int pc_tree_dealloc = 0;

  // If a valid partition is found and reconstruction is required for future
  // sub-blocks in the same group.
  if (part_search_state.found_best_partition && pc_tree->index != 3) {
    if (bsize == cm->seq_params.sb_size) {
      // Encode the superblock.
      const int emit_output = multi_pass_mode != SB_DRY_PASS;
      const RUN_TYPE run_type = emit_output ? OUTPUT_ENABLED : DRY_RUN_NORMAL;

      set_cb_offsets(x->cb_offset, 0, 0);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, run_type, bsize,
                pc_tree, NULL);
      // Dealloc the whole PC_TREE after a superblock is done.
      av1_free_pc_tree_recursive(pc_tree, num_planes, 0, 0);
      pc_tree_dealloc = 1;
    } else {
      // Encode the smaller blocks in DRY_RUN mode.
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

  // If the tree still exists (non-superblock), dealloc most nodes, only keep
  // nodes for the best partition and PARTITION_NONE.
  if (pc_tree_dealloc == 0)
    av1_free_pc_tree_recursive(pc_tree, num_planes, 1, 1);

  if (bsize == cm->seq_params.sb_size) {
    assert(best_rdc.rate < INT_MAX);
    assert(best_rdc.dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }

  // Restore the rd multiplier.
  x->rdmult = orig_rdmult;
  return part_search_state.found_best_partition;
}
#endif  // !CONFIG_REALTIME_ONLY

#define FEATURES 6
#define LABELS 2
static int ml_predict_var_paritioning(AV1_COMP *cpi, MACROBLOCK *x,
                                      BLOCK_SIZE bsize, int mi_row,
                                      int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const NN_CONFIG *nn_config = NULL;
  const float *means = NULL;
  const float *vars = NULL;
  switch (bsize) {
    case BLOCK_64X64:
      nn_config = &av1_var_part_nnconfig_64;
      means = av1_var_part_means_64;
      vars = av1_var_part_vars_64;
      break;
    case BLOCK_32X32:
      nn_config = &av1_var_part_nnconfig_32;
      means = av1_var_part_means_32;
      vars = av1_var_part_vars_32;
      break;
    case BLOCK_16X16:
      nn_config = &av1_var_part_nnconfig_16;
      means = av1_var_part_means_16;
      vars = av1_var_part_vars_16;
      break;
    case BLOCK_8X8:
    default: assert(0 && "Unexpected block size."); return -1;
  }

  if (!nn_config) return -1;

  aom_clear_system_state();

  {
    const float thresh = cpi->oxcf.speed <= 5 ? 1.25f : 0.0f;
    float features[FEATURES] = { 0.0f };
    const int dc_q = av1_dc_quant_QTX(cm->quant_params.base_qindex, 0,
                                      cm->seq_params.bit_depth);
    int feature_idx = 0;
    float score[LABELS];

    features[feature_idx] =
        (logf((float)(dc_q * dc_q) / 256.0f + 1.0f) - means[feature_idx]) /
        sqrtf(vars[feature_idx]);
    feature_idx++;
    av1_setup_src_planes(x, cpi->source, mi_row, mi_col, 1, bsize);
    {
      const int bs = block_size_wide[bsize];
      const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
      const int sb_offset_row = 4 * (mi_row & 15);
      const int sb_offset_col = 4 * (mi_col & 15);
      const uint8_t *pred = x->est_pred + sb_offset_row * 64 + sb_offset_col;
      const uint8_t *src = x->plane[0].src.buf;
      const int src_stride = x->plane[0].src.stride;
      const int pred_stride = 64;
      unsigned int sse;
      int i;
      // Variance of whole block.
      const unsigned int var =
          cpi->fn_ptr[bsize].vf(src, src_stride, pred, pred_stride, &sse);
      const float factor = (var == 0) ? 1.0f : (1.0f / (float)var);

      features[feature_idx] = (logf((float)var + 1.0f) - means[feature_idx]) /
                              sqrtf(vars[feature_idx]);
      feature_idx++;
      for (i = 0; i < 4; ++i) {
        const int x_idx = (i & 1) * bs / 2;
        const int y_idx = (i >> 1) * bs / 2;
        const int src_offset = y_idx * src_stride + x_idx;
        const int pred_offset = y_idx * pred_stride + x_idx;
        // Variance of quarter block.
        const unsigned int sub_var =
            cpi->fn_ptr[subsize].vf(src + src_offset, src_stride,
                                    pred + pred_offset, pred_stride, &sse);
        const float var_ratio = (var == 0) ? 1.0f : factor * (float)sub_var;
        features[feature_idx] =
            (var_ratio - means[feature_idx]) / sqrtf(vars[feature_idx]);
        feature_idx++;
      }
    }
    //    for (int i = 0; i<FEATURES; i++)
    //      printf("F_%d, %f; ", i, features[i]);
    assert(feature_idx == FEATURES);
    av1_nn_predict(features, nn_config, 1, score);
    //    printf("Score %f, thr %f ", (float)score[0], thresh);
    if (score[0] > thresh) return PARTITION_SPLIT;
    if (score[0] < -thresh) return PARTITION_NONE;
    return -1;
  }
}
#undef FEATURES
#undef LABELS

// Uncomment for collecting data for ML-based partitioning
// #define _COLLECT_GROUND_TRUTH_

#ifdef _COLLECT_GROUND_TRUTH_
static int store_partition_data(AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
                                int mi_row, int mi_col, PARTITION_TYPE part) {
  AV1_COMMON *const cm = &cpi->common;
  char fname[128];
  switch (bsize) {
    case BLOCK_64X64: sprintf(fname, "data_64x64.txt"); break;
    case BLOCK_32X32: sprintf(fname, "data_32x32.txt"); break;
    case BLOCK_16X16: sprintf(fname, "data_16x16.txt"); break;
    case BLOCK_8X8: sprintf(fname, "data_8x8.txt"); break;
    default: assert(0 && "Unexpected block size."); return -1;
  }

  float features[6];  // DC_Q, VAR, VAR_RATIO-0..3

  FILE *f = fopen(fname, "a");

  aom_clear_system_state();

  {
    const int dc_q = av1_dc_quant_QTX(cm->quant_params.base_qindex, 0,
                                      cm->seq_params.bit_depth);
    int feature_idx = 0;

    features[feature_idx++] = logf((float)(dc_q * dc_q) / 256.0f + 1.0f);
    av1_setup_src_planes(x, cpi->source, mi_row, mi_col, 1, bsize);
    {
      const int bs = block_size_wide[bsize];
      const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
      const int sb_offset_row = 4 * (mi_row & 15);
      const int sb_offset_col = 4 * (mi_col & 15);
      const uint8_t *pred = x->est_pred + sb_offset_row * 64 + sb_offset_col;
      const uint8_t *src = x->plane[0].src.buf;
      const int src_stride = x->plane[0].src.stride;
      const int pred_stride = 64;
      unsigned int sse;
      int i;
      // Variance of whole block.
      /*
                if (bs == 8)
                {
                  int r, c;
                  printf("%d %d\n", mi_row, mi_col);
                  for (r = 0; r < bs; ++r) {
                    for (c = 0; c < bs; ++c) {
                      printf("%3d ",
                             src[r * src_stride + c] - pred[64 * r + c]);
                    }
                    printf("\n");
                  }
                  printf("\n");
                }
      */
      const unsigned int var =
          cpi->fn_ptr[bsize].vf(src, src_stride, pred, pred_stride, &sse);
      const float factor = (var == 0) ? 1.0f : (1.0f / (float)var);

      features[feature_idx++] = logf((float)var + 1.0f);

      fprintf(f, "%f,%f,", features[0], features[1]);
      for (i = 0; i < 4; ++i) {
        const int x_idx = (i & 1) * bs / 2;
        const int y_idx = (i >> 1) * bs / 2;
        const int src_offset = y_idx * src_stride + x_idx;
        const int pred_offset = y_idx * pred_stride + x_idx;
        // Variance of quarter block.
        const unsigned int sub_var =
            cpi->fn_ptr[subsize].vf(src + src_offset, src_stride,
                                    pred + pred_offset, pred_stride, &sse);
        const float var_ratio = (var == 0) ? 1.0f : factor * (float)sub_var;
        features[feature_idx++] = var_ratio;
        fprintf(f, "%f,", var_ratio);
      }

      fprintf(f, "%d\n", part == PARTITION_NONE ? 0 : 1);
    }

    fclose(f);
    return -1;
  }
}
#endif

static void duplicate_mode_info_in_sb(AV1_COMMON *cm, MACROBLOCKD *xd,
                                      int mi_row, int mi_col,
                                      BLOCK_SIZE bsize) {
  const int block_width =
      AOMMIN(mi_size_wide[bsize], cm->mi_params.mi_cols - mi_col);
  const int block_height =
      AOMMIN(mi_size_high[bsize], cm->mi_params.mi_rows - mi_row);
  const int mi_stride = xd->mi_stride;
  MB_MODE_INFO *const src_mi = xd->mi[0];
  int i, j;

  for (j = 0; j < block_height; ++j)
    for (i = 0; i < block_width; ++i) xd->mi[j * mi_stride + i] = src_mi;
}

static INLINE void copy_mbmi_ext_frame_to_mbmi_ext(
    MB_MODE_INFO_EXT *const mbmi_ext,
    const MB_MODE_INFO_EXT_FRAME *mbmi_ext_best, uint8_t ref_frame_type) {
  memcpy(mbmi_ext->ref_mv_stack[ref_frame_type], mbmi_ext_best->ref_mv_stack,
         sizeof(mbmi_ext->ref_mv_stack[USABLE_REF_MV_STACK_SIZE]));
  memcpy(mbmi_ext->weight[ref_frame_type], mbmi_ext_best->weight,
         sizeof(mbmi_ext->weight[USABLE_REF_MV_STACK_SIZE]));
  mbmi_ext->mode_context[ref_frame_type] = mbmi_ext_best->mode_context;
  mbmi_ext->ref_mv_count[ref_frame_type] = mbmi_ext_best->ref_mv_count;
  memcpy(mbmi_ext->global_mvs, mbmi_ext_best->global_mvs,
         sizeof(mbmi_ext->global_mvs));
}

static void fill_mode_info_sb(AV1_COMP *cpi, MACROBLOCK *x, int mi_row,
                              int mi_col, BLOCK_SIZE bsize, PC_TREE *pc_tree) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  int hbs = mi_size_wide[bsize] >> 1;
  PARTITION_TYPE partition = pc_tree->partitioning;
  BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);

  assert(bsize >= BLOCK_8X8);

  if (mi_row >= cm->mi_params.mi_rows || mi_col >= cm->mi_params.mi_cols)
    return;

  switch (partition) {
    case PARTITION_NONE:
      set_mode_info_offsets(&cm->mi_params, &cpi->mbmi_ext_info, x, xd, mi_row,
                            mi_col);
      *(xd->mi[0]) = pc_tree->none->mic;
      copy_mbmi_ext_frame_to_mbmi_ext(
          &x->mbmi_ext, &pc_tree->none->mbmi_ext_best, LAST_FRAME);
      duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);
      break;
    case PARTITION_SPLIT: {
      fill_mode_info_sb(cpi, x, mi_row, mi_col, subsize, pc_tree->split[0]);
      fill_mode_info_sb(cpi, x, mi_row, mi_col + hbs, subsize,
                        pc_tree->split[1]);
      fill_mode_info_sb(cpi, x, mi_row + hbs, mi_col, subsize,
                        pc_tree->split[2]);
      fill_mode_info_sb(cpi, x, mi_row + hbs, mi_col + hbs, subsize,
                        pc_tree->split[3]);
      break;
    }
    default: break;
  }
}

void av1_nonrd_pick_partition(AV1_COMP *cpi, ThreadData *td,
                              TileDataEnc *tile_data, TokenExtra **tp,
                              int mi_row, int mi_col, BLOCK_SIZE bsize,
                              RD_STATS *rd_cost, int do_recon, int64_t best_rd,
                              PC_TREE *pc_tree) {
  AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int hbs = mi_size_wide[bsize] >> 1;
  TokenExtra *tp_orig = *tp;
  const ModeCosts *mode_costs = &x->mode_costs;
  RD_STATS this_rdc, best_rdc;
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  int do_split = bsize > BLOCK_8X8;
  // Override skipping rectangular partition operations for edge blocks
  const int force_horz_split = (mi_row + 2 * hbs > cm->mi_params.mi_rows);
  const int force_vert_split = (mi_col + 2 * hbs > cm->mi_params.mi_cols);

  int partition_none_allowed = !force_horz_split && !force_vert_split;

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);  // Square partition only
  assert(cm->seq_params.sb_size == BLOCK_64X64);       // Small SB so far

  (void)*tp_orig;

  av1_invalid_rd_stats(&best_rdc);
  best_rdc.rdcost = best_rd;
#ifndef _COLLECT_GROUND_TRUTH_
  if (partition_none_allowed && do_split) {
    const int ml_predicted_partition =
        ml_predict_var_paritioning(cpi, x, bsize, mi_row, mi_col);
    if (ml_predicted_partition == PARTITION_NONE) do_split = 0;
    if (ml_predicted_partition == PARTITION_SPLIT) partition_none_allowed = 0;
  }
#endif

  xd->above_txfm_context =
      cm->above_contexts.txfm[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  av1_save_context(x, &x_ctx, mi_row, mi_col, bsize, 3);

  // PARTITION_NONE
  if (partition_none_allowed) {
    pc_tree->none = av1_alloc_pmc(cm, bsize, &td->shared_coeff_buf);
    PICK_MODE_CONTEXT *ctx = pc_tree->none;

// Flip for RDO based pick mode
#if 0
    RD_STATS dummy;
    av1_invalid_rd_stats(&dummy);
    pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc,
                  PARTITION_NONE, bsize, ctx, dummy);
#else
    pick_sb_modes_nonrd(cpi, tile_data, x, mi_row, mi_col, &this_rdc, bsize,
                        ctx);
#endif
    if (this_rdc.rate != INT_MAX) {
      const int pl = partition_plane_context(xd, mi_row, mi_col, bsize);

      this_rdc.rate += mode_costs->partition_cost[pl][PARTITION_NONE];
      this_rdc.rdcost = RDCOST(x->rdmult, this_rdc.rate, this_rdc.dist);
      if (this_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = this_rdc;
        if (bsize >= BLOCK_8X8) pc_tree->partitioning = PARTITION_NONE;
      }
    }
  }

  // PARTITION_SPLIT
  if (do_split) {
    RD_STATS sum_rdc;
    const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);

    av1_init_rd_stats(&sum_rdc);

    for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
      pc_tree->split[i] = av1_alloc_pc_tree_node(subsize);
      pc_tree->split[i]->index = i;
    }

    int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
    sum_rdc.rate += mode_costs->partition_cost[pl][PARTITION_SPLIT];
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
    for (int i = 0;
         i < SUB_PARTITIONS_SPLIT && sum_rdc.rdcost < best_rdc.rdcost; ++i) {
      const int x_idx = (i & 1) * hbs;
      const int y_idx = (i >> 1) * hbs;

      if (mi_row + y_idx >= cm->mi_params.mi_rows ||
          mi_col + x_idx >= cm->mi_params.mi_cols)
        continue;
      av1_nonrd_pick_partition(cpi, td, tile_data, tp, mi_row + y_idx,
                               mi_col + x_idx, subsize, &this_rdc, i < 3,
                               best_rdc.rdcost - sum_rdc.rdcost,
                               pc_tree->split[i]);

      if (this_rdc.rate == INT_MAX) {
        av1_invalid_rd_stats(&sum_rdc);
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        sum_rdc.rdcost += this_rdc.rdcost;
      }
    }
    if (sum_rdc.rdcost < best_rdc.rdcost) {
      best_rdc = sum_rdc;
      pc_tree->partitioning = PARTITION_SPLIT;
    }
  }

#ifdef _COLLECT_GROUND_TRUTH_
  store_partition_data(cpi, x, bsize, mi_row, mi_col, pc_tree->partitioning);
#endif

  *rd_cost = best_rdc;

  av1_restore_context(x, &x_ctx, mi_row, mi_col, bsize, 3);

  if (best_rdc.rate == INT_MAX) {
    av1_invalid_rd_stats(rd_cost);
    return;
  }

  // update mode info array
  fill_mode_info_sb(cpi, x, mi_row, mi_col, bsize, pc_tree);

  if (do_recon) {
    if (bsize == cm->seq_params.sb_size) {
      // NOTE: To get estimate for rate due to the tokens, use:
      // int rate_coeffs = 0;
      // encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_COSTCOEFFS,
      //           bsize, pc_tree, &rate_coeffs);
      set_cb_offsets(x->cb_offset, 0, 0);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, NULL);
    } else {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL);
    }
  }

  if (bsize == BLOCK_64X64 && do_recon) {
    assert(best_rdc.rate < INT_MAX);
    assert(best_rdc.dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}
