// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_POSIX_SOCKET_H_
#define PLATFORM_POSIX_SOCKET_H_

namespace openscreen {
namespace platform {

struct UdpSocketIPv4Private {
  const int fd;
};

struct UdpSocketIPv6Private {
  const int fd;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_POSIX_SOCKET_H_
