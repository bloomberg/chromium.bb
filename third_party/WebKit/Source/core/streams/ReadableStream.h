// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStream_h
#define ReadableStream_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class ExceptionState;
class UnderlyingSource;

class ReadableStream FINAL : public GarbageCollectedFinalized<ReadableStream> {
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

    template <typename T>
    bool enqueue(const T& chunk)
    {
        ScriptState::Scope scope(m_scriptState.get());
        return enqueueInternal(ScriptValue(m_scriptState.get(), V8ValueTraits<T>::toV8Value(chunk)));
    }

    bool isStarted() const { return m_isStarted; }
    bool isDraining() const { return m_isDraining; }
    bool isPulling() const { return m_isPulling; }
    State state() const { return m_state; }

    // FIXME: Implement these APIs.
    // bool read();
    // ScriptPromise wait();
    // void close();

    void error(ScriptValue error);

    void trace(Visitor*);

private:
    class OnStarted;
    class OnStartFailed;

    void onStarted(void);
    bool enqueueInternal(ScriptValue chunk);

    RefPtr<ScriptState> m_scriptState;
    Member<UnderlyingSource> m_source;
    bool m_isStarted;
    bool m_isDraining;
    bool m_isPulling;
    State m_state;
};

} // namespace blink

#endif // ReadableStream_h

