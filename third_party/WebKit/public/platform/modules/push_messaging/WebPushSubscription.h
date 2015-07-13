// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushSubscription_h
#define WebPushSubscription_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"

namespace blink {

struct WebPushSubscription {
    // The |endpoint| must be unique for each subscription.
    WebPushSubscription(const WebURL& endpoint, const WebVector<unsigned char>& curve25519dh)
        : endpoint(endpoint)
        , curve25519dh(curve25519dh)
    {
    }

    // TODO(peter): Remove this constructor when the embedder doesn't use it anymore.
    explicit WebPushSubscription(const WebURL& endpoint)
        : endpoint(endpoint)
    {
    }

    WebURL endpoint;
    WebVector<unsigned char> curve25519dh;
};

} // namespace blink

#endif // WebPushSubscription_h
