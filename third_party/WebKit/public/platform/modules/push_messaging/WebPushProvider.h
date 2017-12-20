// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushProvider_h
#define WebPushProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/push_messaging/WebPushSubscription.h"

#include <memory>

namespace blink {

class WebServiceWorkerRegistration;
struct WebPushError;
struct WebPushSubscriptionOptions;

using WebPushSubscriptionCallbacks =
    WebCallbacks<std::unique_ptr<WebPushSubscription>, const WebPushError&>;
using WebPushUnsubscribeCallbacks = WebCallbacks<bool, const WebPushError&>;

class WebPushProvider {
 public:
  virtual ~WebPushProvider() = default;

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

  // Takes ownership if the WebPushUnsubscribeCallbacks.
  // Does not take ownership of the WebServiceWorkerRegistration.
  virtual void Unsubscribe(WebServiceWorkerRegistration*,
                           std::unique_ptr<WebPushUnsubscribeCallbacks>) = 0;
};

}  // namespace blink

#endif  // WebPushProvider_h
