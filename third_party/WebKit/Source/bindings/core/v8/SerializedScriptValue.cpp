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

#include "bindings/core/v8/SerializedScriptValue.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValueSerializer.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/Transferables.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMSharedArrayBuffer.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/frame/ImageBitmap.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include "wtf/ByteOrder.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringHash.h"

namespace blink {

SerializedScriptValue::SerializedScriptValue()
    : m_externallyAllocatedMemory(0)
{
}

SerializedScriptValue::~SerializedScriptValue()
{
    // If the allocated memory was not registered before, then this class is likely
    // used in a context other than Worker's onmessage environment and the presence of
    // current v8 context is not guaranteed. Avoid calling v8 then.
    if (m_externallyAllocatedMemory) {
        ASSERT(v8::Isolate::GetCurrent());
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-m_externallyAllocatedMemory);
    }
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::nullValue()
{
    SerializedScriptValueWriter writer;
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

static void acculumateArrayBuffersForAllWorlds(v8::Isolate* isolate, DOMArrayBuffer* object, Vector<v8::Local<v8::ArrayBuffer>, 4>& buffers)
{
    if (isMainThread()) {
        Vector<RefPtr<DOMWrapperWorld>> worlds;
        DOMWrapperWorld::allWorldsInMainThread(worlds);
        for (size_t i = 0; i < worlds.size(); i++) {
            v8::Local<v8::Object> wrapper = worlds[i]->domDataStore().get(object, isolate);
            if (!wrapper.IsEmpty())
                buffers.append(v8::Local<v8::ArrayBuffer>::Cast(wrapper));
        }
    } else {
        v8::Local<v8::Object> wrapper = DOMWrapperWorld::current(isolate).domDataStore().get(object, isolate);
        if (!wrapper.IsEmpty())
            buffers.append(v8::Local<v8::ArrayBuffer>::Cast(wrapper));
    }
}

void SerializedScriptValue::transferImageBitmaps(v8::Isolate* isolate, const ImageBitmapArray& imageBitmaps, ExceptionState& exceptionState)
{
    if (!imageBitmaps.size())
        return;

    for (size_t i = 0; i < imageBitmaps.size(); ++i) {
        if (imageBitmaps[i]->isNeutered()) {
            exceptionState.throwDOMException(DataCloneError, "ImageBitmap at index " + String::number(i) + " is already neutered.");
            return;
        }
    }

    OwnPtr<ImageBitmapContentsArray> contents = adoptPtr(new ImageBitmapContentsArray);
    HeapHashSet<Member<ImageBitmap>> visited;
    for (size_t i = 0; i < imageBitmaps.size(); ++i) {
        if (visited.contains(imageBitmaps[i]))
            continue;
        visited.add(imageBitmaps[i]);
        contents->append(imageBitmaps[i]->transfer());
    }
    m_imageBitmapContentsArray = contents.release();
}


void SerializedScriptValue::transferArrayBuffers(v8::Isolate* isolate, const ArrayBufferArray& arrayBuffers, ExceptionState& exceptionState)
{
    if (!arrayBuffers.size())
        return;

    for (size_t i = 0; i < arrayBuffers.size(); ++i) {
        if (arrayBuffers[i]->isNeutered()) {
            exceptionState.throwDOMException(DataCloneError, "ArrayBuffer at index " + String::number(i) + " is already neutered.");
            return;
        }
    }

    OwnPtr<ArrayBufferContentsArray> contents = adoptPtr(new ArrayBufferContentsArray(arrayBuffers.size()));

    HeapHashSet<Member<DOMArrayBufferBase>> visited;
    for (size_t i = 0; i < arrayBuffers.size(); ++i) {
        if (visited.contains(arrayBuffers[i]))
            continue;
        visited.add(arrayBuffers[i]);

        if (arrayBuffers[i]->isShared()) {
            bool result = arrayBuffers[i]->shareContentsWith(contents->at(i));
            if (!result) {
                exceptionState.throwDOMException(DataCloneError, "SharedArrayBuffer at index " + String::number(i) + " could not be transferred.");
                return;
            }
        } else {
            Vector<v8::Local<v8::ArrayBuffer>, 4> bufferHandles;
            v8::HandleScope handleScope(isolate);
            acculumateArrayBuffersForAllWorlds(isolate, static_cast<DOMArrayBuffer*>(arrayBuffers[i].get()), bufferHandles);
            bool isNeuterable = true;
            for (size_t j = 0; j < bufferHandles.size(); ++j)
                isNeuterable &= bufferHandles[j]->IsNeuterable();

            DOMArrayBufferBase* toTransfer = arrayBuffers[i];
            if (!isNeuterable)
                toTransfer = DOMArrayBuffer::create(arrayBuffers[i]->buffer()->data(), arrayBuffers[i]->buffer()->byteLength());
            bool result = toTransfer->transfer(contents->at(i));
            if (!result) {
                exceptionState.throwDOMException(DataCloneError, "ArrayBuffer at index " + String::number(i) + " could not be transferred.");
                return;
            }

            if (isNeuterable)
                for (size_t j = 0; j < bufferHandles.size(); ++j)
                    bufferHandles[j]->Neuter();
        }

    }
    m_arrayBufferContentsArray = contents.release();
}

SerializedScriptValue::SerializedScriptValue(const String& wireData)
    : m_externallyAllocatedMemory(0)
{
    m_data = wireData.isolatedCopy();
}

v8::Local<v8::Value> SerializedScriptValue::deserialize(MessagePortArray* messagePorts)
{
    return deserialize(v8::Isolate::GetCurrent(), messagePorts, 0);
}

v8::Local<v8::Value> SerializedScriptValue::deserialize(v8::Isolate* isolate, MessagePortArray* messagePorts, const WebBlobInfoArray* blobInfo)
{
    return SerializedScriptValueFactory::instance().deserialize(this, isolate, messagePorts, blobInfo);
}

bool SerializedScriptValue::extractTransferables(v8::Isolate* isolate, v8::Local<v8::Value> value, int argumentIndex, Transferables& transferables, ExceptionState& exceptionState)
{
    if (value.IsEmpty() || value->IsUndefined())
        return true;

    uint32_t length = 0;
    if (value->IsArray()) {
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);
        length = array->Length();
    } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
        if (!exceptionState.hadException())
            exceptionState.throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex + 1));
        return false;
    }

    v8::Local<v8::Object> transferableArray = v8::Local<v8::Object>::Cast(value);

    // Validate the passed array of transferables.
    for (unsigned i = 0; i < length; ++i) {
        v8::Local<v8::Value> transferableObject;
        if (!transferableArray->Get(isolate->GetCurrentContext(), i).ToLocal(&transferableObject))
            return false;
        // Validation of non-null objects, per HTML5 spec 10.3.3.
        if (isUndefinedOrNull(transferableObject)) {
            exceptionState.throwTypeError("Value at index " + String::number(i) + " is an untransferable " + (transferableObject->IsUndefined() ? "'undefined'" : "'null'") + " value.");
            return false;
        }
        if (!SerializedScriptValueFactory::instance().extractTransferables(isolate, transferables, exceptionState, transferableObject, i)) {
            if (!exceptionState.hadException())
                exceptionState.throwTypeError("Value at index " + String::number(i) + " does not have a transferable type.");
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

bool SerializedScriptValue::containsTransferableArrayBuffer() const
{
    return m_arrayBufferContentsArray && !m_arrayBufferContentsArray->isEmpty();
}

} // namespace blink
