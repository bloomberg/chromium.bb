// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPermissionClient_h
#define PushPermissionClient_h

#include "platform/Supplementable.h"
#include "public/platform/WebCallback.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class ExecutionContext;
class LocalFrame;

class PushPermissionClient : public WillBeHeapSupplement<LocalFrame> {
public:
    virtual ~PushPermissionClient() { }

    // Requests user permission to use the Push API from the origin of the
    // current frame. The provided callback will be run when a decision has been
    // made.
    virtual void requestPermission(ExecutionContext*, WebCallback*) = 0;

    // WillBeHeapSupplement requirements.
    static const char* supplementName();
    static PushPermissionClient* from(ExecutionContext*);
};

void providePushPermissionClientTo(LocalFrame&, PassOwnPtrWillBeRawPtr<PushPermissionClient>);

} // namespace blink

#endif // PushPermissionClient_h
