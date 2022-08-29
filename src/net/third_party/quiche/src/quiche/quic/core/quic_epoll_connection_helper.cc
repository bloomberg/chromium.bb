// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_epoll_connection_helper.h"

#include <errno.h>
#include <sys/socket.h>

#include "quiche/quic/core/crypto/quic_random.h"

namespace quic {

QuicEpollConnectionHelper::QuicEpollConnectionHelper(
    QuicEpollServer* epoll_server, QuicAllocator allocator_type)
    : clock_(epoll_server),
      random_generator_(QuicRandom::GetInstance()),
      allocator_type_(allocator_type) {}

QuicEpollConnectionHelper::~QuicEpollConnectionHelper() = default;

const QuicClock* QuicEpollConnectionHelper::GetClock() const { return &clock_; }

QuicRandom* QuicEpollConnectionHelper::GetRandomGenerator() {
  return random_generator_;
}

quiche::QuicheBufferAllocator*
QuicEpollConnectionHelper::GetStreamSendBufferAllocator() {
  if (allocator_type_ == QuicAllocator::BUFFER_POOL) {
    return &stream_buffer_allocator_;
  } else {
    QUICHE_DCHECK(allocator_type_ == QuicAllocator::SIMPLE);
    return &simple_buffer_allocator_;
  }
}

}  // namespace quic
