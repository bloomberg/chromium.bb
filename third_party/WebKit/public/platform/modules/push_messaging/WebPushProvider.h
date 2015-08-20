// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushProvider_h
#define WebPushProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPassOwnPtr.h"
#include "public/platform/modules/push_messaging/WebPushError.h"
#include "public/platform/modules/push_messaging/WebPushPermissionStatus.h"
#include "public/platform/modules/push_messaging/WebPushSubscription.h"

namespace blink {

class WebServiceWorkerRegistration;
struct WebPushSubscriptionOptions;

class WebPushSubscriptionCallbacks : public WebCallbacks<WebPassOwnPtr<WebPushSubscription>, const WebPushError&> {
public:
    void onSuccess(WebPushSubscription* r)
    {
        onSuccess(adoptWebPtr(r));
    }
    void onError(WebPushError* e)
    {
        onError(*e);
        delete e;
    }
    void onSuccess(WebPassOwnPtr<WebPushSubscription>) override {}
    void onError(const WebPushError&) override {}
};
class WebPushPermissionStatusCallbacks : public WebCallbacks<WebPushPermissionStatus, const WebPushError&> {
public:
    void onSuccess(WebPushPermissionStatus* r)
    {
        onSuccess(*r);
    }
    void onError(WebPushError* e)
    {
        onError(*e);
        delete e;
    }
    void onSuccess(WebPushPermissionStatus) override {}
    void onError(const WebPushError&) override {}
};
using WebPushUnsubscribeCallbacks = WebCallbacks<bool, const WebPushError&>;

class WebPushProvider {
public:
    virtual ~WebPushProvider() { }

    // Takes ownership of the WebPushSubscriptionCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void subscribe(WebServiceWorkerRegistration*, const WebPushSubscriptionOptions&, WebPushSubscriptionCallbacks*) = 0;

    // Takes ownership of the WebPushSubscriptionCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getSubscription(WebServiceWorkerRegistration*, WebPushSubscriptionCallbacks*) = 0;

    // Takes ownership of the WebPushPermissionStatusCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getPermissionStatus(WebServiceWorkerRegistration*, const WebPushSubscriptionOptions&, WebPushPermissionStatusCallbacks*) = 0;

    // Takes ownership if the WebPushUnsubscribeCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void unsubscribe(WebServiceWorkerRegistration*, WebPushUnsubscribeCallbacks*) = 0;
};

} // namespace blink

#endif // WebPushProvider_h
