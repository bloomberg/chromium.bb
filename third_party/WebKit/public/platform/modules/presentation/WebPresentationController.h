// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationController_h
#define WebPresentationController_h

#include "public/platform/WebCommon.h"

namespace blink {

struct WebPresentationSessionInfo;
class WebPresentationConnection;
class WebString;

enum class WebPresentationConnectionCloseReason { Error = 0, Closed, WentAway };

enum class WebPresentationConnectionState {
  Connecting = 0,
  Connected,
  Closed,
  Terminated,
};

// The delegate Blink provides to WebPresentationClient in order to get updates.
class BLINK_PLATFORM_EXPORT WebPresentationController {
 public:
  virtual ~WebPresentationController() {}

  // Called when the presentation session is started by the embedder using
  // the default presentation URL and id.
  virtual WebPresentationConnection* didStartDefaultSession(
      const WebPresentationSessionInfo&) = 0;

  // Called when the state of a session changes.
  virtual void didChangeSessionState(const WebPresentationSessionInfo&,
                                     WebPresentationConnectionState) = 0;

  // Called when a connection closes.
  virtual void didCloseConnection(const WebPresentationSessionInfo&,
                                  WebPresentationConnectionCloseReason,
                                  const WebString& message) = 0;

  // Called when a text message of a session is received.
  virtual void didReceiveSessionTextMessage(const WebPresentationSessionInfo&,
                                            const WebString& message) = 0;

  // Called when a binary message of a session is received.
  virtual void didReceiveSessionBinaryMessage(const WebPresentationSessionInfo&,
                                              const uint8_t* data,
                                              size_t length) = 0;
};

}  // namespace blink

#endif  // WebPresentationController_h
