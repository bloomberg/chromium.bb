// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"

#include "bindings/core/v8/ToV8.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMSharedArrayBuffer.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

V8ScriptValueDeserializer::V8ScriptValueDeserializer(RefPtr<ScriptState> scriptState, RefPtr<SerializedScriptValue> serializedScriptValue)
    : m_scriptState(std::move(scriptState))
    , m_serializedScriptValue(std::move(serializedScriptValue))
    , m_deserializer(
        m_scriptState->isolate(),
        reinterpret_cast<const uint8_t*>(
            m_serializedScriptValue->data().ensure16Bit(),
            m_serializedScriptValue->data().characters16()),
        m_serializedScriptValue->data().length() * 2)
{
    DCHECK(RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled());
    m_deserializer.SetSupportsLegacyWireFormat(true);
}

v8::Local<v8::Value> V8ScriptValueDeserializer::deserialize()
{
#if DCHECK_IS_ON()
    DCHECK(!m_deserializeInvoked);
    m_deserializeInvoked = true;
#endif

    v8::Isolate* isolate = m_scriptState->isolate();
    v8::EscapableHandleScope scope(isolate);
    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::Context> context = m_scriptState->context();

    bool readHeader;
    if (!m_deserializer.ReadHeader().To(&readHeader))
        return v8::Null(isolate);
    DCHECK(readHeader);
    m_version = m_deserializer.GetWireFormatVersion();

    // Prepare to transfer the provided transferables.
    transfer();

    v8::Local<v8::Value> value;
    if (!m_deserializer.ReadValue(context).ToLocal(&value))
        return v8::Null(isolate);
    return scope.Escape(value);
}

void V8ScriptValueDeserializer::transfer()
{
    v8::Isolate* isolate = m_scriptState->isolate();
    v8::Local<v8::Context> context = m_scriptState->context();
    v8::Local<v8::Object> creationContext = context->Global();

    // Transfer array buffers.
    if (auto* arrayBufferContents = m_serializedScriptValue->getArrayBufferContentsArray()) {
        for (unsigned i = 0; i < arrayBufferContents->size(); i++) {
            WTF::ArrayBufferContents& contents = arrayBufferContents->at(i);
            if (contents.isShared()) {
                DOMSharedArrayBuffer* arrayBuffer = DOMSharedArrayBuffer::create(contents);
                v8::Local<v8::Value> wrapper = toV8(arrayBuffer, creationContext, isolate);
                DCHECK(wrapper->IsSharedArrayBuffer());
                m_deserializer.TransferSharedArrayBuffer(
                    i, v8::Local<v8::SharedArrayBuffer>::Cast(wrapper));
            } else {
                DOMArrayBuffer* arrayBuffer = DOMArrayBuffer::create(contents);
                v8::Local<v8::Value> wrapper = toV8(arrayBuffer, creationContext, isolate);
                DCHECK(wrapper->IsArrayBuffer());
                m_deserializer.TransferArrayBuffer(i, v8::Local<v8::ArrayBuffer>::Cast(wrapper));
            }
        }
    }
}

} // namespace blink
