// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationConnectionProxy_h
#define WebPresentationConnectionProxy_h

namespace blink {

enum class WebPresentationConnectionState;
class WebString;

// The implementation the embedder has to provide for the Presentation API to
// work. This class is used to send messages to a remote PresentationConnection
// (the "target"), which may be hosted in a different process.
class WebPresentationConnectionProxy {
 public:
  virtual ~WebPresentationConnectionProxy() = default;

  // Closes the target connection.
  virtual void Close() const = 0;

  // Notifies the target connection about connection state change.
  virtual void NotifyTargetConnection(WebPresentationConnectionState) = 0;

  // Sends a text or binary message to the target connection.
  virtual void SendTextMessage(const WebString& message) = 0;
  virtual void SendBinaryMessage(const uint8_t* data, size_t length) = 0;
};

}  // namespace blink

#endif  // WebPresentationConnectionProxy_h
