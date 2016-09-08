// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ScriptValueSerializer_h
#define V8ScriptValueSerializer_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace blink {

class Transferables;

// Serializes V8 values according to the HTML structured clone algorithm:
// https://html.spec.whatwg.org/multipage/infrastructure.html#structured-clone
//
// Supports only basic JavaScript objects and core DOM types. Support for
// modules types is implemented in a subclass.
//
// A serializer cannot be used multiple times; it is expected that its serialize
// method will be invoked exactly once.
class GC_PLUGIN_IGNORE("https://crbug.com/644725") V8ScriptValueSerializer : public v8::ValueSerializer::Delegate {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(V8ScriptValueSerializer);
public:
    explicit V8ScriptValueSerializer(RefPtr<ScriptState>);
    RefPtr<SerializedScriptValue> serialize(v8::Local<v8::Value>, Transferables*, ExceptionState&);

private:
    void transfer(Transferables*, ExceptionState&);

    // v8::ValueSerializer::Delegate
    void ThrowDataCloneError(v8::Local<v8::String> message) override;

    RefPtr<ScriptState> m_scriptState;
    RefPtr<SerializedScriptValue> m_serializedScriptValue;
    v8::ValueSerializer m_serializer;
    const ExceptionState* m_exceptionState = nullptr;
#if DCHECK_IS_ON()
    bool m_serializeInvoked = false;
#endif
};

} // namespace blink

#endif // V8ScriptValueSerializer_h
