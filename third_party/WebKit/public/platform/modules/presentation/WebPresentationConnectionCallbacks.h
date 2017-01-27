// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationConnectionCallbacks_h
#define WebPresentationConnectionCallbacks_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"

namespace blink {

class WebPresentationConnection;
struct WebPresentationError;
struct WebPresentationSessionInfo;

// If session was created, callback's onSuccess() is invoked with the
// information about the presentation session created by the embedder.
// Otherwise, onError() is invoked with the error code and message.
class WebPresentationConnectionCallbacks
    : public WebCallbacks<const WebPresentationSessionInfo&,
                          const WebPresentationError&> {
 public:
  // Returns newly created PresentationConnection object if
  // WebCallbacks::onSuccess() has been called.
  virtual WebPresentationConnection* getConnection() = 0;
};

}  // namespace blink

#endif  // WebPresentationConnectionCallbacks_h
