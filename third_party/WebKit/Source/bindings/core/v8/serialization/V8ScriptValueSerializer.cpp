// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"

#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8ImageData.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "core/dom/DOMArrayBufferBase.h"
#include "core/html/ImageData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/AutoReset.h"
#include "wtf/text/StringUTF8Adaptor.h"

namespace blink {

V8ScriptValueSerializer::V8ScriptValueSerializer(RefPtr<ScriptState> scriptState)
    : m_scriptState(std::move(scriptState))
    , m_serializedScriptValue(SerializedScriptValue::create())
    , m_serializer(m_scriptState->isolate(), this)
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
    AutoReset<const ExceptionState*> reset(&m_exceptionState, &exceptionState);
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
    DCHECK(wroteValue);

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
    m_transferables = transferables;
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

void V8ScriptValueSerializer::writeUTF8String(const String& string)
{
    // TODO(jbroman): Ideally this method would take a WTF::StringView, but the
    // StringUTF8Adaptor trick doesn't yet work with StringView.
    StringUTF8Adaptor utf8(string);
    DCHECK_LT(utf8.length(), std::numeric_limits<uint32_t>::max());
    writeUint32(utf8.length());
    writeRawBytes(utf8.data(), utf8.length());
}

bool V8ScriptValueSerializer::writeDOMObject(ScriptWrappable* wrappable, ExceptionState& exceptionState)
{
    const WrapperTypeInfo* wrapperTypeInfo = wrappable->wrapperTypeInfo();
    if (wrapperTypeInfo == &V8ImageData::wrapperTypeInfo) {
        ImageData* imageData = wrappable->toImpl<ImageData>();
        DOMUint8ClampedArray* pixels = imageData->data();
        writeTag(ImageDataTag);
        writeUint32(imageData->width());
        writeUint32(imageData->height());
        writeUint32(pixels->length());
        writeRawBytes(pixels->data(), pixels->length());
        return true;
    }
    if (wrapperTypeInfo == &V8MessagePort::wrapperTypeInfo) {
        MessagePort* messagePort = wrappable->toImpl<MessagePort>();
        size_t index = kNotFound;
        if (m_transferables)
            index = m_transferables->messagePorts.find(messagePort);
        if (index == kNotFound) {
            exceptionState.throwDOMException(DataCloneError, "A MessagePort could not be cloned because it was not transferred.");
            return false;
        }
        DCHECK_LE(index, std::numeric_limits<uint32_t>::max());
        writeTag(MessagePortTag);
        writeUint32(static_cast<uint32_t>(index));
        return true;
    }
    return false;
}

void V8ScriptValueSerializer::ThrowDataCloneError(v8::Local<v8::String> v8Message)
{
    DCHECK(m_exceptionState);
    String message = m_exceptionState->addExceptionContext(
        v8StringToWebCoreString<String>(v8Message, DoNotExternalize));
    v8::Local<v8::Value> exception = V8ThrowException::createDOMException(
        m_scriptState->isolate(), DataCloneError, message);
    V8ThrowException::throwException(m_scriptState->isolate(), exception);
}

v8::Maybe<bool> V8ScriptValueSerializer::WriteHostObject(v8::Isolate* isolate, v8::Local<v8::Object> object)
{
    DCHECK(m_exceptionState);
    DCHECK_EQ(isolate, m_scriptState->isolate());
    ExceptionState exceptionState(
        isolate, m_exceptionState->context(),
        m_exceptionState->interfaceName(), m_exceptionState->propertyName());

    if (!V8DOMWrapper::isWrapper(isolate, object)) {
        exceptionState.throwDOMException(DataCloneError, "An object could not be cloned.");
        return v8::Nothing<bool>();
    }
    ScriptWrappable* wrappable = toScriptWrappable(object);
    bool wroteDOMObject = writeDOMObject(wrappable, exceptionState);
    if (wroteDOMObject) {
        DCHECK(!exceptionState.hadException());
        return v8::Just(true);
    }
    if (!exceptionState.hadException()) {
        String interface = wrappable->wrapperTypeInfo()->interfaceName;
        exceptionState.throwDOMException(DataCloneError, interface + " object could not be cloned.");
    }
    return v8::Nothing<bool>();
}

} // namespace blink
