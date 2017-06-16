// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushProvider_h
#define WebPushProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/push_messaging/WebPushPermissionStatus.h"
#include "public/platform/modules/push_messaging/WebPushSubscription.h"

#include <memory>

namespace blink {

class WebServiceWorkerRegistration;
struct WebPushError;
struct WebPushSubscriptionOptions;

using WebPushSubscriptionCallbacks =
    WebCallbacks<std::unique_ptr<WebPushSubscription>, const WebPushError&>;
using WebPushPermissionStatusCallbacks =
    WebCallbacks<WebPushPermissionStatus, const WebPushError&>;
using WebPushUnsubscribeCallbacks = WebCallbacks<bool, const WebPushError&>;

class WebPushProvider {
 public:
  virtual ~WebPushProvider() {}

  // Takes ownership of the WebPushSubscriptionCallbacks.
  // Does not take ownership of the WebServiceWorkerRegistration.
  virtual void Subscribe(WebServiceWorkerRegistration*,
                         const WebPushSubscriptionOptions&,
                         bool user_gesture,
                         std::unique_ptr<WebPushSubscriptionCallbacks>) = 0;

  // Takes ownership of the WebPushSubscriptionCallbacks.
  // Does not take ownership of the WebServiceWorkerRegistration.
  virtual void GetSubscription(
      WebServiceWorkerRegistration*,
      std::unique_ptr<WebPushSubscriptionCallbacks>) = 0;

  // Takes ownership of the WebPushPermissionStatusCallbacks.
  // Does not take ownership of the WebServiceWorkerRegistration.
  virtual void GetPermissionStatus(
      WebServiceWorkerRegistration*,
      const WebPushSubscriptionOptions&,
      std::unique_ptr<WebPushPermissionStatusCallbacks>) = 0;

  // Takes ownership if the WebPushUnsubscribeCallbacks.
  // Does not take ownership of the WebServiceWorkerRegistration.
  virtual void Unsubscribe(WebServiceWorkerRegistration*,
                           std::unique_ptr<WebPushUnsubscribeCallbacks>) = 0;
};

}  // namespace blink

#endif  // WebPushProvider_h
