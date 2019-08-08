// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PUSH_MESSAGING_WEB_PUSH_SUBSCRIPTION_OPTIONS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PUSH_MESSAGING_WEB_PUSH_SUBSCRIPTION_OPTIONS_H_

#include <string>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Structure to hold the options provided from the web app developer as
// part of asking for a new push subscription.
struct BLINK_COMMON_EXPORT WebPushSubscriptionOptions {
  WebPushSubscriptionOptions() {}
  ~WebPushSubscriptionOptions() {}

  // Whether or not the app developer agrees to provide user visible
  // notifications whenever they receive a push message.
  bool user_visible_only = false;

  // The unique identifier of the application service which is used to
  // verify the push message before delivery. This could either be an ID
  // assigned by the developer console or the app server's public key.
  std::string application_server_key;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PUSH_MESSAGING_WEB_PUSH_SUBSCRIPTION_OPTIONS_H_
