// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushClient_h
#define WebPushClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPushError.h"
#include "public/platform/WebString.h"

namespace blink {

class WebServiceWorkerProvider;
struct WebPushRegistration;

typedef WebCallbacks<WebPushRegistration, WebPushError> WebPushRegistrationCallbacks;

class WebPushClient {
public:
    virtual ~WebPushClient() { }

    // Ownership of the WebPushRegistrationCallbacks is transferred to the
    // client. Ownership of the WebServiceWorkerProvider is not transferred.
    virtual void registerPushMessaging(const WebString& senderId, WebPushRegistrationCallbacks*, WebServiceWorkerProvider*) { }
};

} // namespace blink

#endif // WebPushClient_h
