// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushSubscription_h
#define WebPushSubscription_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"

namespace blink {

struct WebPushSubscription {
    // The |endpoint| and |p256dh| must both be unique for each subscription.
    WebPushSubscription(const WebURL& endpoint, const WebVector<unsigned char>& p256dh)
        : endpoint(endpoint)
        , p256dh(p256dh)
    {
    }

    WebURL endpoint;
    WebVector<unsigned char> p256dh;
};

} // namespace blink

#endif // WebPushSubscription_h
