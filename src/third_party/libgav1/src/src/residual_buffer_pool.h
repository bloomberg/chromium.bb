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

#ifndef LIBGAV1_SRC_RESIDUAL_BUFFER_POOL_H_
#define LIBGAV1_SRC_RESIDUAL_BUFFER_POOL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>  // NOLINT (unapproved c++11 header)
#include <new>

#include "src/utils/common.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"
#include "src/utils/types.h"

namespace libgav1 {

// A simple fixed size queue implementation to hold the transform parameters
// when |Tile::split_parse_and_decode_| is true. We don't have to do any
// boundary checks since we always push data into the queue before accessing it.
class TransformParameterQueue {
 public:
  TransformParameterQueue() = default;

  // Move only.
  TransformParameterQueue(TransformParameterQueue&& other) = default;
  TransformParameterQueue& operator=(TransformParameterQueue&& other) = default;

  LIBGAV1_MUST_USE_RESULT bool Init(int max_size) {
    max_size_ = max_size;
    // No initialization is necessary since the data will be always written to
    // before being read.
    non_zero_coeff_count_.reset(new (std::nothrow) int16_t[max_size_]);
    tx_type_.reset(new (std::nothrow) TransformType[max_size_]);
    return non_zero_coeff_count_ != nullptr && tx_type_ != nullptr;
  }

  // Adds the |non_zero_coeff_count| and the |tx_type| to the back of the queue.
  void Push(int non_zero_coeff_count, TransformType tx_type) {
    assert(back_ < max_size_);
    non_zero_coeff_count_[back_] = non_zero_coeff_count;
    tx_type_[back_++] = tx_type;
  }

  // Returns the non_zero_coeff_count at the front of the queue.
  int16_t NonZeroCoeffCount() const {
    assert(front_ != back_);
    return non_zero_coeff_count_[front_];
  }

  // Returns the tx_type at the front of the queue.
  TransformType Type() const {
    assert(front_ != back_);
    return tx_type_[front_];
  }

  // Removes the |non_zero_coeff_count| and the |tx_type| from the front of the
  // queue.
  void Pop() {
    assert(front_ != back_);
    ++front_;
  }

  // Clears the queue.
  void Reset() {
    front_ = 0;
    back_ = 0;
  }

  // Used only in the tests. Returns the number of elements in the queue.
  int Size() const { return back_ - front_; }

 private:
  int max_size_ = 0;
  std::unique_ptr<int16_t[]> non_zero_coeff_count_;
  std::unique_ptr<TransformType[]> tx_type_;
  int front_ = 0;
  int back_ = 0;
};

// This class is used for parsing and decoding a superblock. Members of this
// class are populated in the "parse" step and consumed in the "decode" step.
class ResidualBuffer : public Allocable {
 public:
  static std::unique_ptr<ResidualBuffer> Create(size_t buffer_size,
                                                int queue_size) {
    std::unique_ptr<ResidualBuffer> buffer(new (std::nothrow) ResidualBuffer);
    if (buffer != nullptr) {
      buffer->buffer_ = MakeAlignedUniquePtr<uint8_t>(32, buffer_size);
      if (buffer->buffer_ == nullptr ||
          !buffer->transform_parameters_.Init(queue_size)) {
        buffer = nullptr;
      }
    }
    return buffer;
  }

  // Move only.
  ResidualBuffer(ResidualBuffer&& other) = default;
  ResidualBuffer& operator=(ResidualBuffer&& other) = default;

  // Buffer used to store the residual values.
  uint8_t* buffer() { return buffer_.get(); }
  // Queue used to store the transform parameters.
  TransformParameterQueue* transform_parameters() {
    return &transform_parameters_;
  }

 private:
  friend class ResidualBufferStack;

  ResidualBuffer() = default;

  AlignedUniquePtr<uint8_t> buffer_;
  TransformParameterQueue transform_parameters_;
  // Used by ResidualBufferStack to form a chain of ResidualBuffers.
  ResidualBuffer* next_ = nullptr;
};

// A LIFO stack of ResidualBuffers. Owns the buffers in the stack.
class ResidualBufferStack {
 public:
  ResidualBufferStack() = default;

  // Not copyable or movable
  ResidualBufferStack(const ResidualBufferStack&) = delete;
  ResidualBufferStack& operator=(const ResidualBufferStack&) = delete;

  ~ResidualBufferStack();

  // Pushes |buffer| to the top of the stack.
  void Push(std::unique_ptr<ResidualBuffer> buffer);

  // If the stack is non-empty, returns the buffer at the top of the stack and
  // removes it from the stack. If the stack is empty, returns nullptr.
  std::unique_ptr<ResidualBuffer> Pop();

  // Swaps the contents of this stack and |other|.
  void Swap(ResidualBufferStack* other);

  // Returns the number of buffers in the stack.
  size_t Size() const { return num_buffers_; }

 private:
  // A singly-linked list of ResidualBuffers, chained together using the next_
  // field of ResidualBuffer.
  ResidualBuffer* top_ = nullptr;
  size_t num_buffers_ = 0;
};

// Utility class used to manage the residual buffers (and the transform
// parameters) used for multi-threaded decoding. This class uses a stack to
// store the buffers for better cache locality. Since buffers used more recently
// are more likely to be in the cache. All functions in this class are
// thread-safe.
class ResidualBufferPool : public Allocable {
 public:
  ResidualBufferPool(bool use_128x128_superblock, int subsampling_x,
                     int subsampling_y, size_t residual_size);

  // Recomputes |buffer_size_| and invalidates the existing buffers if
  // necessary.
  void Reset(bool use_128x128_superblock, int subsampling_x, int subsampling_y,
             size_t residual_size);
  // Gets a residual buffer. The buffer is guaranteed to be large enough to
  // store the residual values for one superblock whose parameters are the same
  // as the constructor or the last call to Reset(). If there are free buffers
  // in the stack, it returns one from the stack, otherwise a new buffer is
  // allocated.
  std::unique_ptr<ResidualBuffer> Get();
  // Returns the |buffer| back to the pool (by appending it to the stack).
  // Subsequent calls to Get() may re-use this buffer.
  void Release(std::unique_ptr<ResidualBuffer> buffer);

  // Used only in the tests. Returns the number of buffers in the stack.
  size_t Size() const;

 private:
  mutable std::mutex mutex_;
  ResidualBufferStack buffers_ LIBGAV1_GUARDED_BY(mutex_);
  size_t buffer_size_;
  int queue_size_;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_RESIDUAL_BUFFER_POOL_H_
