/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/idct.h"

#include "av1/encoder/allintra_vis.h"
#include "av1/encoder/hybrid_fwd_txfm.h"

// Process the wiener variance in 16x16 block basis.
static int qsort_comp(const void *elem1, const void *elem2) {
  int a = *((const int *)elem1);
  int b = *((const int *)elem2);
  if (a > b) return 1;
  if (a < b) return -1;
  return 0;
}

void av1_init_mb_wiener_var_buffer(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;

  if (cpi->mb_weber_stats) return;

  CHECK_MEM_ERROR(cm, cpi->mb_weber_stats,
                  aom_calloc(cpi->frame_info.mb_rows * cpi->frame_info.mb_cols,
                             sizeof(*cpi->mb_weber_stats)));
}

static int64_t get_satd(AV1_COMP *const cpi, BLOCK_SIZE bsize, int mi_row,
                        int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  const int mi_step = mi_size_wide[BLOCK_16X16];
  int mb_stride = cpi->frame_info.mb_cols;
  int mb_count = 0;
  int64_t satd = 0;

  for (int row = mi_row; row < mi_row + mi_high; row += mi_step) {
    for (int col = mi_col; col < mi_col + mi_wide; col += mi_step) {
      if (row >= cm->mi_params.mi_rows || col >= cm->mi_params.mi_cols)
        continue;

      satd += cpi->mb_weber_stats[(row / mi_step) * mb_stride + (col / mi_step)]
                  .satd;
      ++mb_count;
    }
  }

  if (mb_count) satd = (int)(satd / mb_count);
  satd = AOMMAX(1, satd);

  return (int)satd;
}

static int64_t get_sse(AV1_COMP *const cpi, BLOCK_SIZE bsize, int mi_row,
                       int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  const int mi_step = mi_size_wide[BLOCK_16X16];
  int mb_stride = cpi->frame_info.mb_cols;
  int mb_count = 0;
  int64_t distortion = 0;

  for (int row = mi_row; row < mi_row + mi_high; row += mi_step) {
    for (int col = mi_col; col < mi_col + mi_wide; col += mi_step) {
      if (row >= cm->mi_params.mi_rows || col >= cm->mi_params.mi_cols)
        continue;

      distortion +=
          cpi->mb_weber_stats[(row / mi_step) * mb_stride + (col / mi_step)]
              .distortion;
      ++mb_count;
    }
  }

  if (mb_count) distortion = (int)(distortion / mb_count);
  distortion = AOMMAX(1, distortion);

  return (int)distortion;
}

static int get_window_wiener_var(AV1_COMP *const cpi, BLOCK_SIZE bsize,
                                 int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  const int mi_step = mi_size_wide[BLOCK_16X16];
  int sb_wiener_var = 0;
  int mb_stride = cpi->frame_info.mb_cols;
  int mb_count = 0;
  int64_t mb_wiener_sum = 0;
  double base_num = 1;
  double base_den = 1;
  double base_reg = 1;

  for (int row = mi_row; row < mi_row + mi_high; row += mi_step) {
    for (int col = mi_col; col < mi_col + mi_wide; col += mi_step) {
      if (row >= cm->mi_params.mi_rows || col >= cm->mi_params.mi_cols)
        continue;

      WeberStats *weber_stats =
          &cpi->mb_weber_stats[(row / mi_step) * mb_stride + (col / mi_step)];
      mb_wiener_sum += (int)(cpi->mb_weber_stats[(row / mi_step) * mb_stride +
                                                 (col / mi_step)]
                                 .alpha *
                             10000);

      base_num += sqrt((double)weber_stats->distortion) *
                  sqrt((double)weber_stats->src_variance) *
                  weber_stats->rec_pix_max;

      base_den += fabs(
          weber_stats->rec_pix_max * sqrt((double)weber_stats->src_variance) -
          weber_stats->src_pix_max * sqrt((double)weber_stats->rec_variance));

      base_reg += sqrt((double)weber_stats->distortion) *
                  sqrt((double)weber_stats->src_pix_max) * 0.1;
      ++mb_count;
    }
  }

  sb_wiener_var = (int)((base_num + base_reg) / (base_den + base_reg));
  sb_wiener_var = AOMMAX(1, sb_wiener_var);

  return (int)sb_wiener_var;
}

static int get_var_perceptual_ai(AV1_COMP *const cpi, BLOCK_SIZE bsize,
                                 int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  int sb_wiener_var = get_window_wiener_var(cpi, bsize, mi_row, mi_col);

  if (mi_row >= (mi_high / 2)) {
    sb_wiener_var =
        AOMMIN(sb_wiener_var,
               get_window_wiener_var(cpi, bsize, mi_row - mi_high / 2, mi_col));
  }
  if (mi_row <= (cm->mi_params.mi_rows - mi_high - (mi_high / 2))) {
    sb_wiener_var =
        AOMMIN(sb_wiener_var,
               get_window_wiener_var(cpi, bsize, mi_row + mi_high / 2, mi_col));
  }
  if (mi_col >= (mi_wide / 2)) {
    sb_wiener_var =
        AOMMIN(sb_wiener_var,
               get_window_wiener_var(cpi, bsize, mi_row, mi_col - mi_wide / 2));
  }
  if (mi_col <= (cm->mi_params.mi_cols - mi_wide - (mi_wide / 2))) {
    sb_wiener_var =
        AOMMIN(sb_wiener_var,
               get_window_wiener_var(cpi, bsize, mi_row, mi_col + mi_wide / 2));
  }

  return sb_wiener_var;
}

void av1_set_mb_wiener_variance(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  uint8_t *buffer = cpi->source->y_buffer;
  int buf_stride = cpi->source->y_stride;
  ThreadData *td = &cpi->td;
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO mbmi;
  memset(&mbmi, 0, sizeof(mbmi));
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  xd->mi = &mbmi_ptr;

  cm->quant_params.base_qindex = cpi->oxcf.rc_cfg.cq_level;
  av1_frame_init_quantizer(cpi);

  union {
#if CONFIG_AV1_HIGHBITDEPTH
    DECLARE_ALIGNED(32, uint16_t, zero_pred16[32 * 32]);
#endif
    DECLARE_ALIGNED(32, uint8_t, zero_pred8[32 * 32]);
  } pred_buffer_mem;
  uint8_t *pred_buf;

  DECLARE_ALIGNED(32, int16_t, src_diff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, qcoeff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, dqcoeff[32 * 32]);

  int mb_row, mb_col, count = 0;
  const TX_SIZE tx_size = TX_16X16;
  const int block_size = tx_size_wide[tx_size];
  const int coeff_count = block_size * block_size;

#if CONFIG_AV1_HIGHBITDEPTH
  xd->cur_buf = cpi->source;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    pred_buf = CONVERT_TO_BYTEPTR(pred_buffer_mem.zero_pred16);
    memset(pred_buffer_mem.zero_pred16, 0,
           sizeof(*pred_buffer_mem.zero_pred16) * coeff_count);
  } else {
    pred_buf = pred_buffer_mem.zero_pred8;
    memset(pred_buffer_mem.zero_pred8, 0,
           sizeof(*pred_buffer_mem.zero_pred8) * coeff_count);
  }
#else
  pred_buf = pred_buffer_mem.zero_pred8;
  memset(pred_buffer_mem.zero_pred8, 0,
         sizeof(*pred_buffer_mem.zero_pred8) * coeff_count);
#endif

  const BitDepthInfo bd_info = get_bit_depth_info(xd);
  cpi->norm_wiener_variance = 0;

  int mb_step = mi_size_wide[BLOCK_16X16];

  for (mb_row = 0; mb_row < cpi->frame_info.mb_rows; ++mb_row) {
    for (mb_col = 0; mb_col < cpi->frame_info.mb_cols; ++mb_col) {
      PREDICTION_MODE best_mode = DC_PRED;
      int best_intra_cost = INT_MAX;

      int mi_row = mb_row * mb_step;
      int mi_col = mb_col * mb_step;

      xd->up_available = mi_row > 0;
      xd->left_available = mi_col > 0;

      int dst_mb_offset = mi_row * MI_SIZE * buf_stride + mi_col * MI_SIZE;
      uint8_t *dst_buffer = xd->cur_buf->y_buffer + dst_mb_offset;

      for (PREDICTION_MODE mode = INTRA_MODE_START; mode < INTRA_MODE_END;
           ++mode) {
        av1_predict_intra_block(xd, cm->seq_params->sb_size,
                                cm->seq_params->enable_intra_edge_filter,
                                block_size, block_size, tx_size, mode, 0, 0,
                                FILTER_INTRA_MODES, dst_buffer, buf_stride,
                                pred_buf, block_size, 0, 0, 0);

        av1_subtract_block(bd_info, block_size, block_size, src_diff,
                           block_size, dst_buffer, buf_stride, pred_buf,
                           block_size);
        av1_quick_txfm(0, tx_size, bd_info, src_diff, block_size, coeff);
        int intra_cost = aom_satd(coeff, coeff_count);
        if (intra_cost < best_intra_cost) {
          best_intra_cost = intra_cost;
          best_mode = mode;
        }
      }

      int idx;
      int16_t median_val = 0;
      uint8_t *mb_buffer =
          buffer + mb_row * block_size * buf_stride + mb_col * block_size;
      int64_t wiener_variance = 0;
      av1_predict_intra_block(
          xd, cm->seq_params->sb_size, cm->seq_params->enable_intra_edge_filter,
          block_size, block_size, tx_size, best_mode, 0, 0, FILTER_INTRA_MODES,
          dst_buffer, buf_stride, pred_buf, block_size, 0, 0, 0);
      av1_subtract_block(bd_info, block_size, block_size, src_diff, block_size,
                         mb_buffer, buf_stride, pred_buf, block_size);
      av1_quick_txfm(0, tx_size, bd_info, src_diff, block_size, coeff);

      const struct macroblock_plane *const p = &x->plane[0];
      uint16_t eob;
      const SCAN_ORDER *const scan_order = &av1_scan_orders[tx_size][DCT_DCT];
      QUANT_PARAM quant_param;
      int pix_num = 1 << num_pels_log2_lookup[txsize_to_bsize[tx_size]];
      av1_setup_quant(tx_size, 0, AV1_XFORM_QUANT_FP, 0, &quant_param);
#if CONFIG_AV1_HIGHBITDEPTH
      if (is_cur_buf_hbd(xd)) {
        av1_highbd_quantize_fp_facade(coeff, pix_num, p, qcoeff, dqcoeff, &eob,
                                      scan_order, &quant_param);
      } else {
        av1_quantize_fp_facade(coeff, pix_num, p, qcoeff, dqcoeff, &eob,
                               scan_order, &quant_param);
      }
#else
      av1_quantize_fp_facade(coeff, pix_num, p, qcoeff, dqcoeff, &eob,
                             scan_order, &quant_param);
#endif  // CONFIG_AV1_HIGHBITDEPTH
      av1_inverse_transform_block(xd, dqcoeff, 0, DCT_DCT, tx_size, pred_buf,
                                  block_size, eob, 0);

      WeberStats *weber_stats =
          &cpi->mb_weber_stats[mb_row * cpi->frame_info.mb_cols + mb_col];

      weber_stats->rec_pix_max = 1;
      weber_stats->rec_variance = 0;
      weber_stats->src_pix_max = 1;
      weber_stats->src_variance = 0;
      weber_stats->distortion = 0;

      int64_t src_mean = 0;
      int64_t rec_mean = 0;
      int64_t dist_mean = 0;

      for (int pix_row = 0; pix_row < block_size; ++pix_row) {
        for (int pix_col = 0; pix_col < block_size; ++pix_col) {
          int src_pix, rec_pix;
#if CONFIG_AV1_HIGHBITDEPTH
          if (is_cur_buf_hbd(xd)) {
            uint16_t *dst = CONVERT_TO_SHORTPTR(dst_buffer);
            uint16_t *rec = CONVERT_TO_SHORTPTR(pred_buf);
            src_pix = dst[pix_row * buf_stride + pix_col];
            rec_pix = rec[pix_row * block_size + pix_col];
          } else {
            src_pix = dst_buffer[pix_row * buf_stride + pix_col];
            rec_pix = pred_buf[pix_row * block_size + pix_col];
          }
#else
          src_pix = dst_buffer[pix_row * buf_stride + pix_col];
          rec_pix = pred_buf[pix_row * block_size + pix_col];
#endif
          src_mean += src_pix;
          rec_mean += rec_pix;
          dist_mean += src_pix - rec_pix;
          weber_stats->src_variance += src_pix * src_pix;
          weber_stats->rec_variance += rec_pix * rec_pix;
          weber_stats->src_pix_max = AOMMAX(weber_stats->src_pix_max, src_pix);
          weber_stats->rec_pix_max = AOMMAX(weber_stats->rec_pix_max, rec_pix);
          weber_stats->distortion += (src_pix - rec_pix) * (src_pix - rec_pix);
        }
      }

      weber_stats->src_variance -= (src_mean * src_mean) / pix_num;
      weber_stats->rec_variance -= (rec_mean * rec_mean) / pix_num;
      weber_stats->distortion -= (dist_mean * dist_mean) / pix_num;
      weber_stats->satd = best_intra_cost;

      double reg = sqrt((double)weber_stats->distortion) *
                   sqrt((double)weber_stats->src_pix_max) * 0.1;
      double alpha_den = fabs(weber_stats->rec_pix_max *
                                  sqrt((double)weber_stats->src_variance) -
                              weber_stats->src_pix_max *
                                  sqrt((double)weber_stats->rec_variance)) +
                         reg;
      double alpha_num = sqrt((double)weber_stats->distortion) *
                             sqrt((double)weber_stats->src_variance) *
                             weber_stats->rec_pix_max +
                         reg;

      weber_stats->alpha = AOMMAX(alpha_num, 1.0) / AOMMAX(alpha_den, 1.0);

      coeff[0] = 0;
      for (idx = 1; idx < coeff_count; ++idx) coeff[idx] = abs(coeff[idx]);

      qsort(coeff, coeff_count - 1, sizeof(*coeff), qsort_comp);

      // Noise level estimation
      median_val = coeff[coeff_count / 2];

      // Wiener filter
      for (idx = 1; idx < coeff_count; ++idx) {
        int64_t sqr_coeff = (int64_t)coeff[idx] * coeff[idx];
        int64_t tmp_coeff = (int64_t)coeff[idx];
        if (median_val) {
          tmp_coeff = (sqr_coeff * coeff[idx]) /
                      (sqr_coeff + (int64_t)median_val * median_val);
        }
        wiener_variance += tmp_coeff * tmp_coeff;
      }
      cpi->mb_weber_stats[mb_row * cpi->frame_info.mb_cols + mb_col]
          .mb_wiener_variance = wiener_variance / coeff_count;
      ++count;
    }
  }

  int sb_step = mi_size_wide[cm->seq_params->sb_size];
  double sb_wiener_log = 0;
  double sb_count = 0;

  for (int mi_row = 0; mi_row < cm->mi_params.mi_rows; mi_row += sb_step) {
    for (int mi_col = 0; mi_col < cm->mi_params.mi_cols; mi_col += sb_step) {
      int sb_wiener_var =
          get_var_perceptual_ai(cpi, cm->seq_params->sb_size, mi_row, mi_col);
      int64_t satd = get_satd(cpi, cm->seq_params->sb_size, mi_row, mi_col);
      int64_t sse = get_sse(cpi, cm->seq_params->sb_size, mi_row, mi_col);
      double scaled_satd = (double)satd / sqrt((double)sse);
      sb_wiener_log += scaled_satd * log(sb_wiener_var);
      sb_count += scaled_satd;
    }
  }

  if (sb_count > 0)
    cpi->norm_wiener_variance = (int64_t)(exp(sb_wiener_log / sb_count));
  cpi->norm_wiener_variance = AOMMAX(1, cpi->norm_wiener_variance);
}

int av1_get_sbq_perceptual_ai(AV1_COMP *const cpi, BLOCK_SIZE bsize, int mi_row,
                              int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int base_qindex = cm->quant_params.base_qindex;
  int sb_wiener_var = get_var_perceptual_ai(cpi, bsize, mi_row, mi_col);
  int offset = 0;
  double beta = (double)cpi->norm_wiener_variance / sb_wiener_var;
  // Cap beta such that the delta q value is not much far away from the base q.
  beta = AOMMIN(beta, 4);
  beta = AOMMAX(beta, 0.25);
  offset = av1_get_deltaq_offset(cm->seq_params->bit_depth, base_qindex, beta);
  const DeltaQInfo *const delta_q_info = &cm->delta_q_info;
  offset = AOMMIN(offset, delta_q_info->delta_q_res * 20 - 1);
  offset = AOMMAX(offset, -delta_q_info->delta_q_res * 20 + 1);
  int qindex = cm->quant_params.base_qindex + offset;
  qindex = AOMMIN(qindex, MAXQ);
  qindex = AOMMAX(qindex, MINQ);
  if (base_qindex > MINQ) qindex = AOMMAX(qindex, MINQ + 1);

  return qindex;
}
