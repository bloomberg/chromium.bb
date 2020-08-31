// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/threading_strategy.h"

#include <algorithm>
#include <cassert>

#include "src/utils/constants.h"
#include "src/utils/logging.h"

namespace libgav1 {

bool ThreadingStrategy::Reset(const ObuFrameHeader& frame_header,
                              int thread_count) {
  assert(thread_count > 0);
  if (thread_count == 1) {
    thread_pool_.reset(nullptr);
    tile_thread_count_ = 0;
    max_tile_index_for_row_threads_ = 0;
    return true;
  }

  // We do work in the current thread, so it is sufficient to create
  // |thread_count|-1 threads in the threadpool.
  thread_count = std::min(thread_count, static_cast<int>(kMaxThreads)) - 1;

  if (thread_pool_ == nullptr || thread_pool_->num_threads() != thread_count) {
    thread_pool_ = ThreadPool::Create("libgav1", thread_count);
    if (thread_pool_ == nullptr) {
      LIBGAV1_DLOG(ERROR, "Failed to create a thread pool with %d threads.",
                   thread_count);
      tile_thread_count_ = 0;
      max_tile_index_for_row_threads_ = 0;
      return false;
    }
  }

  // Prefer tile threads first (but only if there is more than one tile).
  const int tile_count = frame_header.tile_info.tile_count;
  if (tile_count > 1) {
    // We want 1 + tile_thread_count_ <= tile_count because the current thread
    // is also used to decode tiles. This is equivalent to
    // tile_thread_count_ <= tile_count - 1.
    tile_thread_count_ = std::min(thread_count, tile_count - 1);
    thread_count -= tile_thread_count_;
    if (thread_count == 0) {
      max_tile_index_for_row_threads_ = 0;
      return true;
    }
  } else {
    tile_thread_count_ = 0;
  }

#if defined(__ANDROID__)
  // Assign the remaining threads for each Tile. The heuristic used here is that
  // we will assign two threads for each Tile. So for example, if |thread_count|
  // is 2, for a stream with 2 tiles the first tile would get both the threads
  // and the second tile would have row multi-threading turned off. This
  // heuristic is based on the fact that row multi-threading is fast enough only
  // when there are at least two threads to do the decoding (since one thread
  // always does the parsing).
  //
  // This heuristic might stop working when SIMD optimizations make the decoding
  // much faster and the parsing thread is only as fast as the decoding threads.
  // So we will have to revisit this later to make sure that this is still
  // optimal.
  //
  // Note that while this heuristic significantly improves performance on high
  // end devices (like the Pixel 3), there are some performance regressions in
  // some lower end devices (in some cases) and that needs to be revisited as we
  // bring in more optimizations. Overall, the gains because of this heuristic
  // seems to be much larger than the regressions.
  for (int i = 0; i < tile_count; ++i) {
    max_tile_index_for_row_threads_ = i + 1;
    thread_count -= 2;
    if (thread_count <= 0) break;
  }
#else   // !defined(__ANDROID__)
  // Assign the remaining threads to each Tile.
  for (int i = 0; i < tile_count; ++i) {
    const int count = thread_count / tile_count +
                      static_cast<int>(i < thread_count % tile_count);
    if (count == 0) {
      // Once we see a 0 value, all subsequent values will be 0 since it is
      // supposed to be assigned in a round-robin fashion.
      break;
    }
    max_tile_index_for_row_threads_ = i + 1;
  }
#endif  // defined(__ANDROID__)
  return true;
}

bool InitializeThreadPoolsForFrameParallel(
    int thread_count, std::unique_ptr<ThreadPool>* const frame_thread_pool) {
  *frame_thread_pool = ThreadPool::Create(thread_count);
  if (*frame_thread_pool == nullptr) {
    LIBGAV1_DLOG(ERROR, "Failed to create frame thread pool with %d threads.",
                 thread_count);
    return false;
  }
  return true;
}

}  // namespace libgav1
