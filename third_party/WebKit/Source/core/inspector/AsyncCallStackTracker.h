/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AsyncCallStackTracker_h
#define AsyncCallStackTracker_h

#include "bindings/core/v8/ScriptValue.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class Event;
class EventListener;
class EventTarget;
class ExecutionContext;
class ExecutionContextTask;
class FormData;
class HTTPHeaderMap;
class KURL;
class MutationObserver;
class ScriptDebugServer;
class ThreadableLoaderClient;
class XMLHttpRequest;

class AsyncCallStackTracker final : public NoBaseWillBeGarbageCollected<AsyncCallStackTracker> {
    WTF_MAKE_NONCOPYABLE(AsyncCallStackTracker);
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(AsyncCallStackTracker);
public:
    class AsyncCallStack final : public RefCountedWillBeGarbageCollectedFinalized<AsyncCallStack> {
    public:
        AsyncCallStack(const String&, const ScriptValue&);
        ~AsyncCallStack();
        void trace(Visitor*) { }
        String description() const { return m_description; }
        ScriptValue callFrames() const { return m_callFrames; }
    private:
        String m_description;
        ScriptValue m_callFrames;
    };

    typedef WillBeHeapDeque<RefPtrWillBeMember<AsyncCallStack>, 4> AsyncCallStackVector;

    class AsyncCallChain final : public RefCountedWillBeGarbageCollectedFinalized<AsyncCallChain> {
    public:
        explicit AsyncCallChain(int id) : m_id(id) { }
        AsyncCallChain(int id, const AsyncCallChain& t) : m_id(id), m_callStacks(t.m_callStacks) { }
        int asyncOperationId() const { return m_id; }
        AsyncCallStackVector callStacks() const { return m_callStacks; }
        void trace(Visitor*);
    private:
        friend class AsyncCallStackTracker;
        int m_id;
        AsyncCallStackVector m_callStacks;
    };

    AsyncCallStackTracker();

    bool isEnabled() const { return m_maxAsyncCallStackDepth; }
    void setAsyncCallStackDepth(int);
    const AsyncCallChain* currentAsyncCallChain() const;
    void setScriptDebugServer(ScriptDebugServer* server) { m_scriptDebugServer = server; }

    void didInstallTimer(ExecutionContext*, int timerId, int timeout, bool singleShot);
    void didRemoveTimer(ExecutionContext*, int timerId);
    bool willFireTimer(ExecutionContext*, int timerId);
    void didFireTimer() { didFireAsyncCall(); };

    void didRequestAnimationFrame(ExecutionContext*, int callbackId);
    void didCancelAnimationFrame(ExecutionContext*, int callbackId);
    bool willFireAnimationFrame(ExecutionContext*, int callbackId);
    void didFireAnimationFrame() { didFireAsyncCall(); };

    void didEnqueueEvent(EventTarget*, Event*);
    void didRemoveEvent(EventTarget*, Event*);
    void willHandleEvent(EventTarget*, Event*, EventListener*, bool useCapture);
    void didHandleEvent() { didFireAsyncCall(); };

    void willLoadXHR(XMLHttpRequest*, ThreadableLoaderClient*, const AtomicString& method, const KURL&, bool async, PassRefPtr<FormData> body, const HTTPHeaderMap& headers, bool includeCrendentials);
    void didDispatchXHRLoadendEvent(XMLHttpRequest*);

    void didEnqueueMutationRecord(ExecutionContext*, MutationObserver*);
    void didClearAllMutationRecords(ExecutionContext*, MutationObserver*);
    void willDeliverMutationRecords(ExecutionContext*, MutationObserver*);
    void didDeliverMutationRecords() { didFireAsyncCall(); };

    void didPostExecutionContextTask(ExecutionContext*, ExecutionContextTask*);
    void didKillAllExecutionContextTasks(ExecutionContext*);
    void willPerformExecutionContextTask(ExecutionContext*, ExecutionContextTask*);
    void didPerformExecutionContextTask() { didFireAsyncCall(); };

    void didEnqueueV8AsyncTask(ExecutionContext*, const String& eventName, int id, const ScriptValue& callFrames);
    void willHandleV8AsyncTask(ExecutionContext*, const String& eventName, int id);

    int traceAsyncOperationStarting(ExecutionContext*, const String& operationName, int prevOperationId = 0);
    void traceAsyncOperationCompleted(ExecutionContext*, int operationId);
    void traceAsyncOperationCompletedCallbackStarting(ExecutionContext*, int operationId);
    void traceAsyncCallbackStarting(ExecutionContext*, int operationId);
    void traceAsyncCallbackCompleted() { didFireAsyncCall(); };

    void didFireAsyncCall();
    void clear();

    class Listener : public WillBeGarbageCollectedMixin {
    public:
        virtual ~Listener() { }
        virtual void didCreateAsyncCallChain(const AsyncCallChain*) = 0;
        virtual void didChangeCurrentAsyncCallChain(const AsyncCallChain*) = 0;
    };
    void setListener(Listener* listener) { m_listener = listener; }

    void trace(Visitor*);

    class ExecutionContextData;

private:
    void willHandleXHREvent(XMLHttpRequest*, Event*);

    PassRefPtrWillBeRawPtr<AsyncCallChain> createAsyncCallChain(ExecutionContextData*, const String& description, const ScriptValue& callFrames);
    void setCurrentAsyncCallChain(ExecutionContext*, PassRefPtrWillBeRawPtr<AsyncCallChain>);
    void clearCurrentAsyncCallChain();
    static void ensureMaxAsyncCallChainDepth(AsyncCallChain*, unsigned);
    bool validateCallFrames(const ScriptValue& callFrames);
    int circularSequentialId();

    ExecutionContextData* createContextDataIfNeeded(ExecutionContext*);

    int m_circularSequentialId;
    unsigned m_maxAsyncCallStackDepth;
    RefPtrWillBeMember<AsyncCallChain> m_currentAsyncCallChain;
    unsigned m_nestedAsyncCallCount;
    typedef WillBeHeapHashMap<RawPtrWillBeMember<ExecutionContext>, OwnPtrWillBeMember<ExecutionContextData> > ExecutionContextDataMap;
    ExecutionContextDataMap m_executionContextDataMap;
    Listener* m_listener;
    ScriptDebugServer* m_scriptDebugServer;
};

} // namespace blink

#endif // !defined(AsyncCallStackTracker_h)
