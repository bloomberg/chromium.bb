// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_SLEEP_IMPL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_SLEEP_IMPL_H_

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "net/third_party/quic/core/quic_time.h"

namespace net {

inline void QuicSleepImpl(QuicTime::Delta duration) {
  base::PlatformThread::Sleep(
      base::TimeDelta::FromMilliseconds(duration.ToMilliseconds()));
}

}  // namespace net

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_SLEEP_IMPL_H_
