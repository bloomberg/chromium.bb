// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_errors.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(WebSocketErrorToNetErrorTest, ResultsAreCorrect) {
  EXPECT_EQ(OK, WebSocketErrorToNetError(WEB_SOCKET_OK));
  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            WebSocketErrorToNetError(WEB_SOCKET_ERR_PROTOCOL_ERROR));
  EXPECT_EQ(ERR_MSG_TOO_BIG,
            WebSocketErrorToNetError(WEB_SOCKET_ERR_MESSAGE_TOO_BIG));
}

}  // namespace
}  // namespace net
