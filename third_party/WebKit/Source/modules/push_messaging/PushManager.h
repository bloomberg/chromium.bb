// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushManager_h
#define PushManager_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScriptPromise;
class ScriptState;

class PushManager final : public GarbageCollected<PushManager>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PushManager* create()
    {
        return new PushManager();
    }

    ScriptPromise registerPushMessaging(ScriptState*);
    ScriptPromise hasPermission(ScriptState*);

    void trace(Visitor*) { }

private:
    PushManager();
};

} // namespace blink

#endif // PushManager_h
