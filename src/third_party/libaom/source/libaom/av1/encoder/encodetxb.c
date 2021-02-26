/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/encodetxb.h"

#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "av1/common/idct.h"
#include "av1/common/pred_common.h"
#include "av1/common/scan.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/hash.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/tokenize.h"

typedef struct LevelDownStats {
  int update;
  tran_low_t low_qc;
  tran_low_t low_dqc;
  int64_t dist0;
  int rate;
  int rate_low;
  int64_t dist;
  int64_t dist_low;
  int64_t rd;
  int64_t rd_low;
  int64_t nz_rd;
  int64_t rd_diff;
  int cost_diff;
  int64_t dist_diff;
  int new_eob;
} LevelDownStats;

static INLINE int get_dqv(const int16_t *dequant, int coeff_idx,
                          const qm_val_t *iqmatrix) {
  int dqv = dequant[!!coeff_idx];
  if (iqmatrix != NULL)
    dqv =
        ((iqmatrix[coeff_idx] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
  return dqv;
}

void av1_alloc_txb_buf(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  CoeffBufferPool *coeff_buf_pool = &cpi->coeff_buffer_pool;
  int size = ((cm->mi_params.mi_rows >> cm->seq_params.mib_size_log2) + 1) *
             ((cm->mi_params.mi_cols >> cm->seq_params.mib_size_log2) + 1);
  const int num_planes = av1_num_planes(cm);
  const int subsampling_x = cm->seq_params.subsampling_x;
  const int subsampling_y = cm->seq_params.subsampling_y;
  const int chroma_max_sb_square =
      MAX_SB_SQUARE >> (subsampling_x + subsampling_y);
  const int num_tcoeffs =
      size * (MAX_SB_SQUARE + (num_planes - 1) * chroma_max_sb_square);
  const int txb_unit_size = TX_SIZE_W_MIN * TX_SIZE_H_MIN;

  av1_free_txb_buf(cpi);
  // TODO(jingning): This should be further reduced.
  cpi->coeff_buffer_base = aom_malloc(sizeof(*cpi->coeff_buffer_base) * size);
  CHECK_MEM_ERROR(
      cm, coeff_buf_pool->tcoeff,
      aom_memalign(32, sizeof(*coeff_buf_pool->tcoeff) * num_tcoeffs));
  coeff_buf_pool->eobs =
      aom_malloc(sizeof(*coeff_buf_pool->eobs) * num_tcoeffs / txb_unit_size);
  coeff_buf_pool->entropy_ctx = aom_malloc(
      sizeof(*coeff_buf_pool->entropy_ctx) * num_tcoeffs / txb_unit_size);

  tran_low_t *tcoeff_ptr = coeff_buf_pool->tcoeff;
  uint16_t *eob_ptr = coeff_buf_pool->eobs;
  uint8_t *entropy_ctx_ptr = coeff_buf_pool->entropy_ctx;
  for (int i = 0; i < size; i++) {
    for (int plane = 0; plane < num_planes; plane++) {
      const int max_sb_square =
          (plane == AOM_PLANE_Y) ? MAX_SB_SQUARE : chroma_max_sb_square;
      cpi->coeff_buffer_base[i].tcoeff[plane] = tcoeff_ptr;
      cpi->coeff_buffer_base[i].eobs[plane] = eob_ptr;
      cpi->coeff_buffer_base[i].entropy_ctx[plane] = entropy_ctx_ptr;
      tcoeff_ptr += max_sb_square;
      eob_ptr += max_sb_square / txb_unit_size;
      entropy_ctx_ptr += max_sb_square / txb_unit_size;
    }
  }
}

void av1_free_txb_buf(AV1_COMP *cpi) {
  CoeffBufferPool *coeff_buf_pool = &cpi->coeff_buffer_pool;
  aom_free(cpi->coeff_buffer_base);
  aom_free(coeff_buf_pool->tcoeff);
  aom_free(coeff_buf_pool->eobs);
  aom_free(coeff_buf_pool->entropy_ctx);
}

static void write_golomb(aom_writer *w, int level) {
  int x = level + 1;
  int i = x;
  int length = 0;

  while (i) {
    i >>= 1;
    ++length;
  }
  assert(length > 0);

  for (i = 0; i < length - 1; ++i) aom_write_bit(w, 0);

  for (i = length - 1; i >= 0; --i) aom_write_bit(w, (x >> i) & 0x01);
}

static INLINE int64_t get_coeff_dist(tran_low_t tcoeff, tran_low_t dqcoeff,
                                     int shift) {
  const int64_t diff = (tcoeff - dqcoeff) * (1 << shift);
  const int64_t error = diff * diff;
  return error;
}

static const int8_t eob_to_pos_small[33] = {
  0, 1, 2,                                        // 0-2
  3, 3,                                           // 3-4
  4, 4, 4, 4,                                     // 5-8
  5, 5, 5, 5, 5, 5, 5, 5,                         // 9-16
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6  // 17-32
};

static const int8_t eob_to_pos_large[17] = {
  6,                               // place holder
  7,                               // 33-64
  8,  8,                           // 65-128
  9,  9,  9,  9,                   // 129-256
  10, 10, 10, 10, 10, 10, 10, 10,  // 257-512
  11                               // 513-
};

static INLINE int get_eob_pos_token(const int eob, int *const extra) {
  int t;

  if (eob < 33) {
    t = eob_to_pos_small[eob];
  } else {
    const int e = AOMMIN((eob - 1) >> 5, 16);
    t = eob_to_pos_large[e];
  }

  *extra = eob - av1_eob_group_start[t];

  return t;
}

#if CONFIG_ENTROPY_STATS
void av1_update_eob_context(int cdf_idx, int eob, TX_SIZE tx_size,
                            TX_CLASS tx_class, PLANE_TYPE plane,
                            FRAME_CONTEXT *ec_ctx, FRAME_COUNTS *counts,
                            uint8_t allow_update_cdf) {
#else
void av1_update_eob_context(int eob, TX_SIZE tx_size, TX_CLASS tx_class,
                            PLANE_TYPE plane, FRAME_CONTEXT *ec_ctx,
                            uint8_t allow_update_cdf) {
#endif
  int eob_extra;
  const int eob_pt = get_eob_pos_token(eob, &eob_extra);
  TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);

  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const int eob_multi_ctx = (tx_class == TX_CLASS_2D) ? 0 : 1;

  switch (eob_multi_size) {
    case 0:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi16[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf16[plane][eob_multi_ctx], eob_pt - 1, 5);
      break;
    case 1:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi32[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf32[plane][eob_multi_ctx], eob_pt - 1, 6);
      break;
    case 2:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi64[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf64[plane][eob_multi_ctx], eob_pt - 1, 7);
      break;
    case 3:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi128[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf128[plane][eob_multi_ctx], eob_pt - 1,
                   8);
      }
      break;
    case 4:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi256[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf256[plane][eob_multi_ctx], eob_pt - 1,
                   9);
      }
      break;
    case 5:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi512[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf512[plane][eob_multi_ctx], eob_pt - 1,
                   10);
      }
      break;
    case 6:
    default:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi1024[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf1024[plane][eob_multi_ctx], eob_pt - 1,
                   11);
      }
      break;
  }

  if (av1_eob_offset_bits[eob_pt] > 0) {
    int eob_ctx = eob_pt - 3;
    int eob_shift = av1_eob_offset_bits[eob_pt] - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
#if CONFIG_ENTROPY_STATS
    counts->eob_extra[cdf_idx][txs_ctx][plane][eob_pt][bit]++;
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf)
      update_cdf(ec_ctx->eob_extra_cdf[txs_ctx][plane][eob_ctx], bit, 2);
  }
}

static int get_eob_cost(int eob, const LV_MAP_EOB_COST *txb_eob_costs,
                        const LV_MAP_COEFF_COST *txb_costs, TX_CLASS tx_class) {
  int eob_extra;
  const int eob_pt = get_eob_pos_token(eob, &eob_extra);
  int eob_cost = 0;
  const int eob_multi_ctx = (tx_class == TX_CLASS_2D) ? 0 : 1;
  eob_cost = txb_eob_costs->eob_cost[eob_multi_ctx][eob_pt - 1];

  if (av1_eob_offset_bits[eob_pt] > 0) {
    const int eob_ctx = eob_pt - 3;
    const int eob_shift = av1_eob_offset_bits[eob_pt] - 1;
    const int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
    eob_cost += txb_costs->eob_extra_cost[eob_ctx][bit];
    const int offset_bits = av1_eob_offset_bits[eob_pt];
    if (offset_bits > 1) eob_cost += av1_cost_literal(offset_bits - 1);
  }
  return eob_cost;
}

static const int golomb_bits_cost[32] = {
  0,       512,     512 * 3, 512 * 3, 512 * 5, 512 * 5, 512 * 5, 512 * 5,
  512 * 7, 512 * 7, 512 * 7, 512 * 7, 512 * 7, 512 * 7, 512 * 7, 512 * 7,
  512 * 9, 512 * 9, 512 * 9, 512 * 9, 512 * 9, 512 * 9, 512 * 9, 512 * 9,
  512 * 9, 512 * 9, 512 * 9, 512 * 9, 512 * 9, 512 * 9, 512 * 9, 512 * 9
};
static const int golomb_cost_diff[32] = {
  0,       512, 512 * 2, 0, 512 * 2, 0, 0, 0, 512 * 2, 0, 0, 0, 0, 0, 0, 0,
  512 * 2, 0,   0,       0, 0,       0, 0, 0, 0,       0, 0, 0, 0, 0, 0, 0
};

static INLINE int get_golomb_cost(int abs_qc) {
  if (abs_qc >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    const int r = abs_qc - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
    const int length = get_msb(r) + 1;
    return av1_cost_literal(2 * length - 1);
  }
  return 0;
}

static INLINE int get_br_cost_with_diff(tran_low_t level, const int *coeff_lps,
                                        int *diff) {
  const int base_range = AOMMIN(level - 1 - NUM_BASE_LEVELS, COEFF_BASE_RANGE);
  int golomb_bits = 0;
  if (level <= COEFF_BASE_RANGE + 1 + NUM_BASE_LEVELS)
    *diff += coeff_lps[base_range + COEFF_BASE_RANGE + 1];

  if (level >= COEFF_BASE_RANGE + 1 + NUM_BASE_LEVELS) {
    int r = level - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
    if (r < 32) {
      golomb_bits = golomb_bits_cost[r];
      *diff += golomb_cost_diff[r];
    } else {
      golomb_bits = get_golomb_cost(level);
      *diff += (r & (r - 1)) == 0 ? 1024 : 0;
    }
  }

  return coeff_lps[base_range] + golomb_bits;
}

static INLINE int get_br_cost(tran_low_t level, const int *coeff_lps) {
  const int base_range = AOMMIN(level - 1 - NUM_BASE_LEVELS, COEFF_BASE_RANGE);
  return coeff_lps[base_range] + get_golomb_cost(level);
}

static INLINE int get_nz_map_ctx(const uint8_t *const levels,
                                 const int coeff_idx, const int bwl,
                                 const int height, const int scan_idx,
                                 const int is_eob, const TX_SIZE tx_size,
                                 const TX_CLASS tx_class) {
  if (is_eob) {
    if (scan_idx == 0) return 0;
    if (scan_idx <= (height << bwl) / 8) return 1;
    if (scan_idx <= (height << bwl) / 4) return 2;
    return 3;
  }
  const int stats =
      get_nz_mag(levels + get_padded_idx(coeff_idx, bwl), bwl, tx_class);
  return get_nz_map_ctx_from_stats(stats, coeff_idx, bwl, tx_size, tx_class);
}

void av1_txb_init_levels_c(const tran_low_t *const coeff, const int width,
                           const int height, uint8_t *const levels) {
  const int stride = width + TX_PAD_HOR;
  uint8_t *ls = levels;

  memset(levels + stride * height, 0,
         sizeof(*levels) * (TX_PAD_BOTTOM * stride + TX_PAD_END));

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      *ls++ = (uint8_t)clamp(abs(coeff[i * width + j]), 0, INT8_MAX);
    }
    for (int j = 0; j < TX_PAD_HOR; j++) {
      *ls++ = 0;
    }
  }
}

void av1_get_nz_map_contexts_c(const uint8_t *const levels,
                               const int16_t *const scan, const uint16_t eob,
                               const TX_SIZE tx_size, const TX_CLASS tx_class,
                               int8_t *const coeff_contexts) {
  const int bwl = get_txb_bwl(tx_size);
  const int height = get_txb_high(tx_size);
  for (int i = 0; i < eob; ++i) {
    const int pos = scan[i];
    coeff_contexts[pos] = get_nz_map_ctx(levels, pos, bwl, height, i,
                                         i == eob - 1, tx_size, tx_class);
  }
}

void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCK *const x,
                          aom_writer *w, int blk_row, int blk_col, int plane,
                          int block, TX_SIZE tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  const CB_COEFF_BUFFER *cb_coef_buff = x->cb_coef_buff;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const int txb_offset = x->mbmi_ext_frame->cb_offset[plane_type] /
                         (TX_SIZE_W_MIN * TX_SIZE_H_MIN);
  const uint16_t *eob_txb = cb_coef_buff->eobs[plane] + txb_offset;
  const uint16_t eob = eob_txb[block];
  const uint8_t *entropy_ctx = cb_coef_buff->entropy_ctx[plane] + txb_offset;
  const int txb_skip_ctx = entropy_ctx[block] & TXB_SKIP_CTX_MASK;
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  aom_write_symbol(w, eob == 0, ec_ctx->txb_skip_cdf[txs_ctx][txb_skip_ctx], 2);
  if (eob == 0) return;

  const TX_TYPE tx_type =
      av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                      cm->features.reduced_tx_set_used);
  // Only y plane's tx_type is transmitted
  if (plane == 0) {
    av1_write_tx_type(cm, xd, tx_type, tx_size, w);
  }

  int eob_extra;
  const int eob_pt = get_eob_pos_token(eob, &eob_extra);
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const int eob_multi_ctx = (tx_class == TX_CLASS_2D) ? 0 : 1;
  switch (eob_multi_size) {
    case 0:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf16[plane_type][eob_multi_ctx], 5);
      break;
    case 1:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf32[plane_type][eob_multi_ctx], 6);
      break;
    case 2:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf64[plane_type][eob_multi_ctx], 7);
      break;
    case 3:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf128[plane_type][eob_multi_ctx], 8);
      break;
    case 4:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf256[plane_type][eob_multi_ctx], 9);
      break;
    case 5:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf512[plane_type][eob_multi_ctx], 10);
      break;
    default:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf1024[plane_type][eob_multi_ctx], 11);
      break;
  }

  const int eob_offset_bits = av1_eob_offset_bits[eob_pt];
  if (eob_offset_bits > 0) {
    const int eob_ctx = eob_pt - 3;
    int eob_shift = eob_offset_bits - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
    aom_write_symbol(w, bit,
                     ec_ctx->eob_extra_cdf[txs_ctx][plane_type][eob_ctx], 2);
    for (int i = 1; i < eob_offset_bits; i++) {
      eob_shift = eob_offset_bits - 1 - i;
      bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
      aom_write_bit(w, bit);
    }
  }

  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);
  const tran_low_t *tcoeff_txb =
      cb_coef_buff->tcoeff[plane] + x->mbmi_ext_frame->cb_offset[plane_type];
  const tran_low_t *tcoeff = tcoeff_txb + BLOCK_OFFSET(block);
  av1_txb_init_levels(tcoeff, width, height, levels);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const int bwl = get_txb_bwl(tx_size);
  for (int c = eob - 1; c >= 0; --c) {
    const int pos = scan[c];
    const int coeff_ctx = coeff_contexts[pos];
    const tran_low_t v = tcoeff[pos];
    const tran_low_t level = abs(v);

    if (c == eob - 1) {
      aom_write_symbol(
          w, AOMMIN(level, 3) - 1,
          ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][coeff_ctx], 3);
    } else {
      aom_write_symbol(w, AOMMIN(level, 3),
                       ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx],
                       4);
    }
    if (level > NUM_BASE_LEVELS) {
      // level is above 1.
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      const int br_ctx = get_br_ctx(levels, pos, bwl, tx_class);
      aom_cdf_prob *cdf =
          ec_ctx->coeff_br_cdf[AOMMIN(txs_ctx, TX_32X32)][plane_type][br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        aom_write_symbol(w, k, cdf, BR_CDF_SIZE);
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
  }

  // Loop to code all signs in the transform block,
  // starting with the sign of DC (if applicable)
  for (int c = 0; c < eob; ++c) {
    const tran_low_t v = tcoeff[scan[c]];
    const tran_low_t level = abs(v);
    const int sign = (v < 0) ? 1 : 0;
    if (level) {
      if (c == 0) {
        const int dc_sign_ctx =
            (entropy_ctx[block] >> DC_SIGN_CTX_SHIFT) & DC_SIGN_CTX_MASK;
        aom_write_symbol(w, sign, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx],
                         2);
      } else {
        aom_write_bit(w, sign);
      }
      if (level > COEFF_BASE_RANGE + NUM_BASE_LEVELS)
        write_golomb(w, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
    }
  }
}

typedef struct encode_txb_args {
  const AV1_COMMON *cm;
  MACROBLOCK *x;
  aom_writer *w;
} ENCODE_TXB_ARGS;

void av1_write_intra_coeffs_mb(const AV1_COMMON *const cm, MACROBLOCK *x,
                               aom_writer *w, BLOCK_SIZE bsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int num_planes = av1_num_planes(cm);
  int block[MAX_MB_PLANE] = { 0 };
  int row, col;
  assert(bsize == get_plane_block_size(bsize, xd->plane[0].subsampling_x,
                                       xd->plane[0].subsampling_y));
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const BLOCK_SIZE max_unit_bsize = BLOCK_64X64;
  int mu_blocks_wide = mi_size_wide[max_unit_bsize];
  int mu_blocks_high = mi_size_high[max_unit_bsize];
  mu_blocks_wide = AOMMIN(max_blocks_wide, mu_blocks_wide);
  mu_blocks_high = AOMMIN(max_blocks_high, mu_blocks_high);

  for (row = 0; row < max_blocks_high; row += mu_blocks_high) {
    for (col = 0; col < max_blocks_wide; col += mu_blocks_wide) {
      for (int plane = 0; plane < num_planes; ++plane) {
        if (plane && !xd->is_chroma_ref) break;
        const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
        const int stepr = tx_size_high_unit[tx_size];
        const int stepc = tx_size_wide_unit[tx_size];
        const int step = stepr * stepc;
        const struct macroblockd_plane *const pd = &xd->plane[plane];
        const int unit_height = ROUND_POWER_OF_TWO(
            AOMMIN(mu_blocks_high + row, max_blocks_high), pd->subsampling_y);
        const int unit_width = ROUND_POWER_OF_TWO(
            AOMMIN(mu_blocks_wide + col, max_blocks_wide), pd->subsampling_x);
        for (int blk_row = row >> pd->subsampling_y; blk_row < unit_height;
             blk_row += stepr) {
          for (int blk_col = col >> pd->subsampling_x; blk_col < unit_width;
               blk_col += stepc) {
            av1_write_coeffs_txb(cm, x, w, blk_row, blk_col, plane,
                                 block[plane], tx_size);
            block[plane] += step;
          }
        }
      }
    }
  }
}

// TODO(angiebird): use this function whenever it's possible
static int get_tx_type_cost(const MACROBLOCK *x, const MACROBLOCKD *xd,
                            int plane, TX_SIZE tx_size, TX_TYPE tx_type,
                            int reduced_tx_set_used) {
  if (plane > 0) return 0;

  const TX_SIZE square_tx_size = txsize_sqr_map[tx_size];

  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  if (get_ext_tx_types(tx_size, is_inter, reduced_tx_set_used) > 1 &&
      !xd->lossless[xd->mi[0]->segment_id]) {
    const int ext_tx_set =
        get_ext_tx_set(tx_size, is_inter, reduced_tx_set_used);
    if (is_inter) {
      if (ext_tx_set > 0)
        return x->mode_costs
            .inter_tx_type_costs[ext_tx_set][square_tx_size][tx_type];
    } else {
      if (ext_tx_set > 0) {
        PREDICTION_MODE intra_dir;
        if (mbmi->filter_intra_mode_info.use_filter_intra)
          intra_dir = fimode_to_intradir[mbmi->filter_intra_mode_info
                                             .filter_intra_mode];
        else
          intra_dir = mbmi->mode;
        return x->mode_costs.intra_tx_type_costs[ext_tx_set][square_tx_size]
                                                [intra_dir][tx_type];
      }
    }
  }
  return 0;
}

static INLINE void update_coeff_eob_fast(int *eob, int shift,
                                         const int16_t *dequant_ptr,
                                         const int16_t *scan,
                                         const tran_low_t *coeff_ptr,
                                         tran_low_t *qcoeff_ptr,
                                         tran_low_t *dqcoeff_ptr) {
  // TODO(sarahparker) make this work for aomqm
  int eob_out = *eob;
  int zbin[2] = { dequant_ptr[0] + ROUND_POWER_OF_TWO(dequant_ptr[0] * 70, 7),
                  dequant_ptr[1] + ROUND_POWER_OF_TWO(dequant_ptr[1] * 70, 7) };

  for (int i = *eob - 1; i >= 0; i--) {
    const int rc = scan[i];
    const int qcoeff = qcoeff_ptr[rc];
    const int coeff = coeff_ptr[rc];
    const int coeff_sign = AOMSIGN(coeff);
    int64_t abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

    if (((abs_coeff << (1 + shift)) < zbin[rc != 0]) || (qcoeff == 0)) {
      eob_out--;
      qcoeff_ptr[rc] = 0;
      dqcoeff_ptr[rc] = 0;
    } else {
      break;
    }
  }

  *eob = eob_out;
}

static AOM_FORCE_INLINE int warehouse_efficients_txb(
    const MACROBLOCK *x, const int plane, const int block,
    const TX_SIZE tx_size, const TXB_CTX *const txb_ctx,
    const struct macroblock_plane *p, const int eob,
    const PLANE_TYPE plane_type, const LV_MAP_COEFF_COST *const coeff_costs,
    const MACROBLOCKD *const xd, const TX_TYPE tx_type, const TX_CLASS tx_class,
    int reduced_tx_set_used) {
  const tran_low_t *const qcoeff = p->qcoeff + BLOCK_OFFSET(block);
  const int txb_skip_ctx = txb_ctx->txb_skip_ctx;
  const int bwl = get_txb_bwl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *const eob_costs =
      &x->coeff_costs.eob_costs[eob_multi_size][plane_type];
  int cost = coeff_costs->txb_skip_cost[txb_skip_ctx][0];

  av1_txb_init_levels(qcoeff, width, height, levels);

  cost += get_tx_type_cost(x, xd, plane, tx_size, tx_type, reduced_tx_set_used);

  cost += get_eob_cost(eob, eob_costs, coeff_costs, tx_class);

  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const int(*lps_cost)[COEFF_BASE_RANGE + 1 + COEFF_BASE_RANGE + 1] =
      coeff_costs->lps_cost;
  int c = eob - 1;
  {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];
    const int sign = AOMSIGN(v);
    const int level = (v ^ sign) - sign;
    const int coeff_ctx = coeff_contexts[pos];
    cost += coeff_costs->base_eob_cost[coeff_ctx][AOMMIN(level, 3) - 1];

    if (v) {
      // sign bit cost
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx_eob(pos, bwl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx]);
      }
      if (c) {
        cost += av1_cost_literal(1);
      } else {
        const int sign01 = (sign ^ sign) - sign;
        const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
        cost += coeff_costs->dc_sign_cost[dc_sign_ctx][sign01];
        return cost;
      }
    }
  }
  const int(*base_cost)[8] = coeff_costs->base_cost;
  for (c = eob - 2; c >= 1; --c) {
    const int pos = scan[c];
    const int coeff_ctx = coeff_contexts[pos];
    const tran_low_t v = qcoeff[pos];
    const int level = abs(v);
    cost += base_cost[coeff_ctx][AOMMIN(level, 3)];
    if (v) {
      // sign bit cost
      cost += av1_cost_literal(1);
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx(levels, pos, bwl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx]);
      }
    }
  }
  // c == 0 after previous loop
  {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];
    const int coeff_ctx = coeff_contexts[pos];
    const int sign = AOMSIGN(v);
    const int level = (v ^ sign) - sign;
    cost += base_cost[coeff_ctx][AOMMIN(level, 3)];

    if (v) {
      // sign bit cost
      const int sign01 = (sign ^ sign) - sign;
      const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
      cost += coeff_costs->dc_sign_cost[dc_sign_ctx][sign01];
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx(levels, pos, bwl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx]);
      }
    }
  }
  return cost;
}

static AOM_FORCE_INLINE int warehouse_efficients_txb_laplacian(
    const MACROBLOCK *x, const int plane, const int block,
    const TX_SIZE tx_size, const TXB_CTX *const txb_ctx, const int eob,
    const PLANE_TYPE plane_type, const LV_MAP_COEFF_COST *const coeff_costs,
    const MACROBLOCKD *const xd, const TX_TYPE tx_type, const TX_CLASS tx_class,
    int reduced_tx_set_used) {
  const int txb_skip_ctx = txb_ctx->txb_skip_ctx;

  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *const eob_costs =
      &x->coeff_costs.eob_costs[eob_multi_size][plane_type];
  int cost = coeff_costs->txb_skip_cost[txb_skip_ctx][0];

  cost += get_tx_type_cost(x, xd, plane, tx_size, tx_type, reduced_tx_set_used);

  cost += get_eob_cost(eob, eob_costs, coeff_costs, tx_class);

  cost += av1_cost_coeffs_txb_estimate(x, plane, block, tx_size, tx_type);
  return cost;
}

// Look up table of individual cost of coefficient by its quantization level.
// determined based on Laplacian distribution conditioned on estimated context
static const int costLUT[15] = { -1143, 53,   545,  825,  1031,
                                 1209,  1393, 1577, 1763, 1947,
                                 2132,  2317, 2501, 2686, 2871 };
static const int const_term = (1 << AV1_PROB_COST_SHIFT);
static const int loge_par = ((14427 << AV1_PROB_COST_SHIFT) + 5000) / 10000;
int av1_cost_coeffs_txb_estimate(const MACROBLOCK *x, const int plane,
                                 const int block, const TX_SIZE tx_size,
                                 const TX_TYPE tx_type) {
  assert(plane == 0);

  int cost = 0;
  const struct macroblock_plane *p = &x->plane[plane];
  const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
  const int16_t *scan = scan_order->scan;
  tran_low_t *qcoeff = p->qcoeff + BLOCK_OFFSET(block);

  int eob = p->eobs[block];

  // coeffs
  int c = eob - 1;
  // eob
  {
    const int pos = scan[c];
    const tran_low_t v = abs(qcoeff[pos]) - 1;
    cost += (v << (AV1_PROB_COST_SHIFT + 2));
  }
  // other coeffs
  for (c = eob - 2; c >= 0; c--) {
    const int pos = scan[c];
    const tran_low_t v = abs(qcoeff[pos]);
    const int idx = AOMMIN(v, 14);

    cost += costLUT[idx];
  }

  // const_term does not contain DC, and log(e) does not contain eob, so both
  // (eob-1)
  cost += (const_term + loge_par) * (eob - 1);

  return cost;
}

int av1_cost_coeffs_txb(const MACROBLOCK *x, const int plane, const int block,
                        const TX_SIZE tx_size, const TX_TYPE tx_type,
                        const TXB_CTX *const txb_ctx, int reduced_tx_set_used) {
  const struct macroblock_plane *p = &x->plane[plane];
  const int eob = p->eobs[block];
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const LV_MAP_COEFF_COST *const coeff_costs =
      &x->coeff_costs.coeff_costs[txs_ctx][plane_type];
  if (eob == 0) {
    return coeff_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
  }

  const MACROBLOCKD *const xd = &x->e_mbd;
  const TX_CLASS tx_class = tx_type_to_class[tx_type];

  return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                  plane_type, coeff_costs, xd, tx_type,
                                  tx_class, reduced_tx_set_used);
}

int av1_cost_coeffs_txb_laplacian(const MACROBLOCK *x, const int plane,
                                  const int block, const TX_SIZE tx_size,
                                  const TX_TYPE tx_type,
                                  const TXB_CTX *const txb_ctx,
                                  const int reduced_tx_set_used,
                                  const int adjust_eob) {
  const struct macroblock_plane *p = &x->plane[plane];
  int eob = p->eobs[block];

  if (adjust_eob) {
    const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
    const int16_t *scan = scan_order->scan;
    tran_low_t *tcoeff = p->coeff + BLOCK_OFFSET(block);
    tran_low_t *qcoeff = p->qcoeff + BLOCK_OFFSET(block);
    tran_low_t *dqcoeff = p->dqcoeff + BLOCK_OFFSET(block);
    update_coeff_eob_fast(&eob, av1_get_tx_scale(tx_size), p->dequant_QTX, scan,
                          tcoeff, qcoeff, dqcoeff);
    p->eobs[block] = eob;
  }

  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const LV_MAP_COEFF_COST *const coeff_costs =
      &x->coeff_costs.coeff_costs[txs_ctx][plane_type];
  if (eob == 0) {
    return coeff_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
  }

  const MACROBLOCKD *const xd = &x->e_mbd;
  const TX_CLASS tx_class = tx_type_to_class[tx_type];

  return warehouse_efficients_txb_laplacian(
      x, plane, block, tx_size, txb_ctx, eob, plane_type, coeff_costs, xd,
      tx_type, tx_class, reduced_tx_set_used);
}

static AOM_FORCE_INLINE int get_two_coeff_cost_simple(
    int ci, tran_low_t abs_qc, int coeff_ctx,
    const LV_MAP_COEFF_COST *txb_costs, int bwl, TX_CLASS tx_class,
    const uint8_t *levels, int *cost_low) {
  // this simple version assumes the coeff's scan_idx is not DC (scan_idx != 0)
  // and not the last (scan_idx != eob - 1)
  assert(ci > 0);
  int cost = txb_costs->base_cost[coeff_ctx][AOMMIN(abs_qc, 3)];
  int diff = 0;
  if (abs_qc <= 3) diff = txb_costs->base_cost[coeff_ctx][abs_qc + 4];
  if (abs_qc) {
    cost += av1_cost_literal(1);
    if (abs_qc > NUM_BASE_LEVELS) {
      const int br_ctx = get_br_ctx(levels, ci, bwl, tx_class);
      int brcost_diff = 0;
      cost += get_br_cost_with_diff(abs_qc, txb_costs->lps_cost[br_ctx],
                                    &brcost_diff);
      diff += brcost_diff;
    }
  }
  *cost_low = cost - diff;

  return cost;
}

static INLINE int get_coeff_cost_eob(int ci, tran_low_t abs_qc, int sign,
                                     int coeff_ctx, int dc_sign_ctx,
                                     const LV_MAP_COEFF_COST *txb_costs,
                                     int bwl, TX_CLASS tx_class) {
  int cost = 0;
  cost += txb_costs->base_eob_cost[coeff_ctx][AOMMIN(abs_qc, 3) - 1];
  if (abs_qc != 0) {
    if (ci == 0) {
      cost += txb_costs->dc_sign_cost[dc_sign_ctx][sign];
    } else {
      cost += av1_cost_literal(1);
    }
    if (abs_qc > NUM_BASE_LEVELS) {
      int br_ctx;
      br_ctx = get_br_ctx_eob(ci, bwl, tx_class);
      cost += get_br_cost(abs_qc, txb_costs->lps_cost[br_ctx]);
    }
  }
  return cost;
}

static INLINE int get_coeff_cost_general(int is_last, int ci, tran_low_t abs_qc,
                                         int sign, int coeff_ctx,
                                         int dc_sign_ctx,
                                         const LV_MAP_COEFF_COST *txb_costs,
                                         int bwl, TX_CLASS tx_class,
                                         const uint8_t *levels) {
  int cost = 0;
  if (is_last) {
    cost += txb_costs->base_eob_cost[coeff_ctx][AOMMIN(abs_qc, 3) - 1];
  } else {
    cost += txb_costs->base_cost[coeff_ctx][AOMMIN(abs_qc, 3)];
  }
  if (abs_qc != 0) {
    if (ci == 0) {
      cost += txb_costs->dc_sign_cost[dc_sign_ctx][sign];
    } else {
      cost += av1_cost_literal(1);
    }
    if (abs_qc > NUM_BASE_LEVELS) {
      int br_ctx;
      if (is_last)
        br_ctx = get_br_ctx_eob(ci, bwl, tx_class);
      else
        br_ctx = get_br_ctx(levels, ci, bwl, tx_class);
      cost += get_br_cost(abs_qc, txb_costs->lps_cost[br_ctx]);
    }
  }
  return cost;
}

static INLINE void get_qc_dqc_low(tran_low_t abs_qc, int sign, int dqv,
                                  int shift, tran_low_t *qc_low,
                                  tran_low_t *dqc_low) {
  tran_low_t abs_qc_low = abs_qc - 1;
  *qc_low = (-sign ^ abs_qc_low) + sign;
  assert((sign ? -abs_qc_low : abs_qc_low) == *qc_low);
  tran_low_t abs_dqc_low = (abs_qc_low * dqv) >> shift;
  *dqc_low = (-sign ^ abs_dqc_low) + sign;
  assert((sign ? -abs_dqc_low : abs_dqc_low) == *dqc_low);
}

static INLINE void update_coeff_general(
    int *accu_rate, int64_t *accu_dist, int si, int eob, TX_SIZE tx_size,
    TX_CLASS tx_class, int bwl, int height, int64_t rdmult, int shift,
    int dc_sign_ctx, const int16_t *dequant, const int16_t *scan,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels,
    const qm_val_t *iqmatrix) {
  const int dqv = get_dqv(dequant, scan[si], iqmatrix);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int is_last = si == (eob - 1);
  const int coeff_ctx = get_lower_levels_ctx_general(
      is_last, si, bwl, height, levels, ci, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += txb_costs->base_cost[coeff_ctx][0];
  } else {
    const int sign = (qc < 0) ? 1 : 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int64_t dist = get_coeff_dist(tqc, dqc, shift);
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift);
    const int rate =
        get_coeff_cost_general(is_last, ci, abs_qc, sign, coeff_ctx,
                               dc_sign_ctx, txb_costs, bwl, tx_class, levels);
    const int64_t rd = RDCOST(rdmult, rate, dist);

    tran_low_t qc_low, dqc_low;
    tran_low_t abs_qc_low;
    int64_t dist_low, rd_low;
    int rate_low;
    if (abs_qc == 1) {
      abs_qc_low = qc_low = dqc_low = 0;
      dist_low = dist0;
      rate_low = txb_costs->base_cost[coeff_ctx][0];
    } else {
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      abs_qc_low = abs_qc - 1;
      dist_low = get_coeff_dist(tqc, dqc_low, shift);
      rate_low =
          get_coeff_cost_general(is_last, ci, abs_qc_low, sign, coeff_ctx,
                                 dc_sign_ctx, txb_costs, bwl, tx_class, levels);
    }

    rd_low = RDCOST(rdmult, rate_low, dist_low);
    if (rd_low < rd) {
      qcoeff[ci] = qc_low;
      dqcoeff[ci] = dqc_low;
      levels[get_padded_idx(ci, bwl)] = AOMMIN(abs_qc_low, INT8_MAX);
      *accu_rate += rate_low;
      *accu_dist += dist_low - dist0;
    } else {
      *accu_rate += rate;
      *accu_dist += dist - dist0;
    }
  }
}

static AOM_FORCE_INLINE void update_coeff_simple(
    int *accu_rate, int si, int eob, TX_SIZE tx_size, TX_CLASS tx_class,
    int bwl, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    uint8_t *levels, const qm_val_t *iqmatrix) {
  const int dqv = get_dqv(dequant, scan[si], iqmatrix);
  (void)eob;
  // this simple version assumes the coeff's scan_idx is not DC (scan_idx != 0)
  // and not the last (scan_idx != eob - 1)
  assert(si != eob - 1);
  assert(si > 0);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int coeff_ctx =
      get_lower_levels_ctx(levels, ci, bwl, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += txb_costs->base_cost[coeff_ctx][0];
  } else {
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t abs_tqc = abs(tcoeff[ci]);
    const tran_low_t abs_dqc = abs(dqcoeff[ci]);
    int rate_low = 0;
    const int rate = get_two_coeff_cost_simple(
        ci, abs_qc, coeff_ctx, txb_costs, bwl, tx_class, levels, &rate_low);
    if (abs_dqc < abs_tqc) {
      *accu_rate += rate;
      return;
    }

    const int64_t dist = get_coeff_dist(abs_tqc, abs_dqc, shift);
    const int64_t rd = RDCOST(rdmult, rate, dist);

    const tran_low_t abs_qc_low = abs_qc - 1;
    const tran_low_t abs_dqc_low = (abs_qc_low * dqv) >> shift;
    const int64_t dist_low = get_coeff_dist(abs_tqc, abs_dqc_low, shift);
    const int64_t rd_low = RDCOST(rdmult, rate_low, dist_low);

    if (rd_low < rd) {
      const int sign = (qc < 0) ? 1 : 0;
      qcoeff[ci] = (-sign ^ abs_qc_low) + sign;
      dqcoeff[ci] = (-sign ^ abs_dqc_low) + sign;
      levels[get_padded_idx(ci, bwl)] = AOMMIN(abs_qc_low, INT8_MAX);
      *accu_rate += rate_low;
    } else {
      *accu_rate += rate;
    }
  }
}

static AOM_FORCE_INLINE void update_coeff_eob(
    int *accu_rate, int64_t *accu_dist, int *eob, int *nz_num, int *nz_ci,
    int si, TX_SIZE tx_size, TX_CLASS tx_class, int bwl, int height,
    int dc_sign_ctx, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_EOB_COST *txb_eob_costs,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels, int sharpness,
    const qm_val_t *iqmatrix) {
  const int dqv = get_dqv(dequant, scan[si], iqmatrix);
  assert(si != *eob - 1);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int coeff_ctx =
      get_lower_levels_ctx(levels, ci, bwl, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += txb_costs->base_cost[coeff_ctx][0];
  } else {
    int lower_level = 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int sign = (qc < 0) ? 1 : 0;
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift);
    int64_t dist = get_coeff_dist(tqc, dqc, shift) - dist0;
    int rate =
        get_coeff_cost_general(0, ci, abs_qc, sign, coeff_ctx, dc_sign_ctx,
                               txb_costs, bwl, tx_class, levels);
    int64_t rd = RDCOST(rdmult, *accu_rate + rate, *accu_dist + dist);

    tran_low_t qc_low, dqc_low;
    tran_low_t abs_qc_low;
    int64_t dist_low, rd_low;
    int rate_low;
    if (abs_qc == 1) {
      abs_qc_low = 0;
      dqc_low = qc_low = 0;
      dist_low = 0;
      rate_low = txb_costs->base_cost[coeff_ctx][0];
      rd_low = RDCOST(rdmult, *accu_rate + rate_low, *accu_dist);
    } else {
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      abs_qc_low = abs_qc - 1;
      dist_low = get_coeff_dist(tqc, dqc_low, shift) - dist0;
      rate_low =
          get_coeff_cost_general(0, ci, abs_qc_low, sign, coeff_ctx,
                                 dc_sign_ctx, txb_costs, bwl, tx_class, levels);
      rd_low = RDCOST(rdmult, *accu_rate + rate_low, *accu_dist + dist_low);
    }

    int lower_level_new_eob = 0;
    const int new_eob = si + 1;
    const int coeff_ctx_new_eob = get_lower_levels_ctx_eob(bwl, height, si);
    const int new_eob_cost =
        get_eob_cost(new_eob, txb_eob_costs, txb_costs, tx_class);
    int rate_coeff_eob =
        new_eob_cost + get_coeff_cost_eob(ci, abs_qc, sign, coeff_ctx_new_eob,
                                          dc_sign_ctx, txb_costs, bwl,
                                          tx_class);
    int64_t dist_new_eob = dist;
    int64_t rd_new_eob = RDCOST(rdmult, rate_coeff_eob, dist_new_eob);

    if (abs_qc_low > 0) {
      const int rate_coeff_eob_low =
          new_eob_cost + get_coeff_cost_eob(ci, abs_qc_low, sign,
                                            coeff_ctx_new_eob, dc_sign_ctx,
                                            txb_costs, bwl, tx_class);
      const int64_t dist_new_eob_low = dist_low;
      const int64_t rd_new_eob_low =
          RDCOST(rdmult, rate_coeff_eob_low, dist_new_eob_low);
      if (rd_new_eob_low < rd_new_eob) {
        lower_level_new_eob = 1;
        rd_new_eob = rd_new_eob_low;
        rate_coeff_eob = rate_coeff_eob_low;
        dist_new_eob = dist_new_eob_low;
      }
    }

    if (rd_low < rd) {
      lower_level = 1;
      rd = rd_low;
      rate = rate_low;
      dist = dist_low;
    }

    if (sharpness == 0 && rd_new_eob < rd) {
      for (int ni = 0; ni < *nz_num; ++ni) {
        int last_ci = nz_ci[ni];
        levels[get_padded_idx(last_ci, bwl)] = 0;
        qcoeff[last_ci] = 0;
        dqcoeff[last_ci] = 0;
      }
      *eob = new_eob;
      *nz_num = 0;
      *accu_rate = rate_coeff_eob;
      *accu_dist = dist_new_eob;
      lower_level = lower_level_new_eob;
    } else {
      *accu_rate += rate;
      *accu_dist += dist;
    }

    if (lower_level) {
      qcoeff[ci] = qc_low;
      dqcoeff[ci] = dqc_low;
      levels[get_padded_idx(ci, bwl)] = AOMMIN(abs_qc_low, INT8_MAX);
    }
    if (qcoeff[ci]) {
      nz_ci[*nz_num] = ci;
      ++*nz_num;
    }
  }
}

static INLINE void update_skip(int *accu_rate, int64_t accu_dist, int *eob,
                               int nz_num, int *nz_ci, int64_t rdmult,
                               int skip_cost, int non_skip_cost,
                               tran_low_t *qcoeff, tran_low_t *dqcoeff,
                               int sharpness) {
  const int64_t rd = RDCOST(rdmult, *accu_rate + non_skip_cost, accu_dist);
  const int64_t rd_new_eob = RDCOST(rdmult, skip_cost, 0);
  if (sharpness == 0 && rd_new_eob < rd) {
    for (int i = 0; i < nz_num; ++i) {
      const int ci = nz_ci[i];
      qcoeff[ci] = 0;
      dqcoeff[ci] = 0;
      // no need to set up levels because this is the last step
      // levels[get_padded_idx(ci, bwl)] = 0;
    }
    *accu_rate = 0;
    *eob = 0;
  }
}

int av1_optimize_txb_new(const struct AV1_COMP *cpi, MACROBLOCK *x, int plane,
                         int block, TX_SIZE tx_size, TX_TYPE tx_type,
                         const TXB_CTX *const txb_ctx, int *rate_cost,
                         int sharpness) {
  MACROBLOCKD *xd = &x->e_mbd;
  const struct macroblock_plane *p = &x->plane[plane];
  const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
  const int16_t *scan = scan_order->scan;
  const int shift = av1_get_tx_scale(tx_size);
  int eob = p->eobs[block];
  const int16_t *dequant = p->dequant_QTX;
  const qm_val_t *iqmatrix =
      av1_get_iqmatrix(&cpi->common.quant_params, xd, plane, tx_size, tx_type);
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  tran_low_t *dqcoeff = p->dqcoeff + block_offset;
  const tran_low_t *tcoeff = p->coeff + block_offset;
  const CoeffCosts *coeff_costs = &x->coeff_costs;

  // This function is not called if eob = 0.
  assert(eob > 0);

  const AV1_COMMON *cm = &cpi->common;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int bwl = get_txb_bwl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  assert(width == (1 << bwl));
  const int is_inter = is_inter_block(mbmi);
  const LV_MAP_COEFF_COST *txb_costs =
      &coeff_costs->coeff_costs[txs_ctx][plane_type];
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *txb_eob_costs =
      &coeff_costs->eob_costs[eob_multi_size][plane_type];

  const int rshift =
      (sharpness +
       (cpi->oxcf.q_cfg.aq_mode == VARIANCE_AQ && mbmi->segment_id < 4
            ? 7 - mbmi->segment_id
            : 2) +
       (cpi->oxcf.q_cfg.aq_mode != VARIANCE_AQ &&
                cpi->oxcf.q_cfg.deltaq_mode == DELTA_Q_PERCEPTUAL &&
                cm->delta_q_info.delta_q_present_flag && x->sb_energy_level < 0
            ? (3 - x->sb_energy_level)
            : 0));
  const int64_t rdmult =
      (((int64_t)x->rdmult *
        (plane_rd_mult[is_inter][plane_type] << (2 * (xd->bd - 8)))) +
       2) >>
      rshift;

  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);

  if (eob > 1) av1_txb_init_levels(qcoeff, width, height, levels);

  // TODO(angirbird): check iqmatrix

  const int non_skip_cost = txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][0];
  const int skip_cost = txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
  const int eob_cost = get_eob_cost(eob, txb_eob_costs, txb_costs, tx_class);
  int accu_rate = eob_cost;
  int64_t accu_dist = 0;
  int si = eob - 1;
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const tran_low_t abs_qc = abs(qc);
  const int sign = qc < 0;
  const int max_nz_num = 2;
  int nz_num = 1;
  int nz_ci[3] = { ci, 0, 0 };
  if (abs_qc >= 2) {
    update_coeff_general(&accu_rate, &accu_dist, si, eob, tx_size, tx_class,
                         bwl, height, rdmult, shift, txb_ctx->dc_sign_ctx,
                         dequant, scan, txb_costs, tcoeff, qcoeff, dqcoeff,
                         levels, iqmatrix);
    --si;
  } else {
    assert(abs_qc == 1);
    const int coeff_ctx = get_lower_levels_ctx_eob(bwl, height, si);
    accu_rate +=
        get_coeff_cost_eob(ci, abs_qc, sign, coeff_ctx, txb_ctx->dc_sign_ctx,
                           txb_costs, bwl, tx_class);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int64_t dist = get_coeff_dist(tqc, dqc, shift);
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift);
    accu_dist += dist - dist0;
    --si;
  }

#define UPDATE_COEFF_EOB_CASE(tx_class_literal)                            \
  case tx_class_literal:                                                   \
    for (; si >= 0 && nz_num <= max_nz_num; --si) {                        \
      update_coeff_eob(&accu_rate, &accu_dist, &eob, &nz_num, nz_ci, si,   \
                       tx_size, tx_class_literal, bwl, height,             \
                       txb_ctx->dc_sign_ctx, rdmult, shift, dequant, scan, \
                       txb_eob_costs, txb_costs, tcoeff, qcoeff, dqcoeff,  \
                       levels, sharpness, iqmatrix);                       \
    }                                                                      \
    break;
  switch (tx_class) {
    UPDATE_COEFF_EOB_CASE(TX_CLASS_2D);
    UPDATE_COEFF_EOB_CASE(TX_CLASS_HORIZ);
    UPDATE_COEFF_EOB_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_EOB_CASE
    default: assert(false);
  }

  if (si == -1 && nz_num <= max_nz_num) {
    update_skip(&accu_rate, accu_dist, &eob, nz_num, nz_ci, rdmult, skip_cost,
                non_skip_cost, qcoeff, dqcoeff, sharpness);
  }

#define UPDATE_COEFF_SIMPLE_CASE(tx_class_literal)                             \
  case tx_class_literal:                                                       \
    for (; si >= 1; --si) {                                                    \
      update_coeff_simple(&accu_rate, si, eob, tx_size, tx_class_literal, bwl, \
                          rdmult, shift, dequant, scan, txb_costs, tcoeff,     \
                          qcoeff, dqcoeff, levels, iqmatrix);                  \
    }                                                                          \
    break;
  switch (tx_class) {
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_2D);
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_HORIZ);
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_SIMPLE_CASE
    default: assert(false);
  }

  // DC position
  if (si == 0) {
    // no need to update accu_dist because it's not used after this point
    int64_t dummy_dist = 0;
    update_coeff_general(&accu_rate, &dummy_dist, si, eob, tx_size, tx_class,
                         bwl, height, rdmult, shift, txb_ctx->dc_sign_ctx,
                         dequant, scan, txb_costs, tcoeff, qcoeff, dqcoeff,
                         levels, iqmatrix);
  }

  const int tx_type_cost = get_tx_type_cost(x, xd, plane, tx_size, tx_type,
                                            cm->features.reduced_tx_set_used);
  if (eob == 0)
    accu_rate += skip_cost;
  else
    accu_rate += non_skip_cost + tx_type_cost;

  p->eobs[block] = eob;
  p->txb_entropy_ctx[block] =
      av1_get_txb_entropy_context(qcoeff, scan_order, p->eobs[block]);

  *rate_cost = accu_rate;
  return eob;
}

uint8_t av1_get_txb_entropy_context(const tran_low_t *qcoeff,
                                    const SCAN_ORDER *scan_order, int eob) {
  const int16_t *const scan = scan_order->scan;
  int cul_level = 0;
  int c;

  if (eob == 0) return 0;
  for (c = 0; c < eob; ++c) {
    cul_level += abs(qcoeff[scan[c]]);
    if (cul_level > COEFF_CONTEXT_MASK) break;
  }

  cul_level = AOMMIN(COEFF_CONTEXT_MASK, cul_level);
  set_dc_sign(&cul_level, qcoeff[0]);

  return (uint8_t)cul_level;
}

static void update_tx_type_count(const AV1_COMP *cpi, const AV1_COMMON *cm,
                                 MACROBLOCKD *xd, int blk_row, int blk_col,
                                 int plane, TX_SIZE tx_size,
                                 FRAME_COUNTS *counts,
                                 uint8_t allow_update_cdf) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  int is_inter = is_inter_block(mbmi);
  const int reduced_tx_set_used = cm->features.reduced_tx_set_used;
  FRAME_CONTEXT *fc = xd->tile_ctx;
#if !CONFIG_ENTROPY_STATS
  (void)counts;
#endif  // !CONFIG_ENTROPY_STATS

  // Only y plane's tx_type is updated
  if (plane > 0) return;
  const TX_TYPE tx_type = av1_get_tx_type(xd, PLANE_TYPE_Y, blk_row, blk_col,
                                          tx_size, reduced_tx_set_used);
  if (is_inter) {
    if (cpi->oxcf.txfm_cfg.use_inter_dct_only) {
      assert(tx_type == DCT_DCT);
    }
  } else {
    if (cpi->oxcf.txfm_cfg.use_intra_dct_only) {
      assert(tx_type == DCT_DCT);
    } else if (cpi->oxcf.txfm_cfg.use_intra_default_tx_only) {
      const TX_TYPE default_type = get_default_tx_type(
          PLANE_TYPE_Y, xd, tx_size, cpi->use_screen_content_tools);
      (void)default_type;
      assert(tx_type == default_type);
    }
  }

  if (get_ext_tx_types(tx_size, is_inter, reduced_tx_set_used) > 1 &&
      cm->quant_params.base_qindex > 0 && !mbmi->skip_txfm &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    const int eset = get_ext_tx_set(tx_size, is_inter, reduced_tx_set_used);
    if (eset > 0) {
      const TxSetType tx_set_type =
          av1_get_ext_tx_set_type(tx_size, is_inter, reduced_tx_set_used);
      if (is_inter) {
        if (allow_update_cdf) {
          update_cdf(fc->inter_ext_tx_cdf[eset][txsize_sqr_map[tx_size]],
                     av1_ext_tx_ind[tx_set_type][tx_type],
                     av1_num_ext_tx_set[tx_set_type]);
        }
#if CONFIG_ENTROPY_STATS
        ++counts->inter_ext_tx[eset][txsize_sqr_map[tx_size]]
                              [av1_ext_tx_ind[tx_set_type][tx_type]];
#endif  // CONFIG_ENTROPY_STATS
      } else {
        PREDICTION_MODE intra_dir;
        if (mbmi->filter_intra_mode_info.use_filter_intra)
          intra_dir = fimode_to_intradir[mbmi->filter_intra_mode_info
                                             .filter_intra_mode];
        else
          intra_dir = mbmi->mode;
#if CONFIG_ENTROPY_STATS
        ++counts->intra_ext_tx[eset][txsize_sqr_map[tx_size]][intra_dir]
                              [av1_ext_tx_ind[tx_set_type][tx_type]];
#endif  // CONFIG_ENTROPY_STATS
        if (allow_update_cdf) {
          update_cdf(
              fc->intra_ext_tx_cdf[eset][txsize_sqr_map[tx_size]][intra_dir],
              av1_ext_tx_ind[tx_set_type][tx_type],
              av1_num_ext_tx_set[tx_set_type]);
        }
      }
    }
  }
}

void av1_update_and_record_txb_context(int plane, int block, int blk_row,
                                       int blk_col, BLOCK_SIZE plane_bsize,
                                       TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int eob = p->eobs[block];
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  const PLANE_TYPE plane_type = pd->plane_type;
  const TX_TYPE tx_type =
      av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                      cm->features.reduced_tx_set_used);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  tran_low_t *tcoeff;
  assert(args->dry_run != DRY_RUN_COSTCOEFFS);
  if (args->dry_run == OUTPUT_ENABLED) {
    MB_MODE_INFO *mbmi = xd->mi[0];
    TXB_CTX txb_ctx;
    get_txb_ctx(plane_bsize, tx_size, plane,
                pd->above_entropy_context + blk_col,
                pd->left_entropy_context + blk_row, &txb_ctx);
    const int bwl = get_txb_bwl(tx_size);
    const int width = get_txb_wide(tx_size);
    const int height = get_txb_high(tx_size);
    const uint8_t allow_update_cdf = args->allow_update_cdf;
    const TX_SIZE txsize_ctx = get_txsize_entropy_ctx(tx_size);
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
#if CONFIG_ENTROPY_STATS
    int cdf_idx = cm->coef_cdf_category;
    ++td->counts->txb_skip[cdf_idx][txsize_ctx][txb_ctx.txb_skip_ctx][eob == 0];
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
      update_cdf(ec_ctx->txb_skip_cdf[txsize_ctx][txb_ctx.txb_skip_ctx],
                 eob == 0, 2);
    }

    CB_COEFF_BUFFER *cb_coef_buff = x->cb_coef_buff;
    const int txb_offset = x->mbmi_ext_frame->cb_offset[plane_type] /
                           (TX_SIZE_W_MIN * TX_SIZE_H_MIN);
    uint16_t *eob_txb = cb_coef_buff->eobs[plane] + txb_offset;
    uint8_t *const entropy_ctx = cb_coef_buff->entropy_ctx[plane] + txb_offset;
    entropy_ctx[block] = txb_ctx.txb_skip_ctx;
    eob_txb[block] = eob;

    if (eob == 0) {
      av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, 0, blk_col,
                               blk_row);
      return;
    }
    const int segment_id = mbmi->segment_id;
    const int seg_eob = av1_get_tx_eob(&cpi->common.seg, segment_id, tx_size);
    tran_low_t *tcoeff_txb =
        cb_coef_buff->tcoeff[plane] + x->mbmi_ext_frame->cb_offset[plane_type];
    tcoeff = tcoeff_txb + block_offset;
    memcpy(tcoeff, qcoeff, sizeof(*tcoeff) * seg_eob);

    uint8_t levels_buf[TX_PAD_2D];
    uint8_t *const levels = set_levels(levels_buf, width);
    av1_txb_init_levels(tcoeff, width, height, levels);
    update_tx_type_count(cpi, cm, xd, blk_row, blk_col, plane, tx_size,
                         td->counts, allow_update_cdf);

    const TX_CLASS tx_class = tx_type_to_class[tx_type];
    const int16_t *const scan = scan_order->scan;

    // record tx type usage
    td->rd_counts.tx_type_used[tx_size][tx_type]++;

#if CONFIG_ENTROPY_STATS
    av1_update_eob_context(cdf_idx, eob, tx_size, tx_class, plane_type, ec_ctx,
                           td->counts, allow_update_cdf);
#else
    av1_update_eob_context(eob, tx_size, tx_class, plane_type, ec_ctx,
                           allow_update_cdf);
#endif

    DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
    av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class,
                            coeff_contexts);

    for (int c = eob - 1; c >= 0; --c) {
      const int pos = scan[c];
      const int coeff_ctx = coeff_contexts[pos];
      const tran_low_t v = qcoeff[pos];
      const tran_low_t level = abs(v);

      if (allow_update_cdf) {
        if (c == eob - 1) {
          assert(coeff_ctx < 4);
          update_cdf(
              ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][coeff_ctx],
              AOMMIN(level, 3) - 1, 3);
        } else {
          update_cdf(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][coeff_ctx],
                     AOMMIN(level, 3), 4);
        }
      }
      if (c == eob - 1) {
        assert(coeff_ctx < 4);
#if CONFIG_ENTROPY_STATS
        ++td->counts->coeff_base_eob_multi[cdf_idx][txsize_ctx][plane_type]
                                          [coeff_ctx][AOMMIN(level, 3) - 1];
      } else {
        ++td->counts->coeff_base_multi[cdf_idx][txsize_ctx][plane_type]
                                      [coeff_ctx][AOMMIN(level, 3)];
#endif
      }
      if (level > NUM_BASE_LEVELS) {
        const int base_range = level - 1 - NUM_BASE_LEVELS;
        const int br_ctx = get_br_ctx(levels, pos, bwl, tx_class);
        for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
          const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
          if (allow_update_cdf) {
            update_cdf(ec_ctx->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)]
                                           [plane_type][br_ctx],
                       k, BR_CDF_SIZE);
          }
          for (int lps = 0; lps < BR_CDF_SIZE - 1; lps++) {
#if CONFIG_ENTROPY_STATS
            ++td->counts->coeff_lps[AOMMIN(txsize_ctx, TX_32X32)][plane_type]
                                   [lps][br_ctx][lps == k];
#endif  // CONFIG_ENTROPY_STATS
            if (lps == k) break;
          }
#if CONFIG_ENTROPY_STATS
          ++td->counts->coeff_lps_multi[cdf_idx][AOMMIN(txsize_ctx, TX_32X32)]
                                       [plane_type][br_ctx][k];
#endif
          if (k < BR_CDF_SIZE - 1) break;
        }
      }
    }
    // Update the context needed to code the DC sign (if applicable)
    if (tcoeff[0] != 0) {
      const int dc_sign = (tcoeff[0] < 0) ? 1 : 0;
      const int dc_sign_ctx = txb_ctx.dc_sign_ctx;
#if CONFIG_ENTROPY_STATS
      ++td->counts->dc_sign[plane_type][dc_sign_ctx][dc_sign];
#endif  // CONFIG_ENTROPY_STATS
      if (allow_update_cdf)
        update_cdf(ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], dc_sign, 2);
      entropy_ctx[block] |= dc_sign_ctx << DC_SIGN_CTX_SHIFT;
    }
  } else {
    tcoeff = qcoeff;
  }
  const uint8_t cul_level =
      av1_get_txb_entropy_context(tcoeff, scan_order, eob);
  av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, cul_level,
                           blk_col, blk_row);
}

void av1_update_intra_mb_txb_context(const AV1_COMP *cpi, ThreadData *td,
                                     RUN_TYPE dry_run, BLOCK_SIZE bsize,
                                     uint8_t allow_update_cdf) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  struct tokenize_b_args arg = { cpi, td, 0, allow_update_cdf, dry_run };
  if (mbmi->skip_txfm) {
    av1_reset_entropy_context(xd, bsize, num_planes);
    return;
  }

  for (int plane = 0; plane < num_planes; ++plane) {
    if (plane && !xd->is_chroma_ref) break;
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const int ss_x = pd->subsampling_x;
    const int ss_y = pd->subsampling_y;
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, ss_x, ss_y);
    av1_foreach_transformed_block_in_plane(
        xd, plane_bsize, plane, av1_update_and_record_txb_context, &arg);
  }
}

CB_COEFF_BUFFER *av1_get_cb_coeff_buffer(const struct AV1_COMP *cpi, int mi_row,
                                         int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  const int mib_size_log2 = cm->seq_params.mib_size_log2;
  const int stride = (cm->mi_params.mi_cols >> mib_size_log2) + 1;
  const int offset =
      (mi_row >> mib_size_log2) * stride + (mi_col >> mib_size_log2);
  return cpi->coeff_buffer_base + offset;
}
