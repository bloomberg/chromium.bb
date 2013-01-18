// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_errors.h"

#include "base/logging.h"

namespace net {

Error WebSocketErrorToNetError(WebSocketError error) {
  switch (error) {
    case WEB_SOCKET_OK:
      return OK;
    case WEB_SOCKET_ERR_PROTOCOL_ERROR:
      return ERR_WS_PROTOCOL_ERROR;
    case WEB_SOCKET_ERR_MESSAGE_TOO_BIG:
      return ERR_MSG_TOO_BIG;
    default:
      NOTREACHED();
      return ERR_UNEXPECTED;
  }
}

}  // namespace net
