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
class ExclusiveStreamReader;
class UnderlyingSource;

class ReadableStream : public GarbageCollectedFinalized<ReadableStream>, public ScriptWrappable, public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ReadableStream);
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
    State stateInternal() const { return m_state; }
    DOMException* storedException() { return m_exception.get(); }

    // |stateString|, |read| and |ready| are affected by an exclusive lock. Use
    // |stateInternal|, |readInternal| and |readyInternal| if you want to avoid
    // that.
    String stateString() const;
    ScriptValue read(ScriptState*, ExceptionState&);
    ScriptPromise ready(ScriptState*);
    ScriptPromise cancel(ScriptState*, ScriptValue reason);
    ScriptPromise closed(ScriptState*);

    virtual ScriptValue readInternal(ScriptState*, ExceptionState&) = 0;
    ScriptPromise readyInternal(ScriptState*);

    void close();
    void error(PassRefPtrWillBeRawPtr<DOMException>);

    void didSourceStart();

    // This function is not a getter. It creates an ExclusiveStreamReader and
    // returns it.
    ExclusiveStreamReader* getReader(ExceptionState&);
    // Only ExclusiveStreamReader methods should call this function.
    void setReader(ExclusiveStreamReader*);

    bool isLocked() const { return m_reader; }
    bool isLockedTo(const ExclusiveStreamReader* reader) const { return m_reader == reader; }

    bool hasPendingActivity() const override;
    void stop() override;
    DECLARE_VIRTUAL_TRACE();

    // Returns the string representation of the given State.
    static String stateToString(State);

protected:
    bool enqueuePreliminaryCheck();
    bool enqueuePostAction();
    void readInternalPreliminaryCheck(ExceptionState&);
    void readInternalPostAction();

private:
    using WaitPromise = ScriptPromiseProperty<Member<ReadableStream>, ToV8UndefinedGenerator, RefPtrWillBeMember<DOMException>>;
    using ClosedPromise = ScriptPromiseProperty<Member<ReadableStream>, ToV8UndefinedGenerator, RefPtrWillBeMember<DOMException>>;

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
    Member<ExclusiveStreamReader> m_reader;
};

} // namespace blink

#endif // ReadableStream_h
