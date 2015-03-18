// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SuspendableScriptExecutor_h
#define SuspendableScriptExecutor_h

#include "core/frame/SuspendableTimer.h"
#include "platform/heap/Handle.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class LocalFrame;
class ScriptSourceCode;
class WebScriptExecutionCallback;

class SuspendableScriptExecutor final : public RefCountedWillBeRefCountedGarbageCollected<SuspendableScriptExecutor>, public SuspendableTimer {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(SuspendableScriptExecutor);
public:
    static void createAndRun(LocalFrame*, int worldID, const WillBeHeapVector<ScriptSourceCode>& sources, int extensionGroup, bool userGesture, WebScriptExecutionCallback*);
    virtual ~SuspendableScriptExecutor();

    virtual void contextDestroyed() override;

    DECLARE_VIRTUAL_TRACE();

private:
    SuspendableScriptExecutor(LocalFrame*, int worldID, const WillBeHeapVector<ScriptSourceCode>& sources, int extensionGroup, bool userGesture, WebScriptExecutionCallback*);

    virtual void fired() override;

    void run();
    void executeAndDestroySelf();

    RawPtrWillBeMember<LocalFrame> m_frame;
    int m_worldID;
    WillBeHeapVector<ScriptSourceCode> m_sources;
    int m_extensionGroup;
    bool m_userGesture;
    WebScriptExecutionCallback* m_callback;
};

} // namespace blink

#endif // SuspendableScriptExecutor_h
