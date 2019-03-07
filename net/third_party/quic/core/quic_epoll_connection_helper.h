// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The epoll-specific helper for QuicConnection which uses
// EpollAlarm for alarms, and used an int fd_ for writing data.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_EPOLL_CONNECTION_HELPER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_EPOLL_CONNECTION_HELPER_H_

#include <sys/types.h>
#include <set>

#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_default_packet_writer.h"
#include "net/third_party/quic/core/quic_packet_writer.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/core/quic_time.h"
#include "net/third_party/quic/platform/api/quic_default_buffer_allocator.h"
#include "net/third_party/quic/platform/api/quic_epoll.h"
#include "net/third_party/quic/platform/impl/quic_epoll_clock.h"

namespace quic {

class QuicRandom;

enum class QuicAllocator { SIMPLE, BUFFER_POOL };

class QuicEpollConnectionHelper : public QuicConnectionHelperInterface {
 public:
  QuicEpollConnectionHelper(QuicEpollServer* eps, QuicAllocator allocator);
  QuicEpollConnectionHelper(const QuicEpollConnectionHelper&) = delete;
  QuicEpollConnectionHelper& operator=(const QuicEpollConnectionHelper&) =
      delete;
  ~QuicEpollConnectionHelper() override;

  // QuicConnectionHelperInterface
  const QuicClock* GetClock() const override;
  QuicRandom* GetRandomGenerator() override;
  QuicBufferAllocator* GetStreamSendBufferAllocator() override;

 private:
  const QuicEpollClock clock_;
  QuicRandom* random_generator_;
  // Set up allocators.  They take up minimal memory before use.
  // Allocator for stream send buffers.
  QuicDefaultBufferAllocator stream_buffer_allocator_;
  SimpleBufferAllocator simple_buffer_allocator_;
  QuicAllocator allocator_type_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_EPOLL_CONNECTION_HELPER_H_
