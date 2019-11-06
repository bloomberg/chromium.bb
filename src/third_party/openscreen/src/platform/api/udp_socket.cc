// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket() {
  deletion_callback_ = [](UdpSocket* socket) {};
}

UdpSocket::~UdpSocket() {
  deletion_callback_(this);
}

void UdpSocket::SetDeletionCallback(std::function<void(UdpSocket*)> callback) {
  deletion_callback_ = callback;
}

}  // namespace platform
}  // namespace openscreen
