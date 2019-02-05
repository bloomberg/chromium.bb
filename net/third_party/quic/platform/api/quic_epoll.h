// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A toy server, which listens on a specified address for QUIC traffic and
// handles incoming responses.
//
// Note that this server is intended to verify correctness of the client and is
// in no way expected to be performant.
#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_EPOLL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_EPOLL_H_

#include "net/third_party/quic/platform/impl/quic_epoll_impl.h"

namespace quic {

using QuicEpollServer = QuicEpollServerImpl;
using QuicEpollEvent = QuicEpollEventImpl;
using QuicEpollAlarmBase = QuicEpollAlarmBaseImpl;
using QuicEpollCallbackInterface = QuicEpollCallbackInterfaceImpl;

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_EPOLL_H_
