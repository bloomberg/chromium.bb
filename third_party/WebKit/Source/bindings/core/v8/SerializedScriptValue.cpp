/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/core/v8/SerializedScriptValue.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValueSerializer.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/ExceptionCode.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferContents.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/Assertions.h"
#include "wtf/ByteOrder.h"
#include "wtf/Uint8ClampedArray.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/StringUTF8Adaptor.h"

namespace blink {

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(v8::Handle<v8::Value> value, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, ExceptionState& exceptionState, v8::Isolate* isolate)
{
    return adoptRef(new SerializedScriptValue(value, messagePorts, arrayBuffers, 0, exceptionState, isolate));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::createAndSwallowExceptions(v8::Isolate* isolate, v8::Handle<v8::Value> value)
{
    TrackExceptionState exceptionState;
    return adoptRef(new SerializedScriptValue(value, 0, 0, 0, exceptionState, isolate));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(const ScriptValue& value, WebBlobInfoArray* blobInfo, ExceptionState& exceptionState, v8::Isolate* isolate)
{
    ASSERT(isolate->InContext());
    return adoptRef(new SerializedScriptValue(value.v8Value(), 0, 0, blobInfo, exceptionState, isolate));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::createFromWire(const String& data)
{
    return adoptRef(new SerializedScriptValue(data));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::createFromWireBytes(const Vector<uint8_t>& data)
{
    // Decode wire data from big endian to host byte order.
    ASSERT(!(data.size() % sizeof(UChar)));
    size_t length = data.size() / sizeof(UChar);
    StringBuffer<UChar> buffer(length);
    const UChar* src = reinterpret_cast<const UChar*>(data.data());
    UChar* dst = buffer.characters();
    for (size_t i = 0; i < length; i++)
        dst[i] = ntohs(src[i]);

    return createFromWire(String::adopt(buffer));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(const String& data)
{
    return create(data, v8::Isolate::GetCurrent());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create(const String& data, v8::Isolate* isolate)
{
    SerializedScriptValueInternal::Writer writer;
    writer.writeWebCoreString(data);
    String wireData = writer.takeWireString();
    return adoptRef(new SerializedScriptValue(wireData));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::create()
{
    return adoptRef(new SerializedScriptValue());
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::nullValue()
{
    SerializedScriptValueInternal::Writer writer;
    writer.writeNull();
    String wireData = writer.takeWireString();
    return adoptRef(new SerializedScriptValue(wireData));
}

// Convert serialized string to big endian wire data.
void SerializedScriptValue::toWireBytes(Vector<char>& result) const
{
    ASSERT(result.isEmpty());
    size_t length = m_data.length();
    result.resize(length * sizeof(UChar));
    UChar* dst = reinterpret_cast<UChar*>(result.data());

    if (m_data.is8Bit()) {
        const LChar* src = m_data.characters8();
        for (size_t i = 0; i < length; i++)
            dst[i] = htons(static_cast<UChar>(src[i]));
    } else {
        const UChar* src = m_data.characters16();
        for (size_t i = 0; i < length; i++)
            dst[i] = htons(src[i]);
    }
}

SerializedScriptValue::SerializedScriptValue()
    : m_externallyAllocatedMemory(0)
{
}

static void neuterArrayBufferInAllWorlds(DOMArrayBuffer* object)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isMainThread()) {
        Vector<RefPtr<DOMWrapperWorld> > worlds;
        DOMWrapperWorld::allWorldsInMainThread(worlds);
        for (size_t i = 0; i < worlds.size(); i++) {
            v8::Handle<v8::Object> wrapper = worlds[i]->domDataStore().get(object, isolate);
            if (!wrapper.IsEmpty()) {
                ASSERT(wrapper->IsArrayBuffer());
                v8::Handle<v8::ArrayBuffer>::Cast(wrapper)->Neuter();
            }
        }
    } else {
        v8::Handle<v8::Object> wrapper = DOMWrapperWorld::current(isolate).domDataStore().get(object, isolate);
        if (!wrapper.IsEmpty()) {
            ASSERT(wrapper->IsArrayBuffer());
            v8::Handle<v8::ArrayBuffer>::Cast(wrapper)->Neuter();
        }
    }
}

PassOwnPtr<SerializedScriptValue::ArrayBufferContentsArray> SerializedScriptValue::transferArrayBuffers(v8::Isolate* isolate, ArrayBufferArray& arrayBuffers, ExceptionState& exceptionState)
{
    ASSERT(arrayBuffers.size());

    for (size_t i = 0; i < arrayBuffers.size(); i++) {
        if (arrayBuffers[i]->isNeutered()) {
            exceptionState.throwDOMException(DataCloneError, "ArrayBuffer at index " + String::number(i) + " is already neutered.");
            return nullptr;
        }
    }

    OwnPtr<ArrayBufferContentsArray> contents = adoptPtr(new ArrayBufferContentsArray(arrayBuffers.size()));

    HashSet<DOMArrayBuffer*> visited;
    for (size_t i = 0; i < arrayBuffers.size(); i++) {
        if (visited.contains(arrayBuffers[i].get()))
            continue;
        visited.add(arrayBuffers[i].get());

        bool result = arrayBuffers[i]->transfer(contents->at(i));
        if (!result) {
            exceptionState.throwDOMException(DataCloneError, "ArrayBuffer at index " + String::number(i) + " could not be transferred.");
            return nullptr;
        }

        neuterArrayBufferInAllWorlds(arrayBuffers[i].get());
    }
    return contents.release();
}

SerializedScriptValue::SerializedScriptValue(v8::Handle<v8::Value> value, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, WebBlobInfoArray* blobInfo, ExceptionState& exceptionState, v8::Isolate* isolate)
    : m_externallyAllocatedMemory(0)
{
    SerializedScriptValueInternal::Writer writer;
    SerializedScriptValueInternal::Serializer::Status status;
    String errorMessage;
    {
        v8::TryCatch tryCatch;
        SerializedScriptValueInternal::Serializer serializer(writer, messagePorts, arrayBuffers, blobInfo, m_blobDataHandles, tryCatch, ScriptState::current(isolate));
        status = serializer.serialize(value);
        if (status == SerializedScriptValueInternal::Serializer::JSException) {
            // If there was a JS exception thrown, re-throw it.
            exceptionState.rethrowV8Exception(tryCatch.Exception());
            return;
        }
        errorMessage = serializer.errorMessage();
    }
    switch (status) {
    case SerializedScriptValueInternal::Serializer::InputError:
    case SerializedScriptValueInternal::Serializer::DataCloneError:
        exceptionState.throwDOMException(DataCloneError, errorMessage);
        return;
    case SerializedScriptValueInternal::Serializer::Success:
        m_data = writer.takeWireString();
        ASSERT(m_data.impl()->hasOneRef());
        if (arrayBuffers && arrayBuffers->size())
            m_arrayBufferContentsArray = transferArrayBuffers(isolate, *arrayBuffers, exceptionState);
        return;
    case SerializedScriptValueInternal::Serializer::JSException:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
}

SerializedScriptValue::SerializedScriptValue(const String& wireData)
    : m_externallyAllocatedMemory(0)
{
    m_data = wireData.isolatedCopy();
}

v8::Handle<v8::Value> SerializedScriptValue::deserialize(MessagePortArray* messagePorts)
{
    return deserialize(v8::Isolate::GetCurrent(), messagePorts, 0);
}

v8::Handle<v8::Value> SerializedScriptValue::deserialize(v8::Isolate* isolate, MessagePortArray* messagePorts, const WebBlobInfoArray* blobInfo)
{
    if (!m_data.impl())
        return v8::Null(isolate);
    COMPILE_ASSERT(sizeof(SerializedScriptValueInternal::Writer::BufferValueType) == 2, BufferValueTypeIsTwoBytes);
    m_data.ensure16Bit();
    // FIXME: SerializedScriptValue shouldn't use String for its underlying
    // storage. Instead, it should use SharedBuffer or Vector<uint8_t>. The
    // information stored in m_data isn't even encoded in UTF-16. Instead,
    // unicode characters are encoded as UTF-8 with two code units per UChar.
    SerializedScriptValueInternal::Reader reader(reinterpret_cast<const uint8_t*>(m_data.impl()->characters16()), 2 * m_data.length(), blobInfo, m_blobDataHandles, ScriptState::current(isolate));
    SerializedScriptValueInternal::Deserializer deserializer(reader, messagePorts, m_arrayBufferContentsArray.get());

    // deserialize() can run arbitrary script (e.g., setters), which could result in |this| being destroyed.
    // Holding a RefPtr ensures we are alive (along with our internal data) throughout the operation.
    RefPtr<SerializedScriptValue> protect(this);
    return deserializer.deserialize();
}

bool SerializedScriptValue::extractTransferables(v8::Isolate* isolate, v8::Local<v8::Value> value, int argumentIndex, MessagePortArray& ports, ArrayBufferArray& arrayBuffers, ExceptionState& exceptionState)
{
    if (isUndefinedOrNull(value)) {
        ports.resize(0);
        arrayBuffers.resize(0);
        return true;
    }

    uint32_t length = 0;
    if (value->IsArray()) {
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);
        length = array->Length();
    } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
        if (!exceptionState.hadException())
            exceptionState.throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex + 1));
        return false;
    }

    v8::Local<v8::Object> transferrables = v8::Local<v8::Object>::Cast(value);

    // Validate the passed array of transferrables.
    for (unsigned i = 0; i < length; ++i) {
        v8::Local<v8::Value> transferrable = transferrables->Get(i);
        // Validation of non-null objects, per HTML5 spec 10.3.3.
        if (isUndefinedOrNull(transferrable)) {
            exceptionState.throwDOMException(DataCloneError, "Value at index " + String::number(i) + " is an untransferable " + (transferrable->IsUndefined() ? "'undefined'" : "'null'") + " value.");
            return false;
        }
        // Validation of Objects implementing an interface, per WebIDL spec 4.1.15.
        if (V8MessagePort::hasInstance(transferrable, isolate)) {
            RefPtrWillBeRawPtr<MessagePort> port = V8MessagePort::toImpl(v8::Handle<v8::Object>::Cast(transferrable));
            // Check for duplicate MessagePorts.
            if (ports.contains(port)) {
                exceptionState.throwDOMException(DataCloneError, "Message port at index " + String::number(i) + " is a duplicate of an earlier port.");
                return false;
            }
            ports.append(port.release());
        } else if (V8ArrayBuffer::hasInstance(transferrable, isolate)) {
            RefPtr<DOMArrayBuffer> arrayBuffer = V8ArrayBuffer::toImpl(v8::Handle<v8::Object>::Cast(transferrable));
            if (arrayBuffers.contains(arrayBuffer)) {
                exceptionState.throwDOMException(DataCloneError, "ArrayBuffer at index " + String::number(i) + " is a duplicate of an earlier ArrayBuffer.");
                return false;
            }
            arrayBuffers.append(arrayBuffer.release());
        } else {
            exceptionState.throwDOMException(DataCloneError, "Value at index " + String::number(i) + " does not have a transferable type.");
            return false;
        }
    }
    return true;
}

void SerializedScriptValue::registerMemoryAllocatedWithCurrentScriptContext()
{
    if (m_externallyAllocatedMemory)
        return;
    m_externallyAllocatedMemory = static_cast<intptr_t>(m_data.length());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(m_externallyAllocatedMemory);
}

SerializedScriptValue::~SerializedScriptValue()
{
    // If the allocated memory was not registered before, then this class is likely
    // used in a context other then Worker's onmessage environment and the presence of
    // current v8 context is not guaranteed. Avoid calling v8 then.
    if (m_externallyAllocatedMemory) {
        ASSERT(v8::Isolate::GetCurrent());
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-m_externallyAllocatedMemory);
    }
}

} // namespace blink
