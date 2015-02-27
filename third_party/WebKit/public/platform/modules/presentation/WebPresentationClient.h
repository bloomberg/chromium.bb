// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationClient_h
#define WebPresentationClient_h

#include "public/platform/WebCallbacks.h"

namespace blink {

class WebPresentationController;
class WebPresentationSessionClient;
struct WebPresentationError;

// If session was created, callback's onSuccess() is invoked with the information about the
// presentation session created by the embedder. Otherwise, onError() is invoked with the error code
// and message.
using WebPresentationSessionClientCallbacks = WebCallbacks<WebPresentationSessionClient, WebPresentationError>;

// The implementation the embedder has to provide for the Presentation API to work.
class WebPresentationClient {
public:
    virtual ~WebPresentationClient() { }

    // Passes the Blink-side delegate to the embedder.
    virtual void setController(WebPresentationController*) = 0;

    // Called when the frame attaches the first event listener to or removes the
    // last event listener from the |availablechange| event.
    virtual void updateAvailableChangeWatched(bool watched) = 0;
};

} // namespace blink

#endif // WebPresentationClient_h
