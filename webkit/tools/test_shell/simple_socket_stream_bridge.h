// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_

#include "base/basictypes.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace WebKit {
class WebSocketStreamHandle;
}  // namespace WebKit

namespace webkit_glue {
class WebSocketStreamHandleDelegate;
class WebSocketStreamHandleBridge;
}  // namespace webkit_glue

class SimpleSocketStreamBridge {
 public:
  static void InitializeOnIOThread(net::URLRequestContext* request_context);
  static void Cleanup();
  static webkit_glue::WebSocketStreamHandleBridge* Create(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SimpleSocketStreamBridge);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_
