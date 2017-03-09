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
  static inline bool nativeValue(v8::Isolate* isolate,
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
  static inline int8_t nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static inline int8_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toInt8(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOctet>
    : public NativeValueTraitsBase<IDLOctet> {
  static inline uint8_t nativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static inline uint8_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toUInt8(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLShort>
    : public NativeValueTraitsBase<IDLShort> {
  static inline int16_t nativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static inline int16_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toInt16(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedShort>
    : public NativeValueTraitsBase<IDLUnsignedShort> {
  static inline uint16_t nativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static inline uint16_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toUInt16(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLong>
    : public NativeValueTraitsBase<IDLLong> {
  static inline int32_t nativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static inline int32_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toInt32(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLong>
    : public NativeValueTraitsBase<IDLUnsignedLong> {
  static inline uint32_t nativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static inline uint32_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toUInt32(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLongLong>
    : public NativeValueTraitsBase<IDLLongLong> {
  static inline int64_t nativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static inline int64_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toInt64(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLongLong>
    : public NativeValueTraitsBase<IDLUnsignedLongLong> {
  static inline uint64_t nativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  static inline uint64_t nativeValue(
      v8::Isolate* isolate,
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
  static inline String nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    return toByteString(isolate, value, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLString>
    : public NativeValueTraitsBase<IDLString> {
  static inline String nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    return nativeValue<V8StringResourceMode::DefaultMode>(isolate, value,
                                                          exceptionState);
  }

  template <V8StringResourceMode Mode = DefaultMode>
  static inline String nativeValue(v8::Isolate* isolate,
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
  static inline String nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    return toUSVString(isolate, value, exceptionState);
  }
};

// Floats and doubles
template <>
struct CORE_EXPORT NativeValueTraits<IDLDouble>
    : public NativeValueTraitsBase<IDLDouble> {
  static inline double nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    return toRestrictedDouble(isolate, value, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedDouble>
    : public NativeValueTraitsBase<IDLUnrestrictedDouble> {
  static inline double nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    return toDouble(isolate, value, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLFloat>
    : public NativeValueTraitsBase<IDLFloat> {
  static inline float nativeValue(v8::Isolate* isolate,
                                  v8::Local<v8::Value> value,
                                  ExceptionState& exceptionState) {
    return toRestrictedFloat(isolate, value, exceptionState);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedFloat>
    : public NativeValueTraitsBase<IDLUnrestrictedFloat> {
  static inline float nativeValue(v8::Isolate* isolate,
                                  v8::Local<v8::Value> value,
                                  ExceptionState& exceptionState) {
    return toFloat(isolate, value, exceptionState);
  }
};

// Promises
template <>
struct CORE_EXPORT NativeValueTraits<IDLPromise>
    : public NativeValueTraitsBase<IDLPromise> {
  static inline ScriptPromise nativeValue(v8::Isolate* isolate,
                                          v8::Local<v8::Value> value,
                                          ExceptionState& exceptionState) {
    return nativeValue(isolate, value);
  }

  static inline ScriptPromise nativeValue(v8::Isolate* isolate,
                                          v8::Local<v8::Value> value) {
    return ScriptPromise::cast(ScriptState::current(isolate), value);
  }
};

// Type-specific overloads
template <>
struct CORE_EXPORT NativeValueTraits<IDLDate>
    : public NativeValueTraitsBase<IDLDate> {
  static inline double nativeValue(v8::Isolate* isolate,
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

  static inline ImplType nativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, 0);
  }

  static inline ImplType nativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exceptionState,
                                     int index) {
    return toImplArray<ImplType, T>(value, index, isolate, exceptionState);
  }
};

}  // namespace blink

#endif  // NativeValueTraitsImpl_h
