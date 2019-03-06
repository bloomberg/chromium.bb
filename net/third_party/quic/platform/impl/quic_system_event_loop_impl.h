// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_SYSTEM_EVENT_LOOP_IMPL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_SYSTEM_EVENT_LOOP_IMPL_H_

#include "base/run_loop.h"

inline void QuicRunSystemEventLoopIterationImpl() {
  base::RunLoop().RunUntilIdle();
}

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_SYSTEM_EVENT_LOOP_IMPL_H_
