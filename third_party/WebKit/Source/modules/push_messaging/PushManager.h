// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushManager_h
#define PushManager_h

#include "bindings/v8/ScriptWrappable.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExecutionContext;
class ScriptPromise;

class PushManager FINAL : public RefCountedWillBeRefCountedGarbageCollected<PushManager>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<PushManager> create() { return adoptRefWillBeNoop(new PushManager()); }
    virtual ~PushManager();

    ScriptPromise registerPushMessaging(ExecutionContext*, const String& senderId);

private:
    PushManager();
};

} // namespace WebCore

#endif // PushManager_h
