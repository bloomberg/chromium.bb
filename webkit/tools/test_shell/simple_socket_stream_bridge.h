// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_
#define WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_

class URLRequestContext;

class SimpleSocketStreamBridge {
 public:
  static void InitializeOnIOThread(URLRequestContext* request_context);
  static void Cleanup();
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_SIMPLE_SOCKET_STREAM_BRIDGE_H_
