// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushSubscriptionOptions_h
#define WebPushSubscriptionOptions_h

#include "public/platform/WebString.h"

namespace blink {

struct WebPushSubscriptionOptions {
  WebPushSubscriptionOptions() : user_visible_only(false) {}

  // Indicates that the subscription will only be used for push messages
  // that result in UI visible to the user.
  bool user_visible_only;

  // P-256 public key, in uncompressed form, of the app server that can send
  // push messages to this subscription.
  // TODO(johnme): Make this a WebVector<uint8_t>.
  WebString application_server_key;
};

}  // namespace blink

#endif  // WebPushSubscriptionOptions_h
