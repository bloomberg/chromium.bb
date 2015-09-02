// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8AsyncCallTracker_h
#define V8AsyncCallTracker_h

#include "bindings/core/v8/ScriptState.h"
#include "core/inspector/V8DebuggerAgent.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScriptState;

class V8AsyncCallTracker final : public NoBaseWillBeGarbageCollectedFinalized<V8AsyncCallTracker>, public ScriptState::Observer, public V8DebuggerAgent::AsyncCallTrackingListener {
    WTF_MAKE_NONCOPYABLE(V8AsyncCallTracker);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(V8AsyncCallTracker);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(V8AsyncCallTracker);
public:
    static PassOwnPtrWillBeRawPtr<V8AsyncCallTracker> create(V8DebuggerAgent* debuggerAgent)
    {
        return adoptPtrWillBeNoop(new V8AsyncCallTracker(debuggerAgent));
    }

    ~V8AsyncCallTracker();
    DECLARE_TRACE();

    // V8DebuggerAgent::AsyncCallTrackingListener implementation:
    void asyncCallTrackingStateChanged(bool tracking) override;
    void resetAsyncOperations() override;

    void didReceiveV8AsyncTaskEvent(ScriptState*, const String& eventType, const String& eventName, int id);

    // ScriptState::Observer implementation:
    void willDisposeScriptState(ScriptState*) override;

private:
    explicit V8AsyncCallTracker(V8DebuggerAgent*);

    void didEnqueueV8AsyncTask(ScriptState*, const String& eventName, int id);
    void willHandleV8AsyncTask(ScriptState*, const String& eventName, int id);

    class V8ContextAsyncOperations;
    WillBeHeapHashMap<RefPtr<ScriptState>, OwnPtrWillBeMember<V8ContextAsyncOperations>> m_contextAsyncOperationMap;
    RawPtrWillBeMember<V8DebuggerAgent> m_debuggerAgent;
};

} // namespace blink

#endif // V8AsyncCallTracker_h
