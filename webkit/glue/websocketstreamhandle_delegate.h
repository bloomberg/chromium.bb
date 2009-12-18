// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_DELEGATE_H_
#define WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_DELEGATE_H_

class GURL;

namespace WebKit {
class WebSocketStreamHandle;
}

namespace webkit_glue {

class WebSocketStreamHandleDelegate {
 public:
  WebSocketStreamHandleDelegate() {}
  virtual ~WebSocketStreamHandleDelegate() {}

  virtual void WillOpenStream(WebKit::WebSocketStreamHandle* handle,
                              const GURL& url) {}
  virtual void WillSendData(WebKit::WebSocketStreamHandle* handle,
                            const char* data, int len) {}

  virtual void DidOpenStream(WebKit::WebSocketStreamHandle* handle,
                             int max_amount_send_allowed) {}
  virtual void DidSendData(WebKit::WebSocketStreamHandle* handle,
                           int amount_sent) {}
  virtual void DidReceiveData(WebKit::WebSocketStreamHandle* handle,
                              const char* data, int len) {}
  virtual void DidClose(WebKit::WebSocketStreamHandle*) {}
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_DELEGATE_H_
