// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8AsyncCallTracker_h
#define V8AsyncCallTracker_h

#include "bindings/core/v8/ScriptState.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace blink {

class InspectorDebuggerAgent;
class ScriptState;

class V8AsyncCallTracker final : public NoBaseWillBeGarbageCollectedFinalized<V8AsyncCallTracker>, public ScriptState::Observer, public InspectorDebuggerAgent::AsyncCallTrackingListener {
    WTF_MAKE_NONCOPYABLE(V8AsyncCallTracker);
public:
    explicit V8AsyncCallTracker(InspectorDebuggerAgent*);
    ~V8AsyncCallTracker();
    void trace(Visitor*);

    // InspectorDebuggerAgent::AsyncCallTrackingListener implementation:
    void asyncCallTrackingStateChanged(bool tracking) override;
    void resetAsyncCallChains() override;

    void didReceiveV8AsyncTaskEvent(ScriptState*, const String& eventType, const String& eventName, int id);

    // ScriptState::Observer implementation:
    void willDisposeScriptState(ScriptState*) override;

private:
    void didEnqueueV8AsyncTask(ScriptState*, const String& eventName, int id);
    void willHandleV8AsyncTask(ScriptState*, const String& eventName, int id);

    class V8ContextAsyncCallChains;
    using V8ContextAsyncChainMap = WillBeHeapHashMap<ScriptState*, OwnPtrWillBeMember<V8ContextAsyncCallChains> >;
    V8ContextAsyncChainMap m_contextAsyncCallChainMap;
    RawPtrWillBeMember<InspectorDebuggerAgent> m_debuggerAgent;
};

} // namespace blink

#endif // V8AsyncCallTracker_h
