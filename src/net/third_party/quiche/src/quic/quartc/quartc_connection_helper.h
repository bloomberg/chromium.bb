// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_QUARTC_QUARTC_CONNECTION_HELPER_H_
#define QUICHE_QUIC_QUARTC_QUARTC_CONNECTION_HELPER_H_

#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/quic_buffer_allocator.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_clock.h"

namespace quic {

// Simple implementation of QuicConnectionHelperInterface for Quartc.
class QuartcConnectionHelper : public QuicConnectionHelperInterface {
 public:
  explicit QuartcConnectionHelper(const QuicClock* clock);

  // QuicConnectionHelperInterface overrides.
  const QuicClock* GetClock() const override;
  QuicRandom* GetRandomGenerator() override;
  QuicBufferAllocator* GetStreamSendBufferAllocator() override;

 private:
  const QuicClock* clock_;
  SimpleBufferAllocator buffer_allocator_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_QUARTC_QUARTC_CONNECTION_HELPER_H_
