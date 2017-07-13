// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationController_h
#define WebPresentationController_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebPresentationConnection;
struct WebPresentationInfo;
class WebString;

enum class WebPresentationConnectionCloseReason {
  kError = 0,
  kClosed,
  kWentAway
};

enum class WebPresentationConnectionState {
  kConnecting = 0,
  kConnected,
  kClosed,
  kTerminated,
};

// The delegate Blink provides to WebPresentationClient in order to get updates.
class BLINK_PLATFORM_EXPORT WebPresentationController {
 public:
  virtual ~WebPresentationController() {}

  // Called when the presentation is started using the default presentation URL
  // and id.
  virtual WebPresentationConnection* DidStartDefaultPresentation(
      const WebPresentationInfo&) = 0;

  // Called when the state of a presentation connection changes.
  virtual void DidChangeConnectionState(const WebPresentationInfo&,
                                        WebPresentationConnectionState) = 0;

  // Called when a connection closes.
  virtual void DidCloseConnection(const WebPresentationInfo&,
                                  WebPresentationConnectionCloseReason,
                                  const WebString& message) = 0;
};

}  // namespace blink

#endif  // WebPresentationController_h
