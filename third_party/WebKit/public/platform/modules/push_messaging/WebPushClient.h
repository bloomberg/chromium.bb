// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushClient_h
#define WebPushClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/push_messaging/WebPushError.h"

namespace blink {

class WebServiceWorkerRegistration;
struct WebPushSubscription;

using WebPushSubscriptionCallbacks = WebCallbacks<WebPushSubscription, WebPushError>;
// FIXME: Remove when no longer used by the embedder - https://crbug.com/446883.
using WebPushRegistrationCallbacks = WebPushSubscriptionCallbacks;

class WebPushClient {
public:
    virtual ~WebPushClient() { }

    // Ownership of the WebServiceWorkerRegistration is not transferred.
    // Ownership of the callbacks is transferred to the client.
    virtual void registerPushMessaging(WebServiceWorkerRegistration*, WebPushRegistrationCallbacks*) { BLINK_ASSERT_NOT_REACHED(); }
};

} // namespace blink

#endif // WebPushClient_h
