// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushSubscription_h
#define WebPushSubscription_h

#include "public/platform/WebString.h"

namespace blink {

struct WebPushSubscription {
    WebPushSubscription(const WebString& endpoint, const WebString& subscriptionId)
        : endpoint(endpoint)
        , subscriptionId(subscriptionId)
    {
    }

    WebString endpoint;
    WebString subscriptionId;
};

} // namespace blink

#endif // WebPushSubscription_h
