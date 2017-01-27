// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationConnection_h
#define WebPresentationConnection_h

#include "public/platform/WebCommon.h"

#include <memory>

namespace blink {

enum class WebPresentationConnectionState;
class WebPresentationConnectionProxy;
class WebString;

// Implemented by the embedder for a PresentationConnection.
class WebPresentationConnection {
 public:
  virtual ~WebPresentationConnection() = default;

  // Takes ownership of |proxy| and stores it in connection object. Should be
  // called only once.
  virtual void bindProxy(std::unique_ptr<WebPresentationConnectionProxy>) = 0;

  // Notifies the presentation about new message.
  virtual void didReceiveTextMessage(const WebString& message) = 0;
  virtual void didReceiveBinaryMessage(const uint8_t* data, size_t length) = 0;

  // Notifies the connection about its state change.
  virtual void didChangeState(WebPresentationConnectionState) = 0;
};

}  // namespace blink

#endif  // WebPresentationConnection_h
