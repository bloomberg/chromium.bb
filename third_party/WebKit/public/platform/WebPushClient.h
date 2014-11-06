// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushClient_h
#define WebPushClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPushError.h"
#include "public/platform/WebPushPermissionStatus.h"

namespace blink {

class WebServiceWorkerProvider;
struct WebPushRegistration;

typedef WebCallbacks<WebPushRegistration, WebPushError> WebPushRegistrationCallbacks;
typedef WebCallbacks<WebPushPermissionStatus, void> WebPushPermissionStatusCallback;

class WebPushClient {
public:
    virtual ~WebPushClient() { }

    // Ownership of the WebPushRegistrationCallbacks is transferred to the
    // client. Ownership of the WebServiceWorkerProvider is not transferred.
    virtual void registerPushMessaging(WebPushRegistrationCallbacks*, WebServiceWorkerProvider*) { }

    // Ownership of the WebPushPermissionStatusCallback is transferred to the
    // client. Ownership of the WebServiceWorkerProvider is not transferred.
    virtual void getPermissionStatus(WebPushPermissionStatusCallback*, WebServiceWorkerProvider*) { }
};

} // namespace blink

#endif // WebPushClient_h
