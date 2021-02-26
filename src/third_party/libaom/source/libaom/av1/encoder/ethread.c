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

#include "av1/common/warped_motion.h"

#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encoder_alloc.h"
#include "av1/encoder/ethread.h"
#if !CONFIG_REALTIME_ONLY
#include "av1/encoder/firstpass.h"
#endif
#include "av1/encoder/global_motion.h"
#include "av1/encoder/global_motion_facade.h"
#include "av1/encoder/rdopt.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/encoder/tpl_model.h"

static AOM_INLINE void accumulate_rd_opt(ThreadData *td, ThreadData *td_t) {
  for (int i = 0; i < REFERENCE_MODES; i++)
    td->rd_counts.comp_pred_diff[i] += td_t->rd_counts.comp_pred_diff[i];

  for (int i = 0; i < REF_FRAMES; i++)
    td->rd_counts.global_motion_used[i] +=
        td_t->rd_counts.global_motion_used[i];

  td->rd_counts.compound_ref_used_flag |=
      td_t->rd_counts.compound_ref_used_flag;
  td->rd_counts.skip_mode_used_flag |= td_t->rd_counts.skip_mode_used_flag;

  for (int i = 0; i < TX_SIZES_ALL; i++) {
    for (int j = 0; j < TX_TYPES; j++)
      td->rd_counts.tx_type_used[i][j] += td_t->rd_counts.tx_type_used[i][j];
  }

  for (int i = 0; i < BLOCK_SIZES_ALL; i++) {
    for (int j = 0; j < 2; j++) {
      td->rd_counts.obmc_used[i][j] += td_t->rd_counts.obmc_used[i][j];
    }
  }

  for (int i = 0; i < 2; i++) {
    td->rd_counts.warped_used[i] += td_t->rd_counts.warped_used[i];
  }
}

static AOM_INLINE void update_delta_lf_for_row_mt(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &cpi->td.mb.e_mbd;
  const int mib_size = cm->seq_params.mib_size;
  const int frame_lf_count =
      av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
  for (int row = 0; row < cm->tiles.rows; row++) {
    for (int col = 0; col < cm->tiles.cols; col++) {
      TileDataEnc *tile_data = &cpi->tile_data[row * cm->tiles.cols + col];
      const TileInfo *const tile_info = &tile_data->tile_info;
      for (int mi_row = tile_info->mi_row_start; mi_row < tile_info->mi_row_end;
           mi_row += mib_size) {
        if (mi_row == tile_info->mi_row_start)
          av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
        for (int mi_col = tile_info->mi_col_start;
             mi_col < tile_info->mi_col_end; mi_col += mib_size) {
          const int idx_str = cm->mi_params.mi_stride * mi_row + mi_col;
          MB_MODE_INFO **mi = cm->mi_params.mi_grid_base + idx_str;
          MB_MODE_INFO *mbmi = mi[0];
          if (mbmi->skip_txfm == 1 && (mbmi->bsize == cm->seq_params.sb_size)) {
            for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id)
              mbmi->delta_lf[lf_id] = xd->delta_lf[lf_id];
            mbmi->delta_lf_from_base = xd->delta_lf_from_base;
          } else {
            if (cm->delta_q_info.delta_lf_multi) {
              for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id)
                xd->delta_lf[lf_id] = mbmi->delta_lf[lf_id];
            } else {
              xd->delta_lf_from_base = mbmi->delta_lf_from_base;
            }
          }
        }
      }
    }
  }
}

void av1_row_mt_sync_read_dummy(AV1EncRowMultiThreadSync *row_mt_sync, int r,
                                int c) {
  (void)row_mt_sync;
  (void)r;
  (void)c;
  return;
}

void av1_row_mt_sync_write_dummy(AV1EncRowMultiThreadSync *row_mt_sync, int r,
                                 int c, int cols) {
  (void)row_mt_sync;
  (void)r;
  (void)c;
  (void)cols;
  return;
}

void av1_row_mt_sync_read(AV1EncRowMultiThreadSync *row_mt_sync, int r, int c) {
#if CONFIG_MULTITHREAD
  const int nsync = row_mt_sync->sync_range;

  if (r) {
    pthread_mutex_t *const mutex = &row_mt_sync->mutex_[r - 1];
    pthread_mutex_lock(mutex);

    while (c > row_mt_sync->num_finished_cols[r - 1] - nsync) {
      pthread_cond_wait(&row_mt_sync->cond_[r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)row_mt_sync;
  (void)r;
  (void)c;
#endif  // CONFIG_MULTITHREAD
}

void av1_row_mt_sync_write(AV1EncRowMultiThreadSync *row_mt_sync, int r, int c,
                           int cols) {
#if CONFIG_MULTITHREAD
  const int nsync = row_mt_sync->sync_range;
  int cur;
  // Only signal when there are enough encoded blocks for next row to run.
  int sig = 1;

  if (c < cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = cols + nsync;
  }

  if (sig) {
    pthread_mutex_lock(&row_mt_sync->mutex_[r]);

    row_mt_sync->num_finished_cols[r] = cur;

    pthread_cond_signal(&row_mt_sync->cond_[r]);
    pthread_mutex_unlock(&row_mt_sync->mutex_[r]);
  }
#else
  (void)row_mt_sync;
  (void)r;
  (void)c;
  (void)cols;
#endif  // CONFIG_MULTITHREAD
}

// Allocate memory for row synchronization
static void row_mt_sync_mem_alloc(AV1EncRowMultiThreadSync *row_mt_sync,
                                  AV1_COMMON *cm, int rows) {
#if CONFIG_MULTITHREAD
  int i;

  CHECK_MEM_ERROR(cm, row_mt_sync->mutex_,
                  aom_malloc(sizeof(*row_mt_sync->mutex_) * rows));
  if (row_mt_sync->mutex_) {
    for (i = 0; i < rows; ++i) {
      pthread_mutex_init(&row_mt_sync->mutex_[i], NULL);
    }
  }

  CHECK_MEM_ERROR(cm, row_mt_sync->cond_,
                  aom_malloc(sizeof(*row_mt_sync->cond_) * rows));
  if (row_mt_sync->cond_) {
    for (i = 0; i < rows; ++i) {
      pthread_cond_init(&row_mt_sync->cond_[i], NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD

  CHECK_MEM_ERROR(cm, row_mt_sync->num_finished_cols,
                  aom_malloc(sizeof(*row_mt_sync->num_finished_cols) * rows));

  row_mt_sync->rows = rows;
  // Set up nsync.
  row_mt_sync->sync_range = 1;
}

// Deallocate row based multi-threading synchronization related mutex and data
static void row_mt_sync_mem_dealloc(AV1EncRowMultiThreadSync *row_mt_sync) {
  if (row_mt_sync != NULL) {
#if CONFIG_MULTITHREAD
    int i;

    if (row_mt_sync->mutex_ != NULL) {
      for (i = 0; i < row_mt_sync->rows; ++i) {
        pthread_mutex_destroy(&row_mt_sync->mutex_[i]);
      }
      aom_free(row_mt_sync->mutex_);
    }
    if (row_mt_sync->cond_ != NULL) {
      for (i = 0; i < row_mt_sync->rows; ++i) {
        pthread_cond_destroy(&row_mt_sync->cond_[i]);
      }
      aom_free(row_mt_sync->cond_);
    }
#endif  // CONFIG_MULTITHREAD
    aom_free(row_mt_sync->num_finished_cols);

    // clear the structure as the source of this call may be dynamic change
    // in tiles in which case this call will be followed by an _alloc()
    // which may fail.
    av1_zero(*row_mt_sync);
  }
}

static void row_mt_mem_alloc(AV1_COMP *cpi, int max_rows, int max_cols,
                             int alloc_row_ctx) {
  struct AV1Common *cm = &cpi->common;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &cpi->mt_info.enc_row_mt;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int tile_col, tile_row;

  // Allocate memory for row based multi-threading
  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &cpi->tile_data[tile_index];

      row_mt_sync_mem_alloc(&this_tile->row_mt_sync, cm, max_rows);

      this_tile->row_ctx = NULL;
      if (alloc_row_ctx) {
        assert(max_cols > 0);
        const int num_row_ctx = AOMMAX(1, (max_cols - 1));
        CHECK_MEM_ERROR(cm, this_tile->row_ctx,
                        (FRAME_CONTEXT *)aom_memalign(
                            16, num_row_ctx * sizeof(*this_tile->row_ctx)));
      }
    }
  }
  enc_row_mt->allocated_tile_cols = tile_cols;
  enc_row_mt->allocated_tile_rows = tile_rows;
  enc_row_mt->allocated_rows = max_rows;
  enc_row_mt->allocated_cols = max_cols - 1;
}

void av1_row_mt_mem_dealloc(AV1_COMP *cpi) {
  AV1EncRowMultiThreadInfo *const enc_row_mt = &cpi->mt_info.enc_row_mt;
  const int tile_cols = enc_row_mt->allocated_tile_cols;
  const int tile_rows = enc_row_mt->allocated_tile_rows;
  int tile_col, tile_row;

  // Free row based multi-threading sync memory
  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &cpi->tile_data[tile_index];

      row_mt_sync_mem_dealloc(&this_tile->row_mt_sync);

      if (cpi->oxcf.algo_cfg.cdf_update_mode) aom_free(this_tile->row_ctx);
    }
  }
  enc_row_mt->allocated_rows = 0;
  enc_row_mt->allocated_cols = 0;
  enc_row_mt->allocated_tile_cols = 0;
  enc_row_mt->allocated_tile_rows = 0;
}

static AOM_INLINE void assign_tile_to_thread(int *thread_id_to_tile_id,
                                             int num_tiles, int num_workers) {
  int tile_id = 0;
  int i;

  for (i = 0; i < num_workers; i++) {
    thread_id_to_tile_id[i] = tile_id++;
    if (tile_id == num_tiles) tile_id = 0;
  }
}

static AOM_INLINE int get_next_job(TileDataEnc *const tile_data,
                                   int *current_mi_row, int mib_size) {
  AV1EncRowMultiThreadSync *const row_mt_sync = &tile_data->row_mt_sync;
  const int mi_row_end = tile_data->tile_info.mi_row_end;

  if (row_mt_sync->next_mi_row < mi_row_end) {
    *current_mi_row = row_mt_sync->next_mi_row;
    row_mt_sync->num_threads_working++;
    row_mt_sync->next_mi_row += mib_size;
    return 1;
  }
  return 0;
}

static AOM_INLINE void switch_tile_and_get_next_job(
    AV1_COMMON *const cm, TileDataEnc *const tile_data, int *cur_tile_id,
    int *current_mi_row, int *end_of_frame, int is_firstpass) {
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;

  int tile_id = -1;  // Stores the tile ID with minimum proc done
  int max_mis_to_encode = 0;
  int min_num_threads_working = INT_MAX;

  for (int tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (int tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &tile_data[tile_index];
      AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;

#if CONFIG_REALTIME_ONLY
      int num_b_rows_in_tile =
          av1_get_sb_rows_in_tile(cm, this_tile->tile_info);
      int num_b_cols_in_tile =
          av1_get_sb_cols_in_tile(cm, this_tile->tile_info);
#else
      int num_b_rows_in_tile =
          is_firstpass ? av1_get_mb_rows_in_tile(this_tile->tile_info)
                       : av1_get_sb_rows_in_tile(cm, this_tile->tile_info);
      int num_b_cols_in_tile =
          is_firstpass ? av1_get_mb_cols_in_tile(this_tile->tile_info)
                       : av1_get_sb_cols_in_tile(cm, this_tile->tile_info);
#endif
      int theoretical_limit_on_threads =
          AOMMIN((num_b_cols_in_tile + 1) >> 1, num_b_rows_in_tile);
      int num_threads_working = row_mt_sync->num_threads_working;

      if (num_threads_working < theoretical_limit_on_threads) {
        int num_mis_to_encode =
            this_tile->tile_info.mi_row_end - row_mt_sync->next_mi_row;

        // Tile to be processed by this thread is selected on the basis of
        // availability of jobs:
        // 1) If jobs are available, tile to be processed is chosen on the
        // basis of minimum number of threads working for that tile. If two or
        // more tiles have same number of threads working for them, then the
        // tile with maximum number of jobs available will be chosen.
        // 2) If no jobs are available, then end_of_frame is reached.
        if (num_mis_to_encode > 0) {
          if (num_threads_working < min_num_threads_working) {
            min_num_threads_working = num_threads_working;
            max_mis_to_encode = 0;
          }
          if (num_threads_working == min_num_threads_working &&
              num_mis_to_encode > max_mis_to_encode) {
            tile_id = tile_index;
            max_mis_to_encode = num_mis_to_encode;
          }
        }
      }
    }
  }
  if (tile_id == -1) {
    *end_of_frame = 1;
  } else {
    // Update the current tile id to the tile id that will be processed next,
    // which will be the least processed tile.
    *cur_tile_id = tile_id;
    get_next_job(&tile_data[tile_id], current_mi_row,
                 is_firstpass ? FP_MIB_SIZE : cm->seq_params.mib_size);
  }
}

#if !CONFIG_REALTIME_ONLY
static int fp_enc_row_mt_worker_hook(void *arg1, void *unused) {
  EncWorkerData *const thread_data = (EncWorkerData *)arg1;
  AV1_COMP *const cpi = thread_data->cpi;
  AV1_COMMON *const cm = &cpi->common;
  int thread_id = thread_data->thread_id;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &cpi->mt_info.enc_row_mt;
  int cur_tile_id = enc_row_mt->thread_id_to_tile_id[thread_id];
#if CONFIG_MULTITHREAD
  pthread_mutex_t *enc_row_mt_mutex_ = enc_row_mt->mutex_;
#endif
  (void)unused;

  assert(cur_tile_id != -1);

  int end_of_frame = 0;
  while (1) {
    int current_mi_row = -1;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(enc_row_mt_mutex_);
#endif
    if (!get_next_job(&cpi->tile_data[cur_tile_id], &current_mi_row,
                      FP_MIB_SIZE)) {
      // No jobs are available for the current tile. Query for the status of
      // other tiles and get the next job if available
      switch_tile_and_get_next_job(cm, cpi->tile_data, &cur_tile_id,
                                   &current_mi_row, &end_of_frame, 1);
    }
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(enc_row_mt_mutex_);
#endif
    if (end_of_frame == 1) break;

    TileDataEnc *const this_tile = &cpi->tile_data[cur_tile_id];
    AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;
    ThreadData *td = thread_data->td;

    assert(current_mi_row != -1 &&
           current_mi_row <= this_tile->tile_info.mi_row_end);

    av1_first_pass_row(cpi, td, this_tile, current_mi_row >> FP_MIB_SIZE_LOG2);
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(enc_row_mt_mutex_);
#endif
    row_mt_sync->num_threads_working--;
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(enc_row_mt_mutex_);
#endif
  }

  return 1;
}
#endif

static int enc_row_mt_worker_hook(void *arg1, void *unused) {
  EncWorkerData *const thread_data = (EncWorkerData *)arg1;
  AV1_COMP *const cpi = thread_data->cpi;
  AV1_COMMON *const cm = &cpi->common;
  int thread_id = thread_data->thread_id;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &cpi->mt_info.enc_row_mt;
  int cur_tile_id = enc_row_mt->thread_id_to_tile_id[thread_id];
#if CONFIG_MULTITHREAD
  pthread_mutex_t *enc_row_mt_mutex_ = enc_row_mt->mutex_;
#endif
  (void)unused;

  assert(cur_tile_id != -1);

  int end_of_frame = 0;
  while (1) {
    int current_mi_row = -1;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(enc_row_mt_mutex_);
#endif
    if (!get_next_job(&cpi->tile_data[cur_tile_id], &current_mi_row,
                      cm->seq_params.mib_size)) {
      // No jobs are available for the current tile. Query for the status of
      // other tiles and get the next job if available
      switch_tile_and_get_next_job(cm, cpi->tile_data, &cur_tile_id,
                                   &current_mi_row, &end_of_frame, 0);
    }
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(enc_row_mt_mutex_);
#endif
    if (end_of_frame == 1) break;

    TileDataEnc *const this_tile = &cpi->tile_data[cur_tile_id];
    AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;
    const TileInfo *const tile_info = &this_tile->tile_info;
    const int tile_row = tile_info->tile_row;
    const int tile_col = tile_info->tile_col;
    ThreadData *td = thread_data->td;

    assert(current_mi_row != -1 && current_mi_row <= tile_info->mi_row_end);

    td->mb.e_mbd.tile_ctx = td->tctx;
    td->mb.tile_pb_ctx = &this_tile->tctx;

    if (this_tile->allow_update_cdf) {
      td->mb.row_ctx = this_tile->row_ctx;
      if (current_mi_row == tile_info->mi_row_start)
        memcpy(td->mb.e_mbd.tile_ctx, &this_tile->tctx, sizeof(FRAME_CONTEXT));
    } else {
      memcpy(td->mb.e_mbd.tile_ctx, &this_tile->tctx, sizeof(FRAME_CONTEXT));
    }

    av1_init_above_context(&cm->above_contexts, av1_num_planes(cm), tile_row,
                           &td->mb.e_mbd);

    cfl_init(&td->mb.e_mbd.cfl, &cm->seq_params);
    av1_crc32c_calculator_init(
        &td->mb.txfm_search_info.mb_rd_record.crc_calculator);

    av1_encode_sb_row(cpi, td, tile_row, tile_col, current_mi_row);
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(enc_row_mt_mutex_);
#endif
    row_mt_sync->num_threads_working--;
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(enc_row_mt_mutex_);
#endif
  }

  return 1;
}

static int enc_worker_hook(void *arg1, void *unused) {
  EncWorkerData *const thread_data = (EncWorkerData *)arg1;
  AV1_COMP *const cpi = thread_data->cpi;
  const AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int t;

  (void)unused;

  for (t = thread_data->start; t < tile_rows * tile_cols;
       t += cpi->mt_info.num_workers) {
    int tile_row = t / tile_cols;
    int tile_col = t % tile_cols;

    TileDataEnc *const this_tile =
        &cpi->tile_data[tile_row * cm->tiles.cols + tile_col];
    thread_data->td->mb.e_mbd.tile_ctx = &this_tile->tctx;
    thread_data->td->mb.tile_pb_ctx = &this_tile->tctx;
    av1_encode_tile(cpi, thread_data->td, tile_row, tile_col);
  }

  return 1;
}

static AOM_INLINE void create_enc_workers(AV1_COMP *cpi, int num_workers) {
  AV1_COMMON *const cm = &cpi->common;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  MultiThreadInfo *const mt_info = &cpi->mt_info;

  assert(mt_info->workers != NULL);
  assert(mt_info->tile_thr_data != NULL);

#if CONFIG_MULTITHREAD
  if (cpi->oxcf.row_mt == 1) {
    AV1EncRowMultiThreadInfo *enc_row_mt = &mt_info->enc_row_mt;
    if (enc_row_mt->mutex_ == NULL) {
      CHECK_MEM_ERROR(cm, enc_row_mt->mutex_,
                      aom_malloc(sizeof(*(enc_row_mt->mutex_))));
      if (enc_row_mt->mutex_) pthread_mutex_init(enc_row_mt->mutex_, NULL);
    }
  }
  AV1GlobalMotionSync *gm_sync = &mt_info->gm_sync;
  if (gm_sync->mutex_ == NULL) {
    CHECK_MEM_ERROR(cm, gm_sync->mutex_,
                    aom_malloc(sizeof(*(gm_sync->mutex_))));
    if (gm_sync->mutex_) pthread_mutex_init(gm_sync->mutex_, NULL);
  }
#endif

  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = &mt_info->tile_thr_data[i];

    ++mt_info->num_enc_workers;

    thread_data->cpi = cpi;
    thread_data->thread_id = i;

    if (i > 0) {
      // Set up sms_tree.
      av1_setup_sms_tree(cpi, thread_data->td);

      alloc_obmc_buffers(&thread_data->td->obmc_buffer, cm);

      CHECK_MEM_ERROR(cm, thread_data->td->inter_modes_info,
                      (InterModesInfo *)aom_malloc(
                          sizeof(*thread_data->td->inter_modes_info)));

      for (int x = 0; x < 2; x++)
        for (int y = 0; y < 2; y++)
          CHECK_MEM_ERROR(
              cm, thread_data->td->hash_value_buffer[x][y],
              (uint32_t *)aom_malloc(
                  AOM_BUFFER_SIZE_FOR_BLOCK_HASH *
                  sizeof(*thread_data->td->hash_value_buffer[0][0])));

      // Allocate frame counters in thread data.
      CHECK_MEM_ERROR(cm, thread_data->td->counts,
                      aom_calloc(1, sizeof(*thread_data->td->counts)));

      // Allocate buffers used by palette coding mode.
      CHECK_MEM_ERROR(
          cm, thread_data->td->palette_buffer,
          aom_memalign(16, sizeof(*thread_data->td->palette_buffer)));

      alloc_compound_type_rd_buffers(cm, &thread_data->td->comp_rd_buffer);

      CHECK_MEM_ERROR(
          cm, thread_data->td->tmp_conv_dst,
          aom_memalign(32, MAX_SB_SIZE * MAX_SB_SIZE *
                               sizeof(*thread_data->td->tmp_conv_dst)));
      for (int j = 0; j < 2; ++j) {
        CHECK_MEM_ERROR(
            cm, thread_data->td->tmp_pred_bufs[j],
            aom_memalign(32, 2 * MAX_MB_PLANE * MAX_SB_SQUARE *
                                 sizeof(*thread_data->td->tmp_pred_bufs[j])));
      }

      if (cpi->sf.part_sf.partition_search_type == VAR_BASED_PARTITION) {
        const int num_64x64_blocks =
            (cm->seq_params.sb_size == BLOCK_64X64) ? 1 : 4;
        CHECK_MEM_ERROR(
            cm, thread_data->td->vt64x64,
            aom_malloc(sizeof(*thread_data->td->vt64x64) * num_64x64_blocks));
      }

      // Create threads
      if (!winterface->reset(worker))
        aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                           "Tile encoder thread creation failed");
    } else {
      // Main thread acts as a worker and uses the thread data in cpi.
      thread_data->td = &cpi->td;
    }
    if (cpi->oxcf.row_mt == 1)
      CHECK_MEM_ERROR(
          cm, thread_data->td->tctx,
          (FRAME_CONTEXT *)aom_memalign(16, sizeof(*thread_data->td->tctx)));
    winterface->sync(worker);
  }
}

void av1_create_workers(AV1_COMP *cpi, int num_workers) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  CHECK_MEM_ERROR(cm, mt_info->workers,
                  aom_malloc(num_workers * sizeof(*mt_info->workers)));

  CHECK_MEM_ERROR(cm, mt_info->tile_thr_data,
                  aom_calloc(num_workers, sizeof(*mt_info->tile_thr_data)));

  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = &mt_info->tile_thr_data[i];

    winterface->init(worker);
    worker->thread_name = "aom enc worker";

    if (i > 0) {
      // Allocate thread data.
      CHECK_MEM_ERROR(cm, thread_data->td,
                      aom_memalign(32, sizeof(*thread_data->td)));
      av1_zero(*thread_data->td);

      // Set up shared coeff buffers.
      av1_setup_shared_coeff_buffer(cm, &thread_data->td->shared_coeff_buf);
    }
    ++mt_info->num_workers;
  }
}

#if !CONFIG_REALTIME_ONLY
static AOM_INLINE void fp_create_enc_workers(AV1_COMP *cpi, int num_workers) {
  AV1_COMMON *const cm = &cpi->common;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  MultiThreadInfo *const mt_info = &cpi->mt_info;

  assert(mt_info->workers != NULL);
  assert(mt_info->tile_thr_data != NULL);

#if CONFIG_MULTITHREAD
  AV1EncRowMultiThreadInfo *enc_row_mt = &mt_info->enc_row_mt;
  if (enc_row_mt->mutex_ == NULL) {
    CHECK_MEM_ERROR(cm, enc_row_mt->mutex_,
                    aom_malloc(sizeof(*(enc_row_mt->mutex_))));
    if (enc_row_mt->mutex_) pthread_mutex_init(enc_row_mt->mutex_, NULL);
  }
#endif

  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = &mt_info->tile_thr_data[i];

    ++mt_info->num_fp_workers;

    thread_data->cpi = cpi;
    thread_data->thread_id = i;

    if (i > 0) {
      // Set up firstpass PICK_MODE_CONTEXT.
      thread_data->td->firstpass_ctx =
          av1_alloc_pmc(cm, BLOCK_16X16, &thread_data->td->shared_coeff_buf);

      // Create threads
      if (!winterface->reset(worker))
        aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                           "Tile encoder thread creation failed");
    } else {
      // Main thread acts as a worker and uses the thread data in cpi.
      thread_data->td = &cpi->td;
    }
    winterface->sync(worker);
  }
}
#endif

static AOM_INLINE void launch_enc_workers(MultiThreadInfo *const mt_info,
                                          int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  // Encode a frame
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;

    // Set the starting tile for each thread.
    thread_data->start = i;

    if (i == 0)
      winterface->execute(worker);
    else
      winterface->launch(worker);
  }
}

static AOM_INLINE void sync_enc_workers(MultiThreadInfo *const mt_info,
                                        AV1_COMMON *const cm, int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int had_error = 0;

  // Encoding ends.
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    had_error |= !winterface->sync(worker);
  }

  if (had_error)
    aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                       "Failed to encode tile data");
}

static AOM_INLINE void accumulate_counters_enc_workers(AV1_COMP *cpi,
                                                       int num_workers) {
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &cpi->mt_info.workers[i];
    EncWorkerData *const thread_data = (EncWorkerData *)worker->data1;
    cpi->intrabc_used |= thread_data->td->intrabc_used;
    cpi->deltaq_used |= thread_data->td->deltaq_used;

    // Accumulate counters.
    if (i > 0) {
      av1_accumulate_frame_counts(&cpi->counts, thread_data->td->counts);
      accumulate_rd_opt(&cpi->td, thread_data->td);
      cpi->td.mb.txfm_search_info.txb_split_count +=
          thread_data->td->mb.txfm_search_info.txb_split_count;
#if CONFIG_SPEED_STATS
      cpi->td.mb.txfm_search_info.tx_search_count +=
          thread_data->td->mb.txfm_search_info.tx_search_count;
#endif  // CONFIG_SPEED_STATS
    }
  }
}

static AOM_INLINE void prepare_enc_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                           int num_workers) {
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->cpi = cpi;
    if (i == 0) {
      thread_data->td = &cpi->td;
    }

    thread_data->td->intrabc_used = 0;
    thread_data->td->deltaq_used = 0;

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
      thread_data->td->rd_counts = cpi->td.rd_counts;
      thread_data->td->mb.obmc_buffer = thread_data->td->obmc_buffer;

      thread_data->td->mb.inter_modes_info = thread_data->td->inter_modes_info;
      for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
          memcpy(thread_data->td->hash_value_buffer[x][y],
                 cpi->td.mb.intrabc_hash_info.hash_value_buffer[x][y],
                 AOM_BUFFER_SIZE_FOR_BLOCK_HASH *
                     sizeof(*thread_data->td->hash_value_buffer[0][0]));
          thread_data->td->mb.intrabc_hash_info.hash_value_buffer[x][y] =
              thread_data->td->hash_value_buffer[x][y];
        }
      }
    }
    if (thread_data->td->counts != &cpi->counts) {
      memcpy(thread_data->td->counts, &cpi->counts, sizeof(cpi->counts));
    }

    if (i > 0) {
      thread_data->td->mb.palette_buffer = thread_data->td->palette_buffer;
      thread_data->td->mb.comp_rd_buffer = thread_data->td->comp_rd_buffer;
      thread_data->td->mb.tmp_conv_dst = thread_data->td->tmp_conv_dst;
      for (int j = 0; j < 2; ++j) {
        thread_data->td->mb.tmp_pred_bufs[j] =
            thread_data->td->tmp_pred_bufs[j];
      }

      thread_data->td->mb.e_mbd.tmp_conv_dst = thread_data->td->mb.tmp_conv_dst;
      for (int j = 0; j < 2; ++j) {
        thread_data->td->mb.e_mbd.tmp_obmc_bufs[j] =
            thread_data->td->mb.tmp_pred_bufs[j];
      }
    }
  }
}

#if !CONFIG_REALTIME_ONLY
static AOM_INLINE void fp_prepare_enc_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                              int num_workers) {
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *const worker = &mt_info->workers[i];
    EncWorkerData *const thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->cpi = cpi;
    if (i == 0) {
      thread_data->td = &cpi->td;
    }

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
    }
  }
}
#endif

// Computes the number of workers for row multi-threading of encoding stage
static AOM_INLINE int compute_num_enc_row_mt_workers(AV1_COMMON *const cm,
                                                     int max_threads) {
  TileInfo tile_info;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int total_num_threads_row_mt = 0;
  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      av1_tile_init(&tile_info, cm, row, col);
      const int num_sb_rows_in_tile = av1_get_sb_rows_in_tile(cm, tile_info);
      const int num_sb_cols_in_tile = av1_get_sb_cols_in_tile(cm, tile_info);
      total_num_threads_row_mt +=
          AOMMIN((num_sb_cols_in_tile + 1) >> 1, num_sb_rows_in_tile);
    }
  }
  return AOMMIN(max_threads, total_num_threads_row_mt);
}

// Computes the number of workers for tile multi-threading of encoding stage
static AOM_INLINE int compute_num_enc_tile_mt_workers(AV1_COMMON *const cm,
                                                      int max_threads) {
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  return AOMMIN(max_threads, tile_cols * tile_rows);
}

// Computes the number of workers for encoding stage (row/tile multi-threading)
int av1_compute_num_enc_workers(AV1_COMP *cpi, int max_workers) {
  if (max_workers <= 1) return 1;
  if (cpi->oxcf.row_mt)
    return compute_num_enc_row_mt_workers(&cpi->common, max_workers);
  else
    return compute_num_enc_tile_mt_workers(&cpi->common, max_workers);
}

void av1_encode_tiles_mt(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int num_workers = av1_compute_num_enc_workers(cpi, mt_info->num_workers);

  assert(IMPLIES(cpi->tile_data == NULL,
                 cpi->allocated_tiles < tile_cols * tile_rows));
  if (cpi->allocated_tiles < tile_cols * tile_rows) av1_alloc_tile_data(cpi);

  av1_init_tile_data(cpi);
  // Only run once to create threads and allocate thread data.
  if (mt_info->num_enc_workers == 0) {
    create_enc_workers(cpi, num_workers);
  } else {
    num_workers = AOMMIN(num_workers, mt_info->num_enc_workers);
  }
  prepare_enc_workers(cpi, enc_worker_hook, num_workers);
  launch_enc_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, cm, num_workers);
  accumulate_counters_enc_workers(cpi, num_workers);
}

// Accumulate frame counts. FRAME_COUNTS consist solely of 'unsigned int'
// members, so we treat it as an array, and sum over the whole length.
void av1_accumulate_frame_counts(FRAME_COUNTS *acc_counts,
                                 const FRAME_COUNTS *counts) {
  unsigned int *const acc = (unsigned int *)acc_counts;
  const unsigned int *const cnt = (const unsigned int *)counts;

  const unsigned int n_counts = sizeof(FRAME_COUNTS) / sizeof(unsigned int);

  for (unsigned int i = 0; i < n_counts; i++) acc[i] += cnt[i];
}

// Computes the maximum number of sb_rows for row multi-threading of encoding
// stage
static AOM_INLINE void compute_max_sb_rows_cols(AV1_COMP *cpi, int *max_sb_rows,
                                                int *max_sb_cols) {
  AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      const int tile_index = row * cm->tiles.cols + col;
      TileInfo tile_info = cpi->tile_data[tile_index].tile_info;
      const int num_sb_rows_in_tile = av1_get_sb_rows_in_tile(cm, tile_info);
      const int num_sb_cols_in_tile = av1_get_sb_cols_in_tile(cm, tile_info);
      *max_sb_rows = AOMMAX(*max_sb_rows, num_sb_rows_in_tile);
      *max_sb_cols = AOMMAX(*max_sb_cols, num_sb_cols_in_tile);
    }
  }
}

#if !CONFIG_REALTIME_ONLY
// Computes the number of workers for firstpass stage (row/tile multi-threading)
int av1_fp_compute_num_enc_workers(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int total_num_threads_row_mt = 0;
  TileInfo tile_info;

  if (cpi->oxcf.max_threads <= 1) return 1;

  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      av1_tile_init(&tile_info, cm, row, col);
      const int num_mb_rows_in_tile = av1_get_mb_rows_in_tile(tile_info);
      const int num_mb_cols_in_tile = av1_get_mb_cols_in_tile(tile_info);
      total_num_threads_row_mt +=
          AOMMIN((num_mb_cols_in_tile + 1) >> 1, num_mb_rows_in_tile);
    }
  }
  return AOMMIN(cpi->oxcf.max_threads, total_num_threads_row_mt);
}

// Computes the maximum number of mb_rows for row multi-threading of firstpass
// stage
static AOM_INLINE int fp_compute_max_mb_rows(
    const AV1_COMMON *const cm, const TileDataEnc *const tile_data) {
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int max_mb_rows = 0;
  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      const int tile_index = row * cm->tiles.cols + col;
      TileInfo tile_info = tile_data[tile_index].tile_info;
      const int num_mb_rows_in_tile = av1_get_mb_rows_in_tile(tile_info);
      max_mb_rows = AOMMAX(max_mb_rows, num_mb_rows_in_tile);
    }
  }
  return max_mb_rows;
}
#endif

void av1_encode_tiles_row_mt(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &mt_info->enc_row_mt;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int *thread_id_to_tile_id = enc_row_mt->thread_id_to_tile_id;
  int max_sb_rows = 0, max_sb_cols = 0;

  // TODO(ravi.chaudhary@ittiam.com): Currently the percentage of
  // post-processing stages in encoder is quiet low, so limiting the number of
  // threads to the theoretical limit in row-mt does not have much impact on
  // post-processing multi-threading stage. Need to revisit this when
  // post-processing time starts shooting up.
  int num_workers = av1_compute_num_enc_workers(cpi, mt_info->num_workers);

  assert(IMPLIES(cpi->tile_data == NULL,
                 cpi->allocated_tiles < tile_cols * tile_rows));
  if (cpi->allocated_tiles < tile_cols * tile_rows) {
    av1_row_mt_mem_dealloc(cpi);
    av1_alloc_tile_data(cpi);
  }

  av1_init_tile_data(cpi);

  compute_max_sb_rows_cols(cpi, &max_sb_rows, &max_sb_cols);

  if (enc_row_mt->allocated_tile_cols != tile_cols ||
      enc_row_mt->allocated_tile_rows != tile_rows ||
      enc_row_mt->allocated_rows != max_sb_rows ||
      enc_row_mt->allocated_cols != (max_sb_cols - 1)) {
    av1_row_mt_mem_dealloc(cpi);
    row_mt_mem_alloc(cpi, max_sb_rows, max_sb_cols,
                     cpi->oxcf.algo_cfg.cdf_update_mode);
  }

  memset(thread_id_to_tile_id, -1,
         sizeof(*thread_id_to_tile_id) * MAX_NUM_THREADS);

  for (int tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (int tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &cpi->tile_data[tile_index];
      AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;

      // Initialize num_finished_cols to -1 for all rows.
      memset(row_mt_sync->num_finished_cols, -1,
             sizeof(*row_mt_sync->num_finished_cols) * max_sb_rows);
      row_mt_sync->next_mi_row = this_tile->tile_info.mi_row_start;
      row_mt_sync->num_threads_working = 0;

      av1_inter_mode_data_init(this_tile);
      av1_zero_above_context(cm, &cpi->td.mb.e_mbd,
                             this_tile->tile_info.mi_col_start,
                             this_tile->tile_info.mi_col_end, tile_row);
    }
  }

  // Only run once to create threads and allocate thread data.
  if (mt_info->num_enc_workers == 0) {
    create_enc_workers(cpi, num_workers);
  } else {
    num_workers = AOMMIN(num_workers, mt_info->num_enc_workers);
  }
  assign_tile_to_thread(thread_id_to_tile_id, tile_cols * tile_rows,
                        num_workers);
  prepare_enc_workers(cpi, enc_row_mt_worker_hook, num_workers);
  launch_enc_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, cm, num_workers);
  if (cm->delta_q_info.delta_lf_present_flag) update_delta_lf_for_row_mt(cpi);
  accumulate_counters_enc_workers(cpi, num_workers);
}

#if !CONFIG_REALTIME_ONLY
void av1_fp_encode_tiles_row_mt(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &mt_info->enc_row_mt;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int *thread_id_to_tile_id = enc_row_mt->thread_id_to_tile_id;
  int num_workers = 0;
  int max_mb_rows = 0;

  assert(IMPLIES(cpi->tile_data == NULL,
                 cpi->allocated_tiles < tile_cols * tile_rows));
  if (cpi->allocated_tiles < tile_cols * tile_rows) {
    av1_row_mt_mem_dealloc(cpi);
    av1_alloc_tile_data(cpi);
  }

  av1_init_tile_data(cpi);

  max_mb_rows = fp_compute_max_mb_rows(cm, cpi->tile_data);

  // TODO(ravi.chaudhary@ittiam.com): Currently the percentage of
  // post-processing stages in encoder is quiet low, so limiting the number of
  // threads to the theoretical limit in row-mt does not have much impact on
  // post-processing multi-threading stage. Need to revisit this when
  // post-processing time starts shooting up.
  num_workers = av1_fp_compute_num_enc_workers(cpi);

  if (enc_row_mt->allocated_tile_cols != tile_cols ||
      enc_row_mt->allocated_tile_rows != tile_rows ||
      enc_row_mt->allocated_rows != max_mb_rows) {
    av1_row_mt_mem_dealloc(cpi);
    row_mt_mem_alloc(cpi, max_mb_rows, -1, 0);
  }

  memset(thread_id_to_tile_id, -1,
         sizeof(*thread_id_to_tile_id) * MAX_NUM_THREADS);

  for (int tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (int tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_index = tile_row * tile_cols + tile_col;
      TileDataEnc *const this_tile = &cpi->tile_data[tile_index];
      AV1EncRowMultiThreadSync *const row_mt_sync = &this_tile->row_mt_sync;

      // Initialize num_finished_cols to -1 for all rows.
      memset(row_mt_sync->num_finished_cols, -1,
             sizeof(*row_mt_sync->num_finished_cols) * max_mb_rows);
      row_mt_sync->next_mi_row = this_tile->tile_info.mi_row_start;
      row_mt_sync->num_threads_working = 0;
    }
  }

  num_workers = AOMMIN(num_workers, mt_info->num_workers);
  // Only run once to create threads and allocate thread data.
  if (mt_info->num_fp_workers == 0) fp_create_enc_workers(cpi, num_workers);
  assign_tile_to_thread(thread_id_to_tile_id, tile_cols * tile_rows,
                        num_workers);
  fp_prepare_enc_workers(cpi, fp_enc_row_mt_worker_hook, num_workers);
  launch_enc_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, cm, num_workers);
}

void av1_tpl_row_mt_sync_read_dummy(AV1TplRowMultiThreadSync *tpl_mt_sync,
                                    int r, int c) {
  (void)tpl_mt_sync;
  (void)r;
  (void)c;
  return;
}

void av1_tpl_row_mt_sync_write_dummy(AV1TplRowMultiThreadSync *tpl_mt_sync,
                                     int r, int c, int cols) {
  (void)tpl_mt_sync;
  (void)r;
  (void)c;
  (void)cols;
  return;
}

void av1_tpl_row_mt_sync_read(AV1TplRowMultiThreadSync *tpl_row_mt_sync, int r,
                              int c) {
#if CONFIG_MULTITHREAD
  int nsync = tpl_row_mt_sync->sync_range;

  if (r) {
    pthread_mutex_t *const mutex = &tpl_row_mt_sync->mutex_[r - 1];
    pthread_mutex_lock(mutex);

    while (c > tpl_row_mt_sync->num_finished_cols[r - 1] - nsync)
      pthread_cond_wait(&tpl_row_mt_sync->cond_[r - 1], mutex);
    pthread_mutex_unlock(mutex);
  }
#else
  (void)tpl_row_mt_sync;
  (void)r;
  (void)c;
#endif  // CONFIG_MULTITHREAD
}

void av1_tpl_row_mt_sync_write(AV1TplRowMultiThreadSync *tpl_row_mt_sync, int r,
                               int c, int cols) {
#if CONFIG_MULTITHREAD
  int nsync = tpl_row_mt_sync->sync_range;
  int cur;
  // Only signal when there are enough encoded blocks for next row to run.
  int sig = 1;

  if (c < cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = cols + nsync;
  }

  if (sig) {
    pthread_mutex_lock(&tpl_row_mt_sync->mutex_[r]);

    tpl_row_mt_sync->num_finished_cols[r] = cur;

    pthread_cond_signal(&tpl_row_mt_sync->cond_[r]);
    pthread_mutex_unlock(&tpl_row_mt_sync->mutex_[r]);
  }
#else
  (void)tpl_row_mt_sync;
  (void)r;
  (void)c;
  (void)cols;
#endif  // CONFIG_MULTITHREAD
}

// Each worker calls tpl_worker_hook() and computes the tpl data.
static int tpl_worker_hook(void *arg1, void *unused) {
  (void)unused;
  EncWorkerData *thread_data = (EncWorkerData *)arg1;
  AV1_COMP *cpi = thread_data->cpi;
  AV1_COMMON *cm = &cpi->common;
  MACROBLOCK *x = &thread_data->td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  CommonModeInfoParams *mi_params = &cm->mi_params;
  BLOCK_SIZE bsize = convert_length_to_bsize(cpi->tpl_data.tpl_bsize_1d);
  TX_SIZE tx_size = max_txsize_lookup[bsize];
  int mi_height = mi_size_high[bsize];
  int num_active_workers = cpi->tpl_data.tpl_mt_sync.num_threads_working;
  for (int mi_row = thread_data->start * mi_height; mi_row < mi_params->mi_rows;
       mi_row += num_active_workers * mi_height) {
    // Motion estimation row boundary
    av1_set_mv_row_limits(mi_params, &x->mv_limits, mi_row, mi_height,
                          cpi->oxcf.border_in_pixels);
    xd->mb_to_top_edge = -GET_MV_SUBPEL(mi_row * MI_SIZE);
    xd->mb_to_bottom_edge =
        GET_MV_SUBPEL((mi_params->mi_rows - mi_height - mi_row) * MI_SIZE);
    av1_mc_flow_dispenser_row(cpi, x, mi_row, bsize, tx_size);
  }
  return 1;
}

// Deallocate tpl synchronization related mutex and data.
void av1_tpl_dealloc(AV1TplRowMultiThreadSync *tpl_sync) {
  assert(tpl_sync != NULL);

#if CONFIG_MULTITHREAD
  if (tpl_sync->mutex_ != NULL) {
    for (int i = 0; i < tpl_sync->rows; ++i)
      pthread_mutex_destroy(&tpl_sync->mutex_[i]);
    aom_free(tpl_sync->mutex_);
  }
  if (tpl_sync->cond_ != NULL) {
    for (int i = 0; i < tpl_sync->rows; ++i)
      pthread_cond_destroy(&tpl_sync->cond_[i]);
    aom_free(tpl_sync->cond_);
  }
#endif  // CONFIG_MULTITHREAD

  aom_free(tpl_sync->num_finished_cols);
  // clear the structure as the source of this call may be a resize in which
  // case this call will be followed by an _alloc() which may fail.
  av1_zero(*tpl_sync);
}

// Allocate memory for tpl row synchronization.
void av1_tpl_alloc(AV1TplRowMultiThreadSync *tpl_sync, AV1_COMMON *cm,
                   int mb_rows) {
  tpl_sync->rows = mb_rows;
#if CONFIG_MULTITHREAD
  {
    CHECK_MEM_ERROR(cm, tpl_sync->mutex_,
                    aom_malloc(sizeof(*tpl_sync->mutex_) * mb_rows));
    if (tpl_sync->mutex_) {
      for (int i = 0; i < mb_rows; ++i)
        pthread_mutex_init(&tpl_sync->mutex_[i], NULL);
    }

    CHECK_MEM_ERROR(cm, tpl_sync->cond_,
                    aom_malloc(sizeof(*tpl_sync->cond_) * mb_rows));
    if (tpl_sync->cond_) {
      for (int i = 0; i < mb_rows; ++i)
        pthread_cond_init(&tpl_sync->cond_[i], NULL);
    }
  }
#endif  // CONFIG_MULTITHREAD
  CHECK_MEM_ERROR(cm, tpl_sync->num_finished_cols,
                  aom_malloc(sizeof(*tpl_sync->num_finished_cols) * mb_rows));

  // Set up nsync.
  tpl_sync->sync_range = 1;
}

// Each worker is prepared by assigning the hook function and individual thread
// data.
static AOM_INLINE void prepare_tpl_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                           int num_workers) {
  MultiThreadInfo *mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *worker = &mt_info->workers[i];
    EncWorkerData *thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->cpi = cpi;
    if (i == 0) {
      thread_data->td = &cpi->td;
    }

    // Before encoding a frame, copy the thread data from cpi.
    if (thread_data->td != &cpi->td) {
      thread_data->td->mb = cpi->td.mb;
      thread_data->td->mb.obmc_buffer = thread_data->td->obmc_buffer;
    }
  }
}

// Computes num_workers for tpl multi-threading.
static AOM_INLINE int compute_num_tpl_workers(AV1_COMP *cpi) {
  return av1_compute_num_enc_workers(cpi, cpi->mt_info.num_workers);
}

// Implements multi-threading for tpl.
void av1_mc_flow_dispenser_mt(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  CommonModeInfoParams *mi_params = &cm->mi_params;
  MultiThreadInfo *mt_info = &cpi->mt_info;
  TplParams *tpl_data = &cpi->tpl_data;
  AV1TplRowMultiThreadSync *tpl_sync = &tpl_data->tpl_mt_sync;
  int mb_rows = mi_params->mb_rows;
  int num_workers = compute_num_tpl_workers(cpi);

  if (mt_info->num_enc_workers == 0)
    create_enc_workers(cpi, num_workers);
  else
    num_workers = AOMMIN(num_workers, mt_info->num_enc_workers);

  if (mb_rows != tpl_sync->rows) {
    av1_tpl_dealloc(tpl_sync);
    av1_tpl_alloc(tpl_sync, cm, mb_rows);
  }
  tpl_sync->num_threads_working = num_workers;

  // Initialize cur_mb_col to -1 for all MB rows.
  memset(tpl_sync->num_finished_cols, -1,
         sizeof(*tpl_sync->num_finished_cols) * mb_rows);

  prepare_tpl_workers(cpi, tpl_worker_hook, num_workers);
  launch_enc_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, cm, num_workers);
}

// Checks if a job is available in the current direction. If a job is available,
// frame_idx will be populated and returns 1, else returns 0.
static AOM_INLINE int get_next_gm_job(AV1_COMP *cpi, int *frame_idx,
                                      int cur_dir) {
  GlobalMotionInfo *gm_info = &cpi->gm_info;
  JobInfo *job_info = &cpi->mt_info.gm_sync.job_info;

  int total_refs = gm_info->num_ref_frames[cur_dir];
  int8_t cur_frame_to_process = job_info->next_frame_to_process[cur_dir];

  if (cur_frame_to_process < total_refs && !job_info->early_exit[cur_dir]) {
    *frame_idx = gm_info->reference_frames[cur_dir][cur_frame_to_process].frame;
    job_info->next_frame_to_process[cur_dir] += 1;
    return 1;
  }
  return 0;
}

// Switches the current direction and calls the function get_next_gm_job() if
// the speed feature 'prune_ref_frame_for_gm_search' is not set.
static AOM_INLINE void switch_direction(AV1_COMP *cpi, int *frame_idx,
                                        int *cur_dir) {
  if (cpi->sf.gm_sf.prune_ref_frame_for_gm_search) return;
  // Switch the direction and get next job
  *cur_dir = !(*cur_dir);
  get_next_gm_job(cpi, frame_idx, *(cur_dir));
}

// Initializes inliers, num_inliers and segment_map.
static AOM_INLINE void init_gm_thread_data(
    const GlobalMotionInfo *gm_info, GlobalMotionThreadData *thread_data) {
  for (int m = 0; m < RANSAC_NUM_MOTIONS; m++) {
    MotionModel motion_params = thread_data->params_by_motion[m];
    av1_zero(motion_params.params);
    motion_params.num_inliers = 0;
  }

  av1_zero_array(thread_data->segment_map,
                 gm_info->segment_map_w * gm_info->segment_map_h);
}

// Hook function for each thread in global motion multi-threading.
static int gm_mt_worker_hook(void *arg1, void *unused) {
  (void)unused;

  EncWorkerData *thread_data = (EncWorkerData *)arg1;
  AV1_COMP *cpi = thread_data->cpi;
  GlobalMotionInfo *gm_info = &cpi->gm_info;
  MultiThreadInfo *mt_info = &cpi->mt_info;
  JobInfo *job_info = &mt_info->gm_sync.job_info;
  int thread_id = thread_data->thread_id;
  GlobalMotionThreadData *gm_thread_data =
      &mt_info->gm_sync.thread_data[thread_id];
  int cur_dir = job_info->thread_id_to_dir[thread_id];
#if CONFIG_MULTITHREAD
  pthread_mutex_t *gm_mt_mutex_ = mt_info->gm_sync.mutex_;
#endif

  while (1) {
    int ref_buf_idx = -1;
    int ref_frame_idx = -1;

#if CONFIG_MULTITHREAD
    pthread_mutex_lock(gm_mt_mutex_);
#endif

    // Populates ref_buf_idx(the reference frame type) for which global motion
    // estimation will be done.
    if (!get_next_gm_job(cpi, &ref_buf_idx, cur_dir)) {
      // No jobs are available for the current direction. Switch
      // to other direction and get the next job, if available.
      switch_direction(cpi, &ref_buf_idx, &cur_dir);
    }

    // 'ref_frame_idx' holds the index of the current reference frame type in
    // gm_info->reference_frames. job_info->next_frame_to_process will be
    // incremented in get_next_gm_job() and hence subtracting by 1.
    ref_frame_idx = job_info->next_frame_to_process[cur_dir] - 1;

#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(gm_mt_mutex_);
#endif

    if (ref_buf_idx == -1) break;

    init_gm_thread_data(gm_info, gm_thread_data);

    // Compute global motion for the given ref_buf_idx.
    av1_compute_gm_for_valid_ref_frames(
        cpi, gm_info->ref_buf, ref_buf_idx, gm_info->num_src_corners,
        gm_info->src_corners, gm_info->src_buffer,
        gm_thread_data->params_by_motion, gm_thread_data->segment_map,
        gm_info->segment_map_w, gm_info->segment_map_h);

#if CONFIG_MULTITHREAD
    pthread_mutex_lock(gm_mt_mutex_);
#endif
    assert(ref_frame_idx != -1);
    // If global motion w.r.t. current ref frame is
    // INVALID/TRANSLATION/IDENTITY, skip the evaluation of global motion w.r.t
    // the remaining ref frames in that direction. The below exit is disabled
    // when ref frame distance w.r.t. current frame is zero. E.g.:
    // source_alt_ref_frame w.r.t. ARF frames.
    if (cpi->sf.gm_sf.prune_ref_frame_for_gm_search &&
        gm_info->reference_frames[cur_dir][ref_frame_idx].distance != 0 &&
        cpi->common.global_motion[ref_buf_idx].wmtype != ROTZOOM)
      job_info->early_exit[cur_dir] = 1;

#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(gm_mt_mutex_);
#endif
  }
  return 1;
}

// Assigns global motion hook function and thread data to each worker.
static AOM_INLINE void prepare_gm_workers(AV1_COMP *cpi, AVxWorkerHook hook,
                                          int num_workers) {
  MultiThreadInfo *mt_info = &cpi->mt_info;
  for (int i = num_workers - 1; i >= 0; i--) {
    AVxWorker *worker = &mt_info->workers[i];
    EncWorkerData *thread_data = &mt_info->tile_thr_data[i];

    worker->hook = hook;
    worker->data1 = thread_data;
    worker->data2 = NULL;

    thread_data->cpi = cpi;
  }
}

// Assigns available threads to past/future direction.
static AOM_INLINE void assign_thread_to_dir(int8_t *thread_id_to_dir,
                                            int num_workers) {
  int8_t frame_dir_idx = 0;

  for (int i = 0; i < num_workers; i++) {
    thread_id_to_dir[i] = frame_dir_idx++;
    if (frame_dir_idx == MAX_DIRECTIONS) frame_dir_idx = 0;
  }
}

// Computes number of workers for global motion multi-threading.
static AOM_INLINE int compute_gm_workers(const AV1_COMP *cpi) {
  int total_refs =
      cpi->gm_info.num_ref_frames[0] + cpi->gm_info.num_ref_frames[1];
  int max_num_workers = cpi->mt_info.num_workers;
  int max_allowed_workers = cpi->sf.gm_sf.prune_ref_frame_for_gm_search
                                ? AOMMIN(MAX_DIRECTIONS, max_num_workers)
                                : max_num_workers;

  return (AOMMIN(total_refs, max_allowed_workers));
}

// Frees the memory allocated for each worker in global motion multi-threading.
void av1_gm_dealloc(AV1GlobalMotionSync *gm_sync_data) {
  if (gm_sync_data->thread_data != NULL) {
    for (int j = 0; j < gm_sync_data->allocated_workers; j++) {
      GlobalMotionThreadData *thread_data = &gm_sync_data->thread_data[j];
      aom_free(thread_data->segment_map);

      for (int m = 0; m < RANSAC_NUM_MOTIONS; m++)
        aom_free(thread_data->params_by_motion[m].inliers);
    }
    aom_free(gm_sync_data->thread_data);
  }
}

// Allocates memory for inliers and segment_map for each worker in global motion
// multi-threading.
static AOM_INLINE void gm_alloc(AV1_COMP *cpi, int num_workers) {
  AV1_COMMON *cm = &cpi->common;
  AV1GlobalMotionSync *gm_sync = &cpi->mt_info.gm_sync;
  GlobalMotionInfo *gm_info = &cpi->gm_info;

  gm_sync->allocated_workers = num_workers;
  gm_sync->allocated_width = cpi->source->y_width;
  gm_sync->allocated_height = cpi->source->y_height;

  CHECK_MEM_ERROR(cm, gm_sync->thread_data,
                  aom_malloc(sizeof(*gm_sync->thread_data) * num_workers));

  for (int i = 0; i < num_workers; i++) {
    GlobalMotionThreadData *thread_data = &gm_sync->thread_data[i];
    CHECK_MEM_ERROR(
        cm, thread_data->segment_map,
        aom_malloc(sizeof(*thread_data->segment_map) * gm_info->segment_map_w *
                   gm_info->segment_map_h));

    for (int m = 0; m < RANSAC_NUM_MOTIONS; m++) {
      CHECK_MEM_ERROR(
          cm, thread_data->params_by_motion[m].inliers,
          aom_malloc(sizeof(*thread_data->params_by_motion[m].inliers) * 2 *
                     MAX_CORNERS));
    }
  }
}

// Implements multi-threading for global motion.
void av1_global_motion_estimation_mt(AV1_COMP *cpi) {
  AV1GlobalMotionSync *gm_sync = &cpi->mt_info.gm_sync;
  JobInfo *job_info = &gm_sync->job_info;

  av1_zero(*job_info);

  int num_workers = compute_gm_workers(cpi);

  if (num_workers > gm_sync->allocated_workers ||
      cpi->source->y_width != gm_sync->allocated_width ||
      cpi->source->y_height != gm_sync->allocated_height) {
    av1_gm_dealloc(gm_sync);
    gm_alloc(cpi, num_workers);
  }

  assign_thread_to_dir(job_info->thread_id_to_dir, num_workers);
  prepare_gm_workers(cpi, gm_mt_worker_hook, num_workers);
  launch_enc_workers(&cpi->mt_info, num_workers);
  sync_enc_workers(&cpi->mt_info, &cpi->common, num_workers);
}
#endif  // !CONFIG_REALTIME_ONLY
