/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "bindings/core/v8/ArrayValue.h"
#include "bindings/core/v8/DictionaryHelperForBindings.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMError.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "bindings/core/v8/V8MediaKeyError.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/core/v8/V8Path2D.h"
#include "bindings/core/v8/V8TextTrack.h"
#include "bindings/core/v8/V8Uint8Array.h"
#include "bindings/core/v8/V8VoidCallback.h"
#include "bindings/core/v8/V8Window.h"
#include "core/html/track/TrackBase.h"
#include "wtf/MathExtras.h"

namespace blink {

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, v8::Local<v8::Value>& value)
{
    return dictionary.get(key, value);
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, Dictionary& value)
{
    return dictionary.get(key, value);
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, bool& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    v8::Local<v8::Boolean> v8Bool = v8Value->ToBoolean(dictionary.isolate());
    if (v8Bool.IsEmpty())
        return false;
    value = v8Bool->Value();
    return true;
}

template <>
bool DictionaryHelper::convert(const Dictionary& dictionary, Dictionary::ConversionContext& context, const String& key, bool& value)
{
    Dictionary::ConversionContextScope scope(context);
    DictionaryHelper::get(dictionary, key, value);
    return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, int32_t& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32(dictionary.isolate());
    if (v8Int32.IsEmpty())
        return false;
    value = v8Int32->Value();
    return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, double& value, bool& hasValue)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value)) {
        hasValue = false;
        return false;
    }

    hasValue = true;

    v8::TryCatch block;
    value = v8Value->NumberValue();
    if (UNLIKELY(block.HasCaught())) {
        block.ReThrow();
        return false;
    }
    return true;
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, double& value)
{
    bool unused;
    return DictionaryHelper::get(dictionary, key, value, unused);
}

template <>
bool DictionaryHelper::convert(const Dictionary& dictionary, Dictionary::ConversionContext& context, const String& key, double& value)
{
    Dictionary::ConversionContextScope scope(context);

    bool hasValue = false;
    if (!DictionaryHelper::get(dictionary, key, value, hasValue) && hasValue) {
        context.throwTypeError(ExceptionMessages::incorrectPropertyType(key, "is not of type 'double'."));
        return false;
    }
    return true;
}

template<typename StringType>
bool getStringType(const Dictionary& dictionary, const String& key, StringType& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    V8StringResource<> stringValue(v8Value);
    if (!stringValue.prepare())
        return false;
    value = stringValue;
    return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, String& value)
{
    return getStringType(dictionary, key, value);
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, AtomicString& value)
{
    return getStringType(dictionary, key, value);
}

template <>
bool DictionaryHelper::convert(const Dictionary& dictionary, Dictionary::ConversionContext& context, const String& key, String& value)
{
    Dictionary::ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return true;

    V8StringResource<> stringValue(v8Value);
    if (!stringValue.prepare())
        return false;
    value = stringValue;
    return true;
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, ScriptValue& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    value = ScriptValue(ScriptState::current(dictionary.isolate()), v8Value);
    return true;
}

template <>
bool DictionaryHelper::convert(const Dictionary& dictionary, Dictionary::ConversionContext& context, const String& key, ScriptValue& value)
{
    Dictionary::ConversionContextScope scope(context);

    DictionaryHelper::get(dictionary, key, value);
    return true;
}

template<typename NumericType>
bool getNumericType(const Dictionary& dictionary, const String& key, NumericType& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32(dictionary.isolate());
    if (v8Int32.IsEmpty())
        return false;
    value = static_cast<NumericType>(v8Int32->Value());
    return true;
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, short& value)
{
    return getNumericType<short>(dictionary, key, value);
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, unsigned short& value)
{
    return getNumericType<unsigned short>(dictionary, key, value);
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, unsigned& value)
{
    return getNumericType<unsigned>(dictionary, key, value);
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, unsigned long& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    v8::Local<v8::Integer> v8Integer = v8Value->ToInteger(dictionary.isolate());
    if (v8Integer.IsEmpty())
        return false;
    value = static_cast<unsigned long>(v8Integer->Value());
    return true;
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, unsigned long long& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    v8::TryCatch block;
    double d = v8Value->NumberValue();
    if (UNLIKELY(block.HasCaught())) {
        block.ReThrow();
        return false;
    }
    doubleToInteger(d, value);
    return true;
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, RefPtrWillBeMember<DOMWindow>& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    // We need to handle a DOMWindow specially, because a DOMWindow wrapper
    // exists on a prototype chain of v8Value.
    value = toDOMWindow(dictionary.isolate(), v8Value);
    return true;
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, RefPtrWillBeMember<TrackBase>& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    TrackBase* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);

        // FIXME: this will need to be changed so it can also return an AudioTrack or a VideoTrack once
        // we add them.
        v8::Handle<v8::Object> track = V8TextTrack::findInstanceInPrototypeChain(wrapper, dictionary.isolate());
        if (!track.IsEmpty())
            source = V8TextTrack::toImpl(track);
    }
    value = source;
    return true;
}

template <>
bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, RefPtrWillBeMember<EventTarget>& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    value = nullptr;
    // We need to handle a DOMWindow specially, because a DOMWindow wrapper
    // exists on a prototype chain of v8Value.
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> window = V8Window::findInstanceInPrototypeChain(wrapper, dictionary.isolate());
        if (!window.IsEmpty()) {
            value = toWrapperTypeInfo(window)->toEventTarget(window);
            return true;
        }
    }

    if (V8DOMWrapper::isWrapper(dictionary.isolate(), v8Value)) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        value = toWrapperTypeInfo(wrapper)->toEventTarget(wrapper);
    }
    return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, Vector<String>& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    if (!v8Value->IsArray())
        return false;

    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8::Uint32::New(dictionary.isolate(), i));
        TOSTRING_DEFAULT(V8StringResource<>, stringValue, indexedValue, false);
        value.append(stringValue);
    }

    return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, Vector<Vector<String>>& value, ExceptionState& exceptionState)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    if (!v8Value->IsArray())
        return false;

    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> v8IndexedValue = v8Array->Get(v8::Uint32::New(dictionary.isolate(), i));
        Vector<String> indexedValue = toImplArray<String>(v8IndexedValue, i, dictionary.isolate(), exceptionState);
        if (exceptionState.hadException())
            return false;
        value.append(indexedValue);
    }

    return true;
}

template <>
bool DictionaryHelper::convert(const Dictionary& dictionary, Dictionary::ConversionContext& context, const String& key, Vector<String>& value)
{
    Dictionary::ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return true;

    if (context.isNullable() && blink::isUndefinedOrNull(v8Value))
        return true;

    if (!v8Value->IsArray()) {
        context.throwTypeError(ExceptionMessages::notASequenceTypeProperty(key));
        return false;
    }

    return DictionaryHelper::get(dictionary, key, value);
}

template <>
CORE_EXPORT bool DictionaryHelper::get(const Dictionary& dictionary, const String& key, ArrayValue& value)
{
    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return false;

    if (!v8Value->IsArray())
        return false;

    ASSERT(dictionary.isolate());
    ASSERT(dictionary.isolate() == v8::Isolate::GetCurrent());
    value = ArrayValue(v8::Local<v8::Array>::Cast(v8Value), dictionary.isolate());
    return true;
}

template <>
bool DictionaryHelper::convert(const Dictionary& dictionary, Dictionary::ConversionContext& context, const String& key, ArrayValue& value)
{
    Dictionary::ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return true;

    if (context.isNullable() && blink::isUndefinedOrNull(v8Value))
        return true;

    if (!v8Value->IsArray()) {
        context.throwTypeError(ExceptionMessages::notASequenceTypeProperty(key));
        return false;
    }

    return DictionaryHelper::get(dictionary, key, value);
}

template <>
struct DictionaryHelperTraits<DOMUint8Array> {
    typedef V8Uint8Array type;
};

template CORE_EXPORT bool DictionaryHelper::get(const Dictionary&, const String& key, RefPtr<DOMUint8Array>& value);

template <typename T>
struct IntegralTypeTraits {
};

template <>
struct IntegralTypeTraits<uint8_t> {
    static inline uint8_t toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt8(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt8"; }
};

template <>
struct IntegralTypeTraits<int8_t> {
    static inline int8_t toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt8(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "Int8"; }
};

template <>
struct IntegralTypeTraits<unsigned short> {
    static inline uint16_t toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt16(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt16"; }
};

template <>
struct IntegralTypeTraits<short> {
    static inline int16_t toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt16(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "Int16"; }
};

template <>
struct IntegralTypeTraits<unsigned> {
    static inline uint32_t toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt32(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt32"; }
};

template <>
struct IntegralTypeTraits<unsigned long> {
    static inline uint32_t toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt32(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt32"; }
};

template <>
struct IntegralTypeTraits<int> {
    static inline int32_t toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt32(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "Int32"; }
};

template <>
struct IntegralTypeTraits<long> {
    static inline int32_t toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt32(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "Int32"; }
};

template <>
struct IntegralTypeTraits<unsigned long long> {
    static inline unsigned long long toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toUInt64(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "UInt64"; }
};

template <>
struct IntegralTypeTraits<long long> {
    static inline long long toIntegral(v8::Isolate* isolate, v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
    {
        return toInt64(isolate, value, configuration, exceptionState);
    }
    static const String typeName() { return "Int64"; }
};

template<typename T>
bool DictionaryHelper::convert(const Dictionary& dictionary, Dictionary::ConversionContext& context, const String& key, T& value)
{
    Dictionary::ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return true;

    value = IntegralTypeTraits<T>::toIntegral(dictionary.isolate(), v8Value, NormalConversion, context.exceptionState());
    if (context.exceptionState().throwIfNeeded())
        return false;

    return true;
}

template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, uint8_t& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, int8_t& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, unsigned short& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, short& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, unsigned& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, unsigned long& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, int& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, long& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, long long& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, unsigned long long& value);

template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, RefPtrWillBeMember<EventTarget>& value);

template <>
bool DictionaryHelper::convert(const Dictionary& dictionary, Dictionary::ConversionContext& context, const String& key, MessagePortArray& value)
{
    Dictionary::ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!dictionary.get(key, v8Value))
        return true;

    ASSERT(dictionary.isolate());
    ASSERT(dictionary.isolate() == v8::Isolate::GetCurrent());

    if (isUndefinedOrNull(v8Value))
        return true;

    value = toRefPtrWillBeMemberNativeArray<MessagePort, V8MessagePort>(v8Value, key, dictionary.isolate(), context.exceptionState());

    if (context.exceptionState().throwIfNeeded())
        return false;

    return true;
}

} // namespace blink
