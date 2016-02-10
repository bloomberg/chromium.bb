// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNotificationAction_h
#define WebNotificationAction_h

#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

// Structure representing the data associated with a Web Notification action.
struct WebNotificationAction {
    WebString action;
    WebString title;
    WebURL icon;
};

} // namespace blink

#endif // WebNotificationAction_h
