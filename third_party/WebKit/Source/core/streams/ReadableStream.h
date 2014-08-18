// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStream_h
#define ReadableStream_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class DOMException;
class ExceptionState;
class UnderlyingSource;

class ReadableStream : public GarbageCollectedFinalized<ReadableStream>, public ScriptWrappable {
public:
    enum State {
        Readable,
        Waiting,
        Closed,
        Errored,
    };

    // FIXME: Define Strategy here.
    // FIXME: Add |strategy| constructor parameter.
    // After ReadableStream construction, |didSourceStart| must be called when
    // |source| initialization succeeds and |error| must be called when
    // |source| initialization fails.
    ReadableStream(ExecutionContext*, UnderlyingSource* /* source */);
    virtual ~ReadableStream();

    bool isStarted() const { return m_isStarted; }
    bool isDraining() const { return m_isDraining; }
    bool isPulling() const { return m_isPulling; }
    State state() const { return m_state; }
    String stateString() const;

    virtual ScriptValue read(ScriptState*, ExceptionState&) = 0;
    ScriptPromise wait(ScriptState*);
    ScriptPromise cancel(ScriptState*, ScriptValue reason);
    ScriptPromise closed(ScriptState*);

    void close();
    void error(PassRefPtrWillBeRawPtr<DOMException>);

    void didSourceStart();

    virtual void trace(Visitor*);

protected:
    bool enqueuePreliminaryCheck(size_t chunkSize);
    bool enqueuePostAction(size_t totalQueueSize);
    void readPreliminaryCheck(ExceptionState&);
    void readPostAction();

private:
    typedef ScriptPromiseProperty<Member<ReadableStream>, V8UndefinedType, RefPtrWillBeMember<DOMException> > WaitPromise;
    typedef ScriptPromiseProperty<Member<ReadableStream>, V8UndefinedType, RefPtrWillBeMember<DOMException> > ClosedPromise;

    virtual bool isQueueEmpty() const = 0;
    virtual void clearQueue() = 0;

    void callOrSchedulePull();

    Member<UnderlyingSource> m_source;
    bool m_isStarted;
    bool m_isDraining;
    bool m_isPulling;
    bool m_isSchedulingPull;
    State m_state;

    Member<WaitPromise> m_wait;
    Member<ClosedPromise> m_closed;
    RefPtrWillBeMember<DOMException> m_exception;
};

} // namespace blink

#endif // ReadableStream_h

