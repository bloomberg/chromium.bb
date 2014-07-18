// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationPermissionClientImpl_h
#define NotificationPermissionClientImpl_h

#include "modules/notifications/NotificationPermissionClient.h"

namespace blink {

class NotificationPermissionClientImpl : public NoBaseWillBeGarbageCollectedFinalized<NotificationPermissionClientImpl>, public blink::NotificationPermissionClient {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NotificationPermissionClientImpl);
public:
    static PassOwnPtrWillBeRawPtr<NotificationPermissionClientImpl> create();

    virtual ~NotificationPermissionClientImpl();

    // NotificationPermissionClient implementation.
    virtual void requestPermission(blink::ExecutionContext*, PassOwnPtr<blink::NotificationPermissionCallback>) OVERRIDE;

    // NoBaseWillBeGarbageCollectedFinalized implementation.
    virtual void trace(blink::Visitor* visitor) OVERRIDE { blink::NotificationPermissionClient::trace(visitor); }

private:
    NotificationPermissionClientImpl();
};

} // namespace blink

#endif // NotificationPermissionClientImpl_h
