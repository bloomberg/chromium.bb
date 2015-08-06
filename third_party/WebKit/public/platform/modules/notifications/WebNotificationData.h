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
        DirectionLeftToRight,
        DirectionRightToLeft,
        DirectionAuto
    };

    WebNotificationData()
        : direction(DirectionLeftToRight)
        , silent(false)
    {
    }

    WebNotificationData(const WebString& title, Direction direction, const WebString& lang, const WebString& body, const WebString& tag, const WebURL& icon, const WebVector<int>& vibrate, bool silent, const WebVector<char>& data, const WebVector<WebNotificationAction>& actions)
        : title(title)
        , direction(direction)
        , lang(lang)
        , body(body)
        , tag(tag)
        , icon(icon)
        , vibrate(vibrate)
        , silent(silent)
        , data(data)
        , actions(actions)
    {
    }

    WebString title;
    Direction direction;
    WebString lang;
    WebString body;
    WebString tag;
    WebURL icon;
    WebVector<int> vibrate;
    bool silent;
    WebVector<char> data;
    WebVector<WebNotificationAction> actions;
};

} // namespace blink

#endif // WebNotificationData_h
