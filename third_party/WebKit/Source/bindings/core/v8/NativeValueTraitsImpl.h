// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NativeValueTraitsImpl_h
#define NativeValueTraitsImpl_h

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/CoreExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Boolean
template <>
struct CORE_EXPORT NativeValueTraits<IDLBoolean>
    : public NativeValueTraitsBase<IDLBoolean> {
  static bool nativeValue(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exceptionState) {
    return toBoolean(isolate, value, exceptionState);
  }
};

// Integers
//
// All integer specializations offer a second nativeValue() besides the default
// one: it takes an IntegerConversionConfiguration argument to let callers
// specify how the integers should be converted. The default nativeValue()
// overload will always use NormalConversion.
template <>
struct CORE_EXPORT NativeValueTraits<IDLByte>
    : public NativeValueTraitsBase<IDLByte> {
  static int8_t nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static int8_t nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState,
                            IntegerConversionConfiguration conversionMode) {
    return toInt8(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOctet>
    : public NativeValueTraitsBase<IDLOctet> {
  static uint8_t nativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static uint8_t nativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exceptionState,
                             IntegerConversionConfiguration conversionMode) {
    return toUInt8(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLShort>
    : public NativeValueTraitsBase<IDLShort> {
  static int16_t nativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static int16_t nativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exceptionState,
                             IntegerConversionConfiguration conversionMode) {
    return toInt16(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedShort>
    : public NativeValueTraitsBase<IDLUnsignedShort> {
  static uint16_t nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static uint16_t nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exceptionState,
                              IntegerConversionConfiguration conversionMode) {
    return toUInt16(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLong>
    : public NativeValueTraitsBase<IDLLong> {
  static int32_t nativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static int32_t nativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exceptionState,
                             IntegerConversionConfiguration conversionMode) {
    return toInt32(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLong>
    : public NativeValueTraitsBase<IDLUnsignedLong> {
  static uint32_t nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static uint32_t nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exceptionState,
                              IntegerConversionConfiguration conversionMode) {
    return toUInt32(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLongLong>
    : public NativeValueTraitsBase<IDLLongLong> {
  static int64_t nativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static int64_t nativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exceptionState,
                             IntegerConversionConfiguration conversionMode) {
    return toInt64(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLongLong>
    : public NativeValueTraitsBase<IDLUnsignedLongLong> {
  static uint64_t nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static uint64_t nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exceptionState,
                              IntegerConversionConfiguration conversionMode) {
    return toUInt64(isolate, value, conversionMode, exceptionState);
  }
};

// Strings
template <>
struct CORE_EXPORT NativeValueTraits<IDLByteString>
    : public NativeValueTraitsBase<IDLByteString> {
  static String nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState) {
    return toByteString(isolate, value, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLString>
    : public NativeValueTraitsBase<IDLString> {
  static String nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState) {
    return nativeValue<V8StringResourceMode::DefaultMode>(isolate, value,
                                                          exceptionState);
  }

  template <V8StringResourceMode Mode = DefaultMode>
  static String nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState) {
    V8StringResource<Mode> string(value);
    if (!string.prepare(isolate, exceptionState))
      return String();
    return string;
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUSVString>
    : public NativeValueTraitsBase<IDLUSVString> {
  static String nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState) {
    return toUSVString(isolate, value, exceptionState);
  }
};

// Floats and doubles
template <>
struct CORE_EXPORT NativeValueTraits<IDLDouble>
    : public NativeValueTraitsBase<IDLDouble> {
  static double nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState) {
    return toRestrictedDouble(isolate, value, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedDouble>
    : public NativeValueTraitsBase<IDLUnrestrictedDouble> {
  static double nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState) {
    return toDouble(isolate, value, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLFloat>
    : public NativeValueTraitsBase<IDLFloat> {
  static float nativeValue(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           ExceptionState& exceptionState) {
    return toRestrictedFloat(isolate, value, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedFloat>
    : public NativeValueTraitsBase<IDLUnrestrictedFloat> {
  static float nativeValue(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           ExceptionState& exceptionState) {
    return toFloat(isolate, value, exceptionState);
  }
};

// Promises
template <>
struct CORE_EXPORT NativeValueTraits<IDLPromise>
    : public NativeValueTraitsBase<IDLPromise> {
  static ScriptPromise nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    return nativeValue(isolate, value);
  }

  static ScriptPromise nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value) {
    return ScriptPromise::cast(ScriptState::current(isolate), value);
  }
};

// Type-specific overloads
template <>
struct CORE_EXPORT NativeValueTraits<IDLDate>
    : public NativeValueTraitsBase<IDLDate> {
  static double nativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exceptionState) {
    return toCoreDate(isolate, value, exceptionState);
  }
};

// Sequences
template <typename T>
struct NativeValueTraits<IDLSequence<T>>
    : public NativeValueTraitsBase<IDLSequence<T>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLSequence<T>>::ImplType;

  static ImplType nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, 0);
  }

  static ImplType nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exceptionState,
                              int index) {
    return toImplArray<ImplType, T>(value, index, isolate, exceptionState);
  }
};

// Records
template <typename K, typename V>
struct NativeValueTraits<IDLRecord<K, V>>
    : public NativeValueTraitsBase<IDLRecord<K, V>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLRecord<K, V>>::ImplType;

  // Converts a JavaScript value |O| to an IDL record<K, V> value. In C++, a
  // record is represented as a Vector<std::pair<k, v>> (or a HeapVector if
  // necessary). See https://heycam.github.io/webidl/#es-record.
  static ImplType nativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> v8Value,
                              ExceptionState& exceptionState) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    // "1. If Type(O) is not Object, throw a TypeError."
    if (!v8Value->IsObject()) {
      exceptionState.throwTypeError(
          "Only objects can be converted to record<K,V> types");
      return ImplType();
    }
    v8::Local<v8::Object> v8Object = v8::Local<v8::Object>::Cast(v8Value);
    v8::TryCatch block(isolate);

    // "3. Let keys be ? O.[[OwnPropertyKeys]]()."
    v8::Local<v8::Array> keys;
    // While we could pass v8::ONLY_ENUMERABLE below, doing so breaks
    // web-platform-tests' headers-record.html and deviates from the spec
    // algorithm.
    // Symbols are being skipped due to
    // https://github.com/heycam/webidl/issues/294.
    if (!v8Object
             ->GetOwnPropertyNames(context,
                                   static_cast<v8::PropertyFilter>(
                                       v8::PropertyFilter::ALL_PROPERTIES |
                                       v8::PropertyFilter::SKIP_SYMBOLS))
             .ToLocal(&keys)) {
      exceptionState.rethrowV8Exception(block.Exception());
      return ImplType();
    }
    if (keys->Length() > ImplType::maxCapacity()) {
      exceptionState.throwRangeError("Array length exceeds supported limit.");
      return ImplType();
    }

    // "2. Let result be a new empty instance of record<K, V>."
    ImplType result;
    result.reserveInitialCapacity(keys->Length());

    // The conversion algorithm needs a data structure with fast insertion at
    // the end while at the same time requiring fast checks for previous insert
    // of a given key. |seenKeys| is a key/position in |result| map that aids in
    // the latter part.
    HashMap<String, size_t> seenKeys;

    for (uint32_t i = 0; i < keys->Length(); ++i) {
      // "4. Repeat, for each element key of keys in List order:"
      v8::Local<v8::Value> key;
      if (!keys->Get(context, i).ToLocal(&key)) {
        exceptionState.rethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.1. Let desc be ? O.[[GetOwnProperty]](key)."
      v8::Local<v8::Value> desc;
      if (!v8Object
               ->GetOwnPropertyDescriptor(
                   context, key->ToString(context).ToLocalChecked())
               .ToLocal(&desc)) {
        exceptionState.rethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.2. If desc is not undefined and desc.[[Enumerable]] is true:"
      // We can call ToLocalChecked() and ToChecked() here because
      // GetOwnPropertyDescriptor is responsible for catching any exceptions
      // and failures, and if we got to this point of the code we have a proper
      // object that was not created by a user.
      DCHECK(desc->IsObject());
      v8::Local<v8::Value> enumerable =
          v8::Local<v8::Object>::Cast(desc)
              ->Get(context, v8String(isolate, "enumerable"))
              .ToLocalChecked();
      if (!enumerable->BooleanValue(context).ToChecked())
        continue;

      // "4.2.1. Let typedKey be key converted to an IDL value of type K."
      String typedKey =
          NativeValueTraits<K>::nativeValue(isolate, key, exceptionState);
      if (exceptionState.hadException())
        return ImplType();

      // "4.2.2. Let value be ? Get(O, key)."
      v8::Local<v8::Value> value;
      if (!v8Object->Get(context, key).ToLocal(&value)) {
        exceptionState.rethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.2.3. Let typedValue be value converted to an IDL value of type V."
      typename ImplType::ValueType::second_type typedValue =
          NativeValueTraits<V>::nativeValue(isolate, value, exceptionState);
      if (exceptionState.hadException())
        return ImplType();

      if (seenKeys.contains(typedKey)) {
        // "4.2.4. If typedKey is already a key in result, set its value to
        //         typedValue.
        //         Note: This can happen when O is a proxy object."
        const size_t pos = seenKeys.at(typedKey);
        result[pos] = std::make_pair(typedKey, typedValue);
      } else {
        // "4.2.5. Otherwise, append to result a mapping (typedKey,
        // typedValue)."
        // Note we can take this shortcut because we are always appending.
        const size_t pos = result.size();
        seenKeys.set(typedKey, pos);
        result.uncheckedAppend(std::make_pair(typedKey, typedValue));
      }
    }
    // "5. Return result."
    return result;
  }
};

}  // namespace blink

#endif  // NativeValueTraitsImpl_h
