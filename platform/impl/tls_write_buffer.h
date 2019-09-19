// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_WRITE_BUFFER_H_
#define PLATFORM_IMPL_TLS_WRITE_BUFFER_H_

#include <memory>

#include "absl/types/span.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

// This class is responsible for buffering TLS Write data. The approach taken by
// this class is to allow for a single thread to act as a publisher of data and
// for a separate thread to act as the consumer of that data. The data in
// question is written to a lockless FIFO queue. Whenever the capacity of the
// underlying array changes, Observer::NotifyWriteBufferFill() will be called.
class TlsWriteBuffer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Signals that the write buffer has reached some percentage of being
    // filled. NOTE: This method may be called from multiple threads.
    virtual void NotifyWriteBufferFill(double fraction) = 0;
  };

  explicit TlsWriteBuffer(Observer* observer);

  ~TlsWriteBuffer();

  // Writes as much of the provided data as possible in the buffer, returning
  // the number of bytes written.
  size_t Write(const void* data, size_t len);

  // Returns a subset of the readable region of data. At time of reading, more
  // data may be available for reading than what is represented in this Span.
  absl::Span<const uint8_t> GetReadableRegion();

  // Marks the provided number of bytes as consumed by the consumer thread.
  void Consume(size_t byte_count);

  // The amount of space to allocate in the buffer.
  static constexpr size_t kBufferSizeBytes = 1 << 20;  // 1 MB space.

 private:
  // Signals that the write buffer has reached some percentage of being filled.
  void NotifyWriteBufferFill(double fraction);

  Observer* const observer_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsWriteBuffer);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_WRITE_BUFFER_H_
