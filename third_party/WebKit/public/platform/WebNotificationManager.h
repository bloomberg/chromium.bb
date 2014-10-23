// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNotificationManager_h
#define WebNotificationManager_h

#include "WebNotificationPermission.h"

namespace blink {

struct WebNotificationData;
class WebNotificationDelegate;
class WebSerializedOrigin;

// Provides the services to show platform notifications to the user.
class WebNotificationManager {
public:
    virtual ~WebNotificationManager() { }

    // Shows a notification on the user's system.
    virtual void show(const blink::WebSerializedOrigin&, const WebNotificationData&, WebNotificationDelegate*) = 0;

    // Closes a notification previously shown, and removes it if being shown.
    virtual void close(WebNotificationDelegate*) = 0;

    // Indicates that the delegate object is being destroyed, and must no longer
    // be used by the embedder to dispatch events.
    virtual void notifyDelegateDestroyed(WebNotificationDelegate*) = 0;

    // Synchronously checks the permission level for the given origin.
    virtual WebNotificationPermission checkPermission(const WebSerializedOrigin&) = 0;
};

} // namespace blink

#endif // WebNotificationManager_h
