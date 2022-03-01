// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"

#include "net/socket/stream_socket.h"

namespace net {
namespace test_server {

void EmbeddedTestServerConnectionListener::OnResponseCompletedSuccessfully(
    std::unique_ptr<StreamSocket> socket) {}

}  // namespace test_server
}  // namespace net
