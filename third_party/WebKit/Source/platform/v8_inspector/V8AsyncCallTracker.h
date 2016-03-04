// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8AsyncCallTracker_h
#define V8AsyncCallTracker_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/Collections.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"

#include <v8.h>

namespace blink {

class V8AsyncCallTracker final {
    PROTOCOL_DISALLOW_COPY(V8AsyncCallTracker);
public:
    static PassOwnPtr<V8AsyncCallTracker> create(V8DebuggerAgentImpl* debuggerAgent)
    {
        return adoptPtr(new V8AsyncCallTracker(debuggerAgent));
    }

    ~V8AsyncCallTracker();

    void asyncCallTrackingStateChanged(bool tracking);
    void resetAsyncOperations();

    void didReceiveV8AsyncTaskEvent(v8::Local<v8::Context>, const String& eventType, const String& eventName, int id);

    void contextDisposed(int contextId);

private:
    struct Operations {
        protocol::HashMap<String, int> map;
        int contextId;
        V8AsyncCallTracker* target;
        v8::Global<v8::Context> context;
    };

    static void weakCallback(const v8::WeakCallbackInfo<Operations>& data);

    explicit V8AsyncCallTracker(V8DebuggerAgentImpl*);

    void didEnqueueV8AsyncTask(v8::Local<v8::Context>, const String& eventName, int id);
    void willHandleV8AsyncTask(v8::Local<v8::Context>, const String& eventName, int id);
    void completeOperations(const protocol::HashMap<String, int>& contextCallChains);

    V8DebuggerAgentImpl* m_debuggerAgent;
    protocol::HashMap<int, OwnPtr<Operations>> m_idToOperations;
};

} // namespace blink

#endif // V8AsyncCallTracker_h
