// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_PLATFORM_API_QUIC_SLEEP_H_
#define QUICHE_QUIC_PLATFORM_API_QUIC_SLEEP_H_

#include "quic/core/quic_time.h"
// TODO(b/178613777): move into the common QUICHE platform.
#include "quiche_platform_impl/quiche_sleep_impl.h"

namespace quic {

inline void QuicSleep(QuicTime::Delta duration) {
  QuicSleepImpl(duration);
}

}  // namespace quic

#endif  // QUICHE_QUIC_PLATFORM_API_QUIC_SLEEP_H_
