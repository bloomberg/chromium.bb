// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerAgent_h
#define V8DebuggerAgent_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/v8_inspector/public/V8Debugger.h"

namespace blink {

class V8RuntimeAgent;

class PLATFORM_EXPORT V8DebuggerAgent : public protocol::Backend::Debugger, public V8Debugger::Agent<protocol::Frontend::Debugger> {
public:
    static const char backtraceObjectGroup[];

    static PassOwnPtr<V8DebuggerAgent> create(V8RuntimeAgent*);
    virtual ~V8DebuggerAgent() { }

    // API for the embedder to report native activities.
    virtual void schedulePauseOnNextStatement(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) = 0;
    virtual void cancelPauseOnNextStatement() = 0;
    virtual bool canBreakProgram() = 0;
    virtual void breakProgram(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) = 0;
    virtual void breakProgramOnException(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) = 0;
    virtual void willExecuteScript(int scriptId) = 0;
    virtual void didExecuteScript() = 0;

    virtual bool isPaused() = 0;
    virtual bool enabled() = 0;
    virtual V8Debugger& debugger() = 0;

    // Async call stacks implementation
    static const int unknownAsyncOperationId;
    virtual int traceAsyncOperationStarting(const String16& description) = 0;
    virtual void traceAsyncCallbackStarting(int operationId) = 0;
    virtual void traceAsyncCallbackCompleted() = 0;
    virtual void traceAsyncOperationCompleted(int operationId) = 0;
    virtual bool trackingAsyncCalls() const = 0;
};

} // namespace blink


#endif // V8DebuggerAgent_h
