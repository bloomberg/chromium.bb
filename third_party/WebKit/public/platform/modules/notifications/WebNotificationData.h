// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNotificationData_h
#define WebNotificationData_h

#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/notifications/WebNotificationAction.h"

namespace blink {

// Structure representing the data associated with a Web Notification.
struct WebNotificationData {
  enum Direction {
    kDirectionLeftToRight,
    kDirectionRightToLeft,
    kDirectionAuto
  };

  WebString title;
  Direction direction = kDirectionLeftToRight;
  WebString lang;
  WebString body;
  WebString tag;
  WebURL image;
  WebURL icon;
  WebURL badge;
  WebVector<int> vibrate;
  double timestamp = 0;
  bool renotify = false;
  bool silent = false;
  bool require_interaction = false;
  WebVector<char> data;
  WebVector<WebNotificationAction> actions;
};

}  // namespace blink

#endif  // WebNotificationData_h
