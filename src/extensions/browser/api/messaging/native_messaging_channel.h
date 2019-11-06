// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_

#include <memory>

#include "base/callback.h"

namespace base {
class Value;
}  // namespace base

namespace extensions {

// An interface to receive and send messages between a native component and
// chrome.
class NativeMessagingChannel {
 public:
  // Callback interface for the channel. EventHandler must outlive
  // NativeMessagingChannel.
  class EventHandler {
   public:
    // Called when a message is received from the other endpoint.
    virtual void OnMessage(std::unique_ptr<base::Value> message) = 0;

    // Called when the channel is disconnected.
    // EventHandler is guaranteed not to be called after OnDisconnect().
    virtual void OnDisconnect() = 0;

    virtual ~EventHandler() {}
  };

  virtual ~NativeMessagingChannel() {}

  // Starts reading and processing messages.
  virtual void Start(EventHandler* event_handler) = 0;

  // Sends a message to the other endpoint.
  virtual void SendMessage(std::unique_ptr<base::Value> message) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_
