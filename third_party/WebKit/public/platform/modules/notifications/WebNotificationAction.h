// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNotificationAction_h
#define WebNotificationAction_h

#include "public/platform/WebString.h"

namespace blink {

// Structure representing the data associated with a Web Notification action.
struct WebNotificationAction {
    // Empty and copy constructor only for WebVector.
    WebNotificationAction() = default;
    WebNotificationAction(const WebNotificationAction&) = default;

    WebNotificationAction(const WebString& action, const WebString& title)
        : action(action)
        , title(title)
    {
    }

    WebString action;
    WebString title;
};

} // namespace blink

#endif // WebNotificationAction_h
