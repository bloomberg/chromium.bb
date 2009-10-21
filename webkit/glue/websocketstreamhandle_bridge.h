// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_BRIDGE_H_
#define WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_BRIDGE_H_

#include <vector>

#include "base/basictypes.h"

class GURL;

namespace WebKit {
class WebSocketStreamHandle;
}

namespace webkit_glue {

class WebSocketStreamHandleDelegate;

class WebSocketStreamHandleBridge {
 public:
  virtual ~WebSocketStreamHandleBridge() {}

  static WebSocketStreamHandleBridge* Create(
      WebKit::WebSocketStreamHandle* handle,
      WebSocketStreamHandleDelegate* delegate);

  virtual void Connect(const GURL& url) = 0;

  virtual bool Send(const std::vector<char>& data) = 0;

  virtual void Close() = 0;

 protected:
  WebSocketStreamHandleBridge() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamHandleBridge);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_BRIDGE_H_
