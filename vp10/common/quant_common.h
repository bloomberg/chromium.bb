/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_COMMON_QUANT_COMMON_H_
#define VP10_COMMON_QUANT_COMMON_H_

#include "vpx/vpx_codec.h"
#include "vp10/common/seg_common.h"
#include "vp10/common/enums.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MINQ 0
#define MAXQ 255
#define QINDEX_RANGE (MAXQ - MINQ + 1)
#define QINDEX_BITS 8
#if CONFIG_AOM_QM
// Total number of QM sets stored
#define NUM_QM_LEVELS 16
/* Offset into the list of QMs. Actual number of levels used is
   (NUM_QM_LEVELS-AOM_QM_OFFSET)
   Lower value of AOM_QM_OFFSET implies more heavily weighted matrices.*/
#define AOM_QM_FIRST 8
#define AOM_QM_LAST NUM_QM_LEVELS
#endif

struct VP10Common;

int16_t vp10_dc_quant(int qindex, int delta, vpx_bit_depth_t bit_depth);
int16_t vp10_ac_quant(int qindex, int delta, vpx_bit_depth_t bit_depth);

int vp10_get_qindex(const struct segmentation *seg, int segment_id,
                    int base_qindex);
#if CONFIG_AOM_QM
// Reduce the large number of quantizers to a smaller number of levels for which
// different matrices may be defined
static inline int aom_get_qmlevel(int qindex) {
  int qmlevel =
      (qindex * (AOM_QM_LAST - AOM_QM_FIRST) + QINDEX_RANGE / 2) / QINDEX_RANGE;
  qmlevel = VPXMIN(qmlevel + AOM_QM_FIRST, NUM_QM_LEVELS - 1);
  return qmlevel;
}
void aom_qm_init(struct VP10Common *cm);
qm_val_t *aom_iqmatrix(struct VP10Common *cm, int qindex, int comp,
                       int log2sizem2, int is_intra);
qm_val_t *aom_qmatrix(struct VP10Common *cm, int qindex, int comp,
                      int log2sizem2, int is_intra);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_QUANT_COMMON_H_
