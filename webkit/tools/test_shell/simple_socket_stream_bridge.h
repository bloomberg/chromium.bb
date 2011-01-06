// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_

namespace net {
class URLRequestContext;
}  // namespace net

class SimpleSocketStreamBridge {
 public:
  static void InitializeOnIOThread(net::URLRequestContext* request_context);
  static void Cleanup();
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_
