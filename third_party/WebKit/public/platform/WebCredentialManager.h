// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCredentialManager_h
#define WebCredentialManager_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCredentialManagerError.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

// WebCredentialManager provides an interface for asking Blink's embedder to
// respond to `navigator.credentials.*` calls.
class WebCredentialManager {
public:
    typedef WebCallbacks<WebCredential, WebCredentialManagerError> RequestCallbacks;
    typedef WebCallbacks<void, WebCredentialManagerError> NotificationCallbacks;

    WebCredentialManager() { }
    virtual ~WebCredentialManager() { }

    // Ownership of the callback is transferred to the callee for each of the following methods.
    virtual void dispatchFailedSignIn(const WebCredential&, NotificationCallbacks*) { }
    virtual void dispatchSignedIn(const WebCredential&, NotificationCallbacks*) { }
    virtual void dispatchSignedOut(NotificationCallbacks*) { }
    virtual void dispatchRequest(bool zeroClickOnly, const WebVector<WebURL>& federations, RequestCallbacks*) { }
};

} // namespace blink

#endif // WebCredentialManager_h
