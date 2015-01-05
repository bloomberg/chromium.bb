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
#include "core/dom/ActiveDOMObject.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class DOMException;
class ExceptionState;
class UnderlyingSource;

class ReadableStream : public GarbageCollectedFinalized<ReadableStream>, public ScriptWrappable, public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    enum State {
        Readable,
        Waiting,
        Closed,
        Errored,
    };

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
    ScriptPromise ready(ScriptState*);
    ScriptPromise cancel(ScriptState*, ScriptValue reason);
    ScriptPromise closed(ScriptState*);

    void close();
    void error(PassRefPtrWillBeRawPtr<DOMException>);

    void didSourceStart();

    bool hasPendingActivity() const override;
    virtual void trace(Visitor*);

protected:
    bool enqueuePreliminaryCheck();
    bool enqueuePostAction();
    void readPreliminaryCheck(ExceptionState&);
    void readPostAction();

private:
    typedef ScriptPromiseProperty<Member<ReadableStream>, ToV8UndefinedGenerator, RefPtrWillBeMember<DOMException> > WaitPromise;
    typedef ScriptPromiseProperty<Member<ReadableStream>, ToV8UndefinedGenerator, RefPtrWillBeMember<DOMException> > ClosedPromise;

    virtual bool isQueueEmpty() const = 0;
    virtual void clearQueue() = 0;
    // This function will call ReadableStream::error on error.
    virtual bool shouldApplyBackpressure() = 0;

    void callPullIfNeeded();

    Member<UnderlyingSource> m_source;
    bool m_isStarted;
    bool m_isDraining;
    bool m_isPulling;
    State m_state;

    Member<WaitPromise> m_ready;
    Member<ClosedPromise> m_closed;
    RefPtrWillBeMember<DOMException> m_exception;
};

} // namespace blink

#endif // ReadableStream_h
