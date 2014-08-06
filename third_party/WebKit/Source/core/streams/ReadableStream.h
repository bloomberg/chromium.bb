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

class ReadableStream FINAL : public GarbageCollectedFinalized<ReadableStream>, public ScriptWrappable, public ContextLifecycleObserver {
public:
    enum State {
        Readable,
        Waiting,
        Closed,
        Errored,
    };

    // FIXME: Define Strategy here.
    // FIXME: Add |strategy| constructor parameter.
    ReadableStream(ScriptState*, UnderlyingSource*, ExceptionState*);
    virtual ~ReadableStream();

    bool isStarted() const { return m_isStarted; }
    bool isDraining() const { return m_isDraining; }
    bool isPulling() const { return m_isPulling; }
    State state() const { return m_state; }

    // FIXME: Implement bool read();
    ScriptPromise wait(ScriptState*);
    // FIXME: Implement void close();

    // FIXME: enqueue must accept any type.
    bool enqueue(const String& chunk);
    void close();
    void error(PassRefPtrWillBeRawPtr<DOMException>);

    void trace(Visitor*);

private:
    class OnStarted;
    class OnStartFailed;
    typedef ScriptPromiseProperty<Member<ReadableStream>, V8UndefinedType, RefPtrWillBeMember<DOMException> > WaitPromise;

    void onStarted(void);
    void enqueueValueWithSize(const String& chunk);

    void callOrSchedulePull();

    Member<UnderlyingSource> m_source;
    bool m_isStarted;
    bool m_isDraining;
    bool m_isPulling;
    bool m_isSchedulingPull;
    State m_state;

    Member<WaitPromise> m_wait;
    RefPtrWillBeMember<DOMException> m_exception;
};

} // namespace blink

#endif // ReadableStream_h

