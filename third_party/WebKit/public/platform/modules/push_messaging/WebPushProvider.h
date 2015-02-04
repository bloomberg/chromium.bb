// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushProvider_h
#define WebPushProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"
#include "public/platform/modules/push_messaging/WebPushPermissionStatus.h"

namespace blink {

class WebServiceWorkerRegistration;
struct WebPushError;
struct WebPushSubscription;

using WebPushSubscriptionCallbacks = WebCallbacks<WebPushSubscription, WebPushError>;
// FIXME: Remove when no longer used by the embedder - https://crbug.com/446883.
using WebPushRegistrationCallbacks = WebPushSubscriptionCallbacks;
using WebPushPermissionStatusCallbacks = WebCallbacks<WebPushPermissionStatus, void>;
using WebPushUnsubscribeCallbacks = WebCallbacks<bool, WebPushError>;
// FIXME: Remove when no longer used by the embedder - https://crbug.com/446883.
using WebPushUnregisterCallbacks = WebPushUnsubscribeCallbacks;

class WebPushProvider {
public:
    virtual ~WebPushProvider() { }

    // Takes ownership of the WebPushRegistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void registerPushMessaging(WebServiceWorkerRegistration*, WebPushRegistrationCallbacks*) { BLINK_ASSERT_NOT_REACHED(); }

    // Takes ownership of the WebPushRegistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getRegistration(WebServiceWorkerRegistration*, WebPushRegistrationCallbacks*) { BLINK_ASSERT_NOT_REACHED(); }

    // Takes ownership of the WebPushPermissionStatusCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getPermissionStatus(WebServiceWorkerRegistration*, WebPushPermissionStatusCallbacks*) { BLINK_ASSERT_NOT_REACHED(); }

    // Takes ownership if the WebPushUnregisterCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void unregister(WebServiceWorkerRegistration*, WebPushUnregisterCallbacks* callback) { BLINK_ASSERT_NOT_REACHED(); }
};

} // namespace blink

#endif // WebPushProvider_h
