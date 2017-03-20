// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationReceiver_h
#define WebPresentationReceiver_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebPresentationConnection;
struct WebPresentationInfo;
enum class WebPresentationConnectionState;

// The delegate Blink provides to WebPresentationReceiverClient in order to get
// updates.
class BLINK_PLATFORM_EXPORT WebPresentationReceiver {
 public:
  virtual ~WebPresentationReceiver() = default;

  // Called when receiver page gets an incoming connection.
  virtual WebPresentationConnection* onReceiverConnectionAvailable(
      const WebPresentationInfo&) = 0;

  // Called when receiver page gets destroyed.
  // TODO: Rename to onReceiverTerminated?
  virtual void didChangeConnectionState(WebPresentationConnectionState) = 0;

  // Called when any PresentationConnection object on receiver page invokes
  // connnection.terminate().
  virtual void terminateConnection() = 0;
};

}  // namespace blink

#endif  // WebPresentationReceiver_h
