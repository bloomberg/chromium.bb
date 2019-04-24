// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_SPAN_IMPL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_SPAN_IMPL_H_

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

class QuicStreamSendBuffer;
struct QuicMessageFrame;

// QuicMemSliceSpanImpl wraps a MemSlice span.
class QUIC_EXPORT_PRIVATE QuicMemSliceSpanImpl {
 public:
  QuicMemSliceSpanImpl(const scoped_refptr<net::IOBuffer>* buffers,
                       const int* lengths,
                       size_t num_buffers);

  QuicMemSliceSpanImpl(const QuicMemSliceSpanImpl& other);
  QuicMemSliceSpanImpl& operator=(const QuicMemSliceSpanImpl& other);
  QuicMemSliceSpanImpl(QuicMemSliceSpanImpl&& other);
  QuicMemSliceSpanImpl& operator=(QuicMemSliceSpanImpl&& other);

  ~QuicMemSliceSpanImpl();

  // Save IO buffers in buffers_ to |send_buffer| and returns the length of all
  // saved mem slices.
  QuicByteCount SaveMemSlicesInSendBuffer(QuicStreamSendBuffer* send_buffer);

  // Save data buffers as message data in |message_frame|.
  void SaveMemSlicesAsMessageData(QuicMessageFrame* message_frame);

  QuicStringPiece GetData(size_t index) {
    return QuicStringPiece(buffers_[index]->data(), lengths_[index]);
  }

  QuicByteCount total_length();

  size_t NumSlices() { return num_buffers_; }

  bool empty() const { return num_buffers_ == 0; }

 private:
  const scoped_refptr<net::IOBuffer>* buffers_;
  const int* lengths_;
  // Not const so that the move operator can work properly.
  size_t num_buffers_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_SPAN_IMPL_H_
