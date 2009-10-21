// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_IMPL_H_
#define WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_IMPL_H_

#include "base/ref_counted.h"
#include "webkit/api/public/WebSocketStreamHandle.h"

namespace webkit_glue {

class WebSocketStreamHandleImpl : public WebKit::WebSocketStreamHandle {
 public:
  WebSocketStreamHandleImpl();
  virtual ~WebSocketStreamHandleImpl();

  // WebSocketStreamHandle methods:
  virtual void connect(
      const WebKit::WebURL& url,
      WebKit::WebSocketStreamHandleClient* client);
  virtual bool send(const WebKit::WebData& data);
  virtual void close();

 private:
  class Context;
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamHandleImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBSOCKETSTREAMHANDLE_IMPL_H_
