// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_

#include <string>

namespace net {

// Given a WebSocket handshake challenge, compute the correct response.
// TODO(ricea): There should probably be a test for this.
void ComputeSecWebSocketAccept(const std::string& key,
                               std::string* accept);

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_HANDLER_H_
