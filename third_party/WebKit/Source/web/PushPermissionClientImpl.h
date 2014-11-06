// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPermissionClientImpl_h
#define PushPermissionClientImpl_h

#include "modules/push_messaging/PushPermissionClient.h"

namespace blink {

class PushPermissionClientImpl : public NoBaseWillBeGarbageCollectedFinalized<PushPermissionClientImpl>, public PushPermissionClient {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PushPermissionClientImpl);
public:
    static PassOwnPtrWillBeRawPtr<PushPermissionClientImpl> create();

    ~PushPermissionClientImpl() override;

    // PushPermissionClient implementation.
    void requestPermission(ExecutionContext*, WebCallback*) override;

    // NoBaseWillBeGarbageCollectedFinalized implementation.
    void trace(Visitor* visitor) override { PushPermissionClient::trace(visitor); }

private:
    PushPermissionClientImpl();
};

} // namespace blink

#endif // PushPermissionClientImpl_h
