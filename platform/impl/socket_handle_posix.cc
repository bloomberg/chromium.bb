// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/impl/socket_handle_posix.h"

namespace openscreen {
namespace platform {

SocketHandle::SocketHandle(int descriptor) : fd(descriptor) {}

bool SocketHandle::operator==(const SocketHandle& other) const {
  return fd == other.fd;
}

bool SocketHandle::operator!=(const SocketHandle& other) const {
  return !(*this == other);
}
}  // namespace platform
}  // namespace openscreen
