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
struct NativeValueTraits<IDLBoolean>
    : public NativeValueTraitsBase<IDLBoolean> {
  CORE_EXPORT static inline bool nativeValue(v8::Isolate* isolate,
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
struct NativeValueTraits<IDLByte> : public NativeValueTraitsBase<IDLByte> {
  CORE_EXPORT static inline int8_t nativeValue(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value,
                                               ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  CORE_EXPORT static inline int8_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toInt8(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLOctet> : public NativeValueTraitsBase<IDLOctet> {
  CORE_EXPORT static inline uint8_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  CORE_EXPORT static inline uint8_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toUInt8(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLShort> : public NativeValueTraitsBase<IDLShort> {
  CORE_EXPORT static inline int16_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  CORE_EXPORT static inline int16_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toInt16(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLUnsignedShort>
    : public NativeValueTraitsBase<IDLUnsignedShort> {
  CORE_EXPORT static inline uint16_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  CORE_EXPORT static inline uint16_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toUInt16(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLLong> : public NativeValueTraitsBase<IDLLong> {
  CORE_EXPORT static inline int32_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  CORE_EXPORT static inline int32_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toInt32(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLUnsignedLong>
    : public NativeValueTraitsBase<IDLUnsignedLong> {
  CORE_EXPORT static inline uint32_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  CORE_EXPORT static inline uint32_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toUInt32(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLLongLong>
    : public NativeValueTraitsBase<IDLLongLong> {
  CORE_EXPORT static inline int64_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  CORE_EXPORT static inline int64_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toInt64(isolate, value, conversionMode, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLUnsignedLongLong>
    : public NativeValueTraitsBase<IDLUnsignedLongLong> {
  CORE_EXPORT static inline uint64_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, NormalConversion);
  }

  CORE_EXPORT static inline uint64_t nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState,
      IntegerConversionConfiguration conversionMode) {
    return toUInt64(isolate, value, conversionMode, exceptionState);
  }
};

// Strings
template <>
struct NativeValueTraits<IDLByteString>
    : public NativeValueTraitsBase<IDLByteString> {
  CORE_EXPORT static inline String nativeValue(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value,
                                               ExceptionState& exceptionState) {
    return toByteString(isolate, value, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLString> : public NativeValueTraitsBase<IDLString> {
  CORE_EXPORT static inline String nativeValue(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value,
                                               ExceptionState& exceptionState) {
    return nativeValue<V8StringResourceMode::DefaultMode>(isolate, value,
                                                          exceptionState);
  }

  template <V8StringResourceMode Mode = DefaultMode>
  CORE_EXPORT static inline String nativeValue(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value,
                                               ExceptionState& exceptionState) {
    V8StringResource<Mode> string(value);
    if (!string.prepare(isolate, exceptionState))
      return String();
    return string;
  }
};

template <>
struct NativeValueTraits<IDLUSVString>
    : public NativeValueTraitsBase<IDLUSVString> {
  CORE_EXPORT static inline String nativeValue(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value,
                                               ExceptionState& exceptionState) {
    return toUSVString(isolate, value, exceptionState);
  }
};

// Floats and doubles
template <>
struct NativeValueTraits<IDLDouble> : public NativeValueTraitsBase<IDLDouble> {
  CORE_EXPORT static inline double nativeValue(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value,
                                               ExceptionState& exceptionState) {
    return toRestrictedDouble(isolate, value, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLUnrestrictedDouble>
    : public NativeValueTraitsBase<IDLUnrestrictedDouble> {
  CORE_EXPORT static inline double nativeValue(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value,
                                               ExceptionState& exceptionState) {
    return toDouble(isolate, value, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLFloat> : public NativeValueTraitsBase<IDLFloat> {
  CORE_EXPORT static inline float nativeValue(v8::Isolate* isolate,
                                              v8::Local<v8::Value> value,
                                              ExceptionState& exceptionState) {
    return toRestrictedFloat(isolate, value, exceptionState);
  }
};

template <>
struct NativeValueTraits<IDLUnrestrictedFloat>
    : public NativeValueTraitsBase<IDLUnrestrictedFloat> {
  CORE_EXPORT static inline float nativeValue(v8::Isolate* isolate,
                                              v8::Local<v8::Value> value,
                                              ExceptionState& exceptionState) {
    return toFloat(isolate, value, exceptionState);
  }
};

// Promises
template <>
struct NativeValueTraits<IDLPromise>
    : public NativeValueTraitsBase<IDLPromise> {
  CORE_EXPORT static inline ScriptPromise nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value);
  }

  CORE_EXPORT static inline ScriptPromise nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value) {
    return ScriptPromise::cast(ScriptState::current(isolate), value);
  }
};

// Type-specific overloads
template <>
struct NativeValueTraits<IDLDate> : public NativeValueTraitsBase<IDLDate> {
  CORE_EXPORT static inline double nativeValue(v8::Isolate* isolate,
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

  CORE_EXPORT static inline ImplType nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return nativeValue(isolate, value, exceptionState, 0);
  }

  CORE_EXPORT static inline ImplType nativeValue(v8::Isolate* isolate,
                                                 v8::Local<v8::Value> value,
                                                 ExceptionState& exceptionState,
                                                 int index) {
    return toImplArray<ImplType, T>(value, index, isolate, exceptionState);
  }
};

}  // namespace blink

#endif  // NativeValueTraitsImpl_h
