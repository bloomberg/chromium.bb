/*
 * Copyright 2019 The libgav1 Authors
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

#ifndef LIBGAV1_SRC_THREADING_STRATEGY_H_
#define LIBGAV1_SRC_THREADING_STRATEGY_H_

#include <memory>

#include "src/obu_parser.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/threadpool.h"

namespace libgav1 {

// This class allocates and manages the worker threads among thread pools used
// for multi-threaded decoding.
class ThreadingStrategy {
 public:
  ThreadingStrategy() = default;

  // Not copyable or movable.
  ThreadingStrategy(const ThreadingStrategy&) = delete;
  ThreadingStrategy& operator=(const ThreadingStrategy&) = delete;

  // Creates or re-allocates the thread pools based on the |frame_header| and
  // |thread_count|. This function is idempotent if the |frame_header| and
  // |thread_count| doesn't change between calls (it will only create new
  // threads on the first call and do nothing on the subsequent calls). This
  // function also starts the worker threads whenever it creates new thread
  // pools.
  // The following strategy is used to allocate threads:
  //   * One thread is allocated for decoding each Tile.
  //   * Any remaining threads are allocated for superblock row multi-threading
  //     within each of the tile in a round robin fashion.
  LIBGAV1_MUST_USE_RESULT bool Reset(const ObuFrameHeader& frame_header,
                                     int thread_count);

  // Returns a pointer to the ThreadPool that is to be used for Tile
  // multi-threading.
  ThreadPool* tile_thread_pool() const {
    return (tile_thread_count_ != 0) ? thread_pool_.get() : nullptr;
  }

  int tile_thread_count() const { return tile_thread_count_; }

  // Returns a pointer to the ThreadPool that is to be used within the Tile at
  // index |tile_index| for superblock row multi-threading.
  ThreadPool* row_thread_pool(int tile_index) const {
    return tile_index < max_tile_index_for_row_threads_ ? thread_pool_.get()
                                                        : nullptr;
  }

  // Returns a pointer to the ThreadPool that is to be used for post filter
  // multi-threading.
  ThreadPool* post_filter_thread_pool() const { return thread_pool_.get(); }

  // Returns a pointer to the ThreadPool that is to be used for film grain
  // synthesis and blending.
  ThreadPool* film_grain_thread_pool() const { return thread_pool_.get(); }

 private:
  std::unique_ptr<ThreadPool> thread_pool_;
  int tile_thread_count_;
  int max_tile_index_for_row_threads_;
};

LIBGAV1_MUST_USE_RESULT bool InitializeThreadPoolsForFrameParallel(
    int thread_count, std::unique_ptr<ThreadPool>* frame_thread_pool);

}  // namespace libgav1

#endif  // LIBGAV1_SRC_THREADING_STRATEGY_H_
