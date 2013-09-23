/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/custom/V8ArrayBufferCustom.h"

#include "bindings/v8/V8Binding.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

using namespace WTF;

V8ArrayBufferDeallocationObserver* V8ArrayBufferDeallocationObserver::instance()
{
    DEFINE_STATIC_LOCAL(V8ArrayBufferDeallocationObserver, deallocationObserver, ());
    return &deallocationObserver;
}

WrapperTypeInfo V8ArrayBuffer::info = {
    0, V8ArrayBuffer::derefObject,
    0, 0, 0, 0, 0, WrapperTypeObjectPrototype
};

bool V8ArrayBuffer::HasInstance(v8::Handle<v8::Value> value, v8::Isolate*, WrapperWorldType)
{
    return value->IsArrayBuffer();
}

bool V8ArrayBuffer::HasInstanceInAnyWorld(v8::Handle<v8::Value> value, v8::Isolate*)
{
    return value->IsArrayBuffer();
}

void V8ArrayBuffer::derefObject(void* object)
{
    static_cast<ArrayBuffer*>(object)->deref();
}

v8::Handle<v8::Object> V8ArrayBuffer::createWrapper(PassRefPtr<ArrayBuffer> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl.get());
    ASSERT(!DOMDataStore::containsWrapper<V8ArrayBuffer>(impl.get(), isolate));

    v8::Handle<v8::Object> wrapper = v8::ArrayBuffer::New(impl->data(), impl->byteLength());
    impl->setDeallocationObserver(V8ArrayBufferDeallocationObserver::instance());

    V8DOMWrapper::associateObjectWithWrapper<V8ArrayBuffer>(impl, &info, wrapper, isolate, WrapperConfiguration::Independent);
    return wrapper;
}

ArrayBuffer* V8ArrayBuffer::toNative(v8::Handle<v8::Object> object)
{
    ASSERT(object->IsArrayBuffer());
    void* arraybufferPtr = object->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex);
    if (arraybufferPtr)
        return reinterpret_cast<ArrayBuffer*>(arraybufferPtr);

    v8::Local<v8::ArrayBuffer> v8buffer = object.As<v8::ArrayBuffer>();
    ASSERT(!v8buffer->IsExternal());

    v8::ArrayBuffer::Contents v8Contents = v8buffer->Externalize();
    ArrayBufferContents contents(v8Contents.Data(), v8Contents.ByteLength(),
        V8ArrayBufferDeallocationObserver::instance());
    RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(contents);
    V8DOMWrapper::associateObjectWithWrapper<V8ArrayBuffer>(buffer.release(), &info, object, v8::Isolate::GetCurrent(), WrapperConfiguration::Dependent);

    arraybufferPtr = object->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex);
    ASSERT(arraybufferPtr);
    return reinterpret_cast<ArrayBuffer*>(arraybufferPtr);
}

} // namespace WebCore
