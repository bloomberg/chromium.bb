/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#ifndef V8Collection_h
#define V8Collection_h

#include "V8Node.h"
#include "bindings/v8/V8Binding.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLSelectElement.h"
#include <v8.h>

namespace WebCore {
// FIXME: These functions should be named using to* since they return the item (get* is used for method that take a ref param).
// See https://bugs.webkit.org/show_bug.cgi?id=24664.

template<class T> static v8::Handle<v8::Value> getV8Object(T* implementation, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!implementation)
        return v8Undefined();
    return toV8(implementation, creationContext, isolate);
}

template<class Collection> static Collection* toNativeCollection(v8::Local<v8::Object> object)
{
    return reinterpret_cast<Collection*>(object->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));
}

template<class T> static v8::Handle<v8::Value> getV8Object(PassRefPtr<T> implementation, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return getV8Object(implementation.get(), creationContext, isolate);
}

// Get an array containing the names of indexed properties of HTMLSelectElement and HTMLFormElement.
template<class Collection> static v8::Handle<v8::Array> nodeCollectionIndexedPropertyEnumerator(const v8::AccessorInfo& info)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(info.Holder()));
    Collection* collection = toNativeCollection<Collection>(info.Holder());
    int length = collection->length();
    v8::Handle<v8::Array> properties = v8::Array::New(length);
    for (int i = 0; i < length; ++i) {
        // FIXME: Do we need to check that the item function returns a non-null value for this index?
        v8::Handle<v8::Integer> integer = v8Integer(i, info.GetIsolate());
        properties->Set(integer, integer);
    }
    return properties;
}

v8::Handle<v8::Value> toOptionsCollectionSetter(uint32_t index, v8::Handle<v8::Value>, HTMLSelectElement*, v8::Isolate*);

} // namespace WebCore

#endif // V8Collection_h
