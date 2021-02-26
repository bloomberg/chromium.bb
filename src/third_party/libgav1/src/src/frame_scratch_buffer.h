/*
 * Copyright 2020 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_FRAME_SCRATCH_BUFFER_H_
#define LIBGAV1_SRC_FRAME_SCRATCH_BUFFER_H_

#include <condition_variable>  // NOLINT (unapproved c++11 header)
#include <cstdint>
#include <memory>
#include <mutex>  // NOLINT (unapproved c++11 header)

#include "src/loop_restoration_info.h"
#include "src/residual_buffer_pool.h"
#include "src/symbol_decoder_context.h"
#include "src/threading_strategy.h"
#include "src/tile_scratch_buffer.h"
#include "src/utils/array_2d.h"
#include "src/utils/block_parameters_holder.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"
#include "src/utils/dynamic_buffer.h"
#include "src/utils/memory.h"
#include "src/utils/stack.h"
#include "src/utils/types.h"
#include "src/yuv_buffer.h"

namespace libgav1 {

// Buffer used to store the unfiltered pixels that are necessary for decoding
// the next superblock row (for the intra prediction process).
using IntraPredictionBuffer =
    std::array<AlignedDynamicBuffer<uint8_t, kMaxAlignment>, kMaxPlanes>;

// Buffer to facilitate decoding a frame. This struct is used only within
// DecoderImpl::DecodeTiles().
struct FrameScratchBuffer {
  LoopRestorationInfo loop_restoration_info;
  Array2D<int16_t> cdef_index;
  Array2D<TransformSize> inter_transform_sizes;
  BlockParametersHolder block_parameters_holder;
  TemporalMotionField motion_field;
  SymbolDecoderContext symbol_decoder_context;
  std::unique_ptr<ResidualBufferPool> residual_buffer_pool;
  // Buffer used to store the cdef borders. This buffer will store 4 rows for
  // every 64x64 block (4 rows for every 32x32 for chroma with subsampling). The
  // indices of the rows that are stored are specified in |kCdefBorderRows|.
  YuvBuffer cdef_border;
  AlignedDynamicBuffer<uint8_t, 16> superres_coefficients[kNumPlaneTypes];
  // Buffer used to temporarily store the input row for applying SuperRes.
  YuvBuffer superres_line_buffer;
  // Buffer used to store the loop restoration borders. This buffer will store 4
  // rows for every 64x64 block (4 rows for every 32x32 for chroma with
  // subsampling). The indices of the rows that are stored are specified in
  // |kLoopRestorationBorderRows|.
  YuvBuffer loop_restoration_border;
  // The size of this dynamic buffer is |tile_rows|.
  DynamicBuffer<IntraPredictionBuffer> intra_prediction_buffers;
  TileScratchBufferPool tile_scratch_buffer_pool;
  ThreadingStrategy threading_strategy;
  std::mutex superblock_row_mutex;
  // The size of this buffer is the number of superblock rows.
  // |superblock_row_progress[i]| is incremented whenever a tile finishes
  // decoding superblock row at index i. If the count reaches tile_columns, then
  // |superblock_row_progress_condvar[i]| is notified.
  DynamicBuffer<int> superblock_row_progress
      LIBGAV1_GUARDED_BY(superblock_row_mutex);
  // The size of this buffer is the number of superblock rows. Used to wait for
  // |superblock_row_progress[i]| to reach tile_columns.
  DynamicBuffer<std::condition_variable> superblock_row_progress_condvar;
  // Used to signal tile decoding failure in the combined multithreading mode.
  bool tile_decoding_failed LIBGAV1_GUARDED_BY(superblock_row_mutex);
};

class FrameScratchBufferPool {
 public:
  std::unique_ptr<FrameScratchBuffer> Get() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!buffers_.Empty()) {
      return buffers_.Pop();
    }
    lock.unlock();
    std::unique_ptr<FrameScratchBuffer> scratch_buffer(new (std::nothrow)
                                                           FrameScratchBuffer);
    return scratch_buffer;
  }

  void Release(std::unique_ptr<FrameScratchBuffer> scratch_buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    buffers_.Push(std::move(scratch_buffer));
  }

 private:
  std::mutex mutex_;
  Stack<std::unique_ptr<FrameScratchBuffer>, kMaxThreads> buffers_
      LIBGAV1_GUARDED_BY(mutex_);
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_FRAME_SCRATCH_BUFFER_H_
