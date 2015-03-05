// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ToV8_h
#define ToV8_h

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8Binding.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include <v8.h>

namespace blink {

class DOMWindow;
class Dictionary;
class EventTarget;
class WorkerGlobalScope;

// ScriptWrappable

inline v8::Handle<v8::Value> toV8(ScriptWrappable* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (UNLIKELY(!impl))
        return v8::Null(isolate);
    v8::Handle<v8::Value> wrapper = DOMDataStore::getWrapper(impl, isolate);
    if (!wrapper.IsEmpty())
        return wrapper;

    return impl->wrap(creationContext, isolate);
}

inline v8::Handle<v8::Value> toV8(Node* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (UNLIKELY(!impl))
        return v8::Null(isolate);
    v8::Handle<v8::Value> wrapper = DOMDataStore::getWrapper(impl, isolate);
    if (!wrapper.IsEmpty())
        return wrapper;

    return ScriptWrappable::fromNode(impl)->wrap(creationContext, isolate);
}

// [Custom=ToV8]

v8::Handle<v8::Value> toV8(DOMWindow*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
v8::Handle<v8::Value> toV8(EventTarget*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
v8::Handle<v8::Value> toV8(WorkerGlobalScope*, v8::Handle<v8::Object> creationContext, v8::Isolate*);

// PassRefPtr, RawPtr and RefPtr

template<typename T>
inline v8::Handle<v8::Value> toV8(PassRefPtr<T> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

template<typename T>
inline v8::Handle<v8::Value> toV8(RawPtr<T> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

template<typename T>
inline v8::Handle<v8::Value> toV8(const RefPtr<T>& impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

// Primitives

inline v8::Handle<v8::Value> toV8(const String& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return v8String(isolate, value);
}

inline v8::Handle<v8::Value> toV8(const char* value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return v8String(isolate, value);
}

template<size_t sizeOfValue>
inline v8::Handle<v8::Value> toV8SignedIntegerInternal(int64_t value, v8::Isolate*);

template<>
inline v8::Handle<v8::Value> toV8SignedIntegerInternal<4>(int64_t value, v8::Isolate* isolate)
{
    return v8::Integer::New(isolate, static_cast<int32_t>(value));
}

template<>
inline v8::Handle<v8::Value> toV8SignedIntegerInternal<8>(int64_t value, v8::Isolate* isolate)
{
    int32_t valueIn32Bit = static_cast<int32_t>(value);
    if (valueIn32Bit == value)
        return v8::Integer::New(isolate, value);
    // V8 doesn't have a 64-bit integer implementation.
    return v8::Number::New(isolate, value);
}

template<size_t sizeOfValue>
inline v8::Handle<v8::Value> toV8UnsignedIntegerInternal(uint64_t value, v8::Isolate*);

template<>
inline v8::Handle<v8::Value> toV8UnsignedIntegerInternal<4>(uint64_t value, v8::Isolate* isolate)
{
    return v8::Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(value));
}

template<>
inline v8::Handle<v8::Value> toV8UnsignedIntegerInternal<8>(uint64_t value, v8::Isolate* isolate)
{
    uint32_t valueIn32Bit = static_cast<uint32_t>(value);
    if (valueIn32Bit == value)
        return v8::Integer::NewFromUnsigned(isolate, value);
    // V8 doesn't have a 64-bit integer implementation.
    return v8::Number::New(isolate, value);
}

inline v8::Handle<v8::Value> toV8(int value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8SignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Handle<v8::Value> toV8(long value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8SignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Handle<v8::Value> toV8(long long value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8SignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Handle<v8::Value> toV8(unsigned value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8UnsignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Handle<v8::Value> toV8(unsigned long value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8UnsignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Handle<v8::Value> toV8(unsigned long long value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8UnsignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Handle<v8::Value> toV8(double value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return v8::Number::New(isolate, value);
}

inline v8::Handle<v8::Value> toV8(bool value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return v8::Boolean::New(isolate, value);
}

// Identity operator

inline v8::Handle<v8::Value> toV8(v8::Handle<v8::Value> value, v8::Handle<v8::Object> creationContext, v8::Isolate*)
{
    return value;
}

// Undefined

struct ToV8UndefinedGenerator { }; // Used only for having toV8 return v8::Undefined.

inline v8::Handle<v8::Value> toV8(const ToV8UndefinedGenerator&  value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return v8::Undefined(isolate);
}

// ScriptValue

inline v8::Handle<v8::Value> toV8(const ScriptValue& value, v8::Handle<v8::Object> creationContext, v8::Isolate*)
{
    return value.v8Value();
}

// Dictionary

inline v8::Handle<v8::Value> toV8(const Dictionary& value, v8::Handle<v8::Object> creationContext, v8::Isolate*)
{
    ASSERT_NOT_REACHED();
    return v8::Handle<v8::Value>();
}

// Array

template<typename Sequence>
inline v8::Handle<v8::Value> toV8SequenceInternal(const Sequence& sequence, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    v8::Local<v8::Array> array = v8::Array::New(isolate, sequence.size());
    uint32_t index = 0;
    typename Sequence::const_iterator end = sequence.end();
    for (typename Sequence::const_iterator iter = sequence.begin(); iter != end; ++iter)
        array->Set(v8::Integer::New(isolate, index++), toV8(*iter, creationContext, isolate));
    return array;
}

template<typename T, size_t inlineCapacity>
inline v8::Handle<v8::Value> toV8(const Vector<T, inlineCapacity>& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8SequenceInternal(value, creationContext, isolate);
}

template<typename T, size_t inlineCapacity>
inline v8::Handle<v8::Value> toV8(const HeapVector<T, inlineCapacity>& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8SequenceInternal(value, creationContext, isolate);
}

template<typename T>
inline v8::Handle<v8::Value> toV8(const Vector<std::pair<String, T>>& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    for (unsigned i = 0; i < value.size(); ++i)
        object->Set(v8String(isolate, value[i].first), toV8(value[i].second, creationContext, isolate));
    return object;
}

} // namespace blink

#endif // ToV8_h
