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

#ifndef AOM_AV1_ENCODER_ETHREAD_H_
#define AOM_AV1_ENCODER_ETHREAD_H_

#ifdef __cplusplus
extern "C" {
#endif

struct AV1_COMP;
struct ThreadData;

typedef struct EncWorkerData {
  struct AV1_COMP *cpi;
  struct ThreadData *td;
  int start;
  int thread_id;
} EncWorkerData;

void av1_row_mt_sync_read(AV1EncRowMultiThreadSync *row_mt_sync, int r, int c);
void av1_row_mt_sync_write(AV1EncRowMultiThreadSync *row_mt_sync, int r, int c,
                           int cols);

void av1_row_mt_sync_read_dummy(AV1EncRowMultiThreadSync *row_mt_sync, int r,
                                int c);
void av1_row_mt_sync_write_dummy(AV1EncRowMultiThreadSync *row_mt_sync, int r,
                                 int c, int cols);

void av1_encode_tiles_mt(struct AV1_COMP *cpi);
void av1_encode_tiles_row_mt(struct AV1_COMP *cpi);

#if !CONFIG_REALTIME_ONLY
void av1_fp_encode_tiles_row_mt(AV1_COMP *cpi);

int av1_fp_compute_num_enc_workers(AV1_COMP *cpi);
#endif

void av1_accumulate_frame_counts(struct FRAME_COUNTS *acc_counts,
                                 const struct FRAME_COUNTS *counts);

void av1_row_mt_mem_dealloc(AV1_COMP *cpi);

void av1_global_motion_estimation_mt(AV1_COMP *cpi);

void av1_gm_dealloc(AV1GlobalMotionSync *gm_sync_data);

#if !CONFIG_REALTIME_ONLY
void av1_tpl_row_mt_sync_read_dummy(AV1TplRowMultiThreadSync *tpl_mt_sync,
                                    int r, int c);
void av1_tpl_row_mt_sync_write_dummy(AV1TplRowMultiThreadSync *tpl_mt_sync,
                                     int r, int c, int cols);

void av1_tpl_row_mt_sync_read(AV1TplRowMultiThreadSync *tpl_mt_sync, int r,
                              int c);
void av1_tpl_row_mt_sync_write(AV1TplRowMultiThreadSync *tpl_mt_sync, int r,
                               int c, int cols);

void av1_mc_flow_dispenser_mt(AV1_COMP *cpi);

void av1_tpl_dealloc(AV1TplRowMultiThreadSync *tpl_sync);

#endif  // !CONFIG_REALTIME_ONLY

int av1_compute_num_enc_workers(AV1_COMP *cpi, int max_workers);

void av1_create_workers(AV1_COMP *cpi, int num_workers);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_ETHREAD_H_
