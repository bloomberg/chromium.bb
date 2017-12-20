// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushClient_h
#define WebPushClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/push_messaging/WebPushError.h"
#include "public/platform/modules/push_messaging/WebPushProvider.h"
#include <memory>

namespace blink {

class WebServiceWorkerRegistration;
struct WebPushSubscriptionOptions;

class WebPushClient {
 public:
  virtual ~WebPushClient() = default;

  // Ownership of the WebServiceWorkerRegistration is not transferred.
  // Ownership of the callbacks is transferred to the client.
  virtual void Subscribe(WebServiceWorkerRegistration*,
                         const WebPushSubscriptionOptions&,
                         bool user_gesture,
                         std::unique_ptr<WebPushSubscriptionCallbacks>) = 0;
};

}  // namespace blink

#endif  // WebPushClient_h
