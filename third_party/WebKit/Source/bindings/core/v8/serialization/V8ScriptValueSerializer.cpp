// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"

#include "bindings/core/v8/ToV8.h"
#include "core/dom/DOMArrayBufferBase.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

V8ScriptValueSerializer::V8ScriptValueSerializer(RefPtr<ScriptState> scriptState)
    : m_scriptState(std::move(scriptState))
    , m_serializedScriptValue(SerializedScriptValue::create())
    , m_serializer(m_scriptState->isolate())
{
    DCHECK(RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled());
}

RefPtr<SerializedScriptValue> V8ScriptValueSerializer::serialize(v8::Local<v8::Value> value, Transferables* transferables, ExceptionState& exceptionState)
{
#if DCHECK_IS_ON()
    DCHECK(!m_serializeInvoked);
    m_serializeInvoked = true;
#endif
    DCHECK(m_serializedScriptValue);
    ScriptState::Scope scope(m_scriptState.get());

    // Prepare to transfer the provided transferables.
    transfer(transferables, exceptionState);
    if (exceptionState.hadException()) {
        return nullptr;
    }

    // Serialize the value and handle errors.
    v8::TryCatch tryCatch(m_scriptState->isolate());
    m_serializer.WriteHeader();
    bool wroteValue;
    if (!m_serializer.WriteValue(m_scriptState->context(), value).To(&wroteValue)) {
        DCHECK(tryCatch.HasCaught());
        exceptionState.rethrowV8Exception(tryCatch.Exception());
        return nullptr;
    }
    if (!wroteValue) {
        DCHECK(!tryCatch.HasCaught());
        exceptionState.throwDOMException(DataCloneError, "An object could not be cloned.");
        return nullptr;
    }

    // Finalize the results.
    std::vector<uint8_t> buffer = m_serializer.ReleaseBuffer();
    // Currently, the output must be padded to a multiple of two bytes.
    // TODO(jbroman): Remove this old conversion to WTF::String.
    if (buffer.size() % 2)
        buffer.push_back(0);
    m_serializedScriptValue->setData(
        String(reinterpret_cast<const UChar*>(&buffer[0]), buffer.size() / 2));
    return std::move(m_serializedScriptValue);
}

void V8ScriptValueSerializer::transfer(Transferables* transferables, ExceptionState& exceptionState)
{
    if (!transferables)
        return;
    v8::Isolate* isolate = m_scriptState->isolate();
    v8::Local<v8::Context> context = m_scriptState->context();

    // Transfer array buffers.
    m_serializedScriptValue->transferArrayBuffers(isolate, transferables->arrayBuffers, exceptionState);
    if (exceptionState.hadException())
        return;
    for (uint32_t i = 0; i < transferables->arrayBuffers.size(); i++) {
        DOMArrayBufferBase* arrayBuffer = transferables->arrayBuffers[i].get();
        v8::Local<v8::Value> wrapper = toV8(arrayBuffer, context->Global(), isolate);
        if (wrapper->IsArrayBuffer()) {
            m_serializer.TransferArrayBuffer(
                i, v8::Local<v8::ArrayBuffer>::Cast(wrapper));
        } else if (wrapper->IsSharedArrayBuffer()) {
            m_serializer.TransferSharedArrayBuffer(
                i, v8::Local<v8::SharedArrayBuffer>::Cast(wrapper));
        } else {
            NOTREACHED() << "Unknown type of array buffer in transfer list.";
        }
    }
}

} // namespace blink
