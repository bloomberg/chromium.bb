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

#include <cstdint>
#include <memory>
#include <mutex>  // NOLINT (unapproved c++11 header)

#include "src/loop_filter_mask.h"
#include "src/loop_restoration_info.h"
#include "src/residual_buffer_pool.h"
#include "src/symbol_decoder_context.h"
#include "src/threading_strategy.h"
#include "src/tile_scratch_buffer.h"
#include "src/utils/array_2d.h"
#include "src/utils/constants.h"
#include "src/utils/dynamic_buffer.h"
#include "src/utils/memory.h"
#include "src/utils/stack.h"
#include "src/utils/types.h"
#include "src/yuv_buffer.h"

namespace libgav1 {

// Buffer to facilitate decoding a frame. This struct is used only within
// DecoderImpl::DecodeTiles().
struct FrameScratchBuffer {
  LoopFilterMask loop_filter_mask;
  LoopRestorationInfo loop_restoration_info;
  Array2D<int16_t> cdef_index;
  Array2D<TransformSize> inter_transform_sizes;
  TemporalMotionField motion_field;
  SymbolDecoderContext symbol_decoder_context;
  std::unique_ptr<ResidualBufferPool> residual_buffer_pool;
  Array2D<SuperBlockState> superblock_state;
  // threaded_window_buffer will be subdivided by PostFilter into windows of
  // width 512 pixels. Each row in the window is filtered by a worker thread.
  // To avoid false sharing, each 512-pixel row processed by one thread should
  // not share a cache line with a row processed by another thread. So we align
  // threaded_window_buffer to the cache line size. In addition, it is faster to
  // memcpy from an aligned buffer.
  AlignedDynamicBuffer<uint8_t, kCacheLineSize> threaded_window_buffer;
  // Buffer used to temporarily store the input row for applying SuperRes.
  AlignedDynamicBuffer<uint8_t, 16> superres_line_buffer;
  // Buffer used to store the deblocked pixels that are necessary for loop
  // restoration. This buffer will store 4 rows for every 64x64 block (4 rows
  // for every 32x32 for chroma with subsampling). The indices of the rows that
  // are stored are specified in |kDeblockedRowsForLoopRestoration|.
  YuvBuffer deblock_buffer;
  TileScratchBufferPool tile_scratch_buffer_pool;
  // TODO(vigneshv): This is part of the frame scratch buffer for now. This will
  // have to change or move to DecoderImpl when frame parallel mode with
  // in-frame multi-theading is implemented.
  ThreadingStrategy threading_strategy;
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
  // TODO(b/142583029): The size of this stack is set to kMaxThreads. This may
  // have to be revisited as we iterate over the frame parallel design.
  Stack<std::unique_ptr<FrameScratchBuffer>, kMaxThreads> buffers_
      LIBGAV1_GUARDED_BY(mutex_);
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_FRAME_SCRATCH_BUFFER_H_
