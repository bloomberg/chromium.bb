// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDLTypes_h
#define IDLTypes_h

#include <type_traits>
#include "bindings/core/v8/IDLTypesBase.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ScriptPromise;

// Boolean
struct IDLBoolean final : public IDLBaseHelper<bool> {};

// Integers
struct IDLByte final : public IDLBaseHelper<int8_t> {};
struct IDLOctet final : public IDLBaseHelper<uint8_t> {};
struct IDLShort final : public IDLBaseHelper<int16_t> {};
struct IDLUnsignedShort final : public IDLBaseHelper<uint16_t> {};
struct IDLLong final : public IDLBaseHelper<int32_t> {};
struct IDLUnsignedLong final : public IDLBaseHelper<uint32_t> {};
struct IDLLongLong final : public IDLBaseHelper<int64_t> {};
struct IDLUnsignedLongLong final : public IDLBaseHelper<uint64_t> {};

// Strings
struct IDLByteString final : public IDLBaseHelper<String> {};
struct IDLString final : public IDLBaseHelper<String> {};
struct IDLUSVString final : public IDLBaseHelper<String> {};

// Double
struct IDLDouble final : public IDLBaseHelper<double> {};
struct IDLUnrestrictedDouble final : public IDLBaseHelper<double> {};

// Float
struct IDLFloat final : public IDLBaseHelper<float> {};
struct IDLUnrestrictedFloat final : public IDLBaseHelper<float> {};

struct IDLDate final : public IDLBaseHelper<double> {};

// Promise
struct IDLPromise final : public IDLBaseHelper<ScriptPromise> {};

// Sequence
template <typename T>
struct IDLSequence final : public IDLBase {
  using ImplType = VectorOf<typename NativeValueTraits<T>::ImplType>;
};

// Record
template <typename Key, typename Value>
struct IDLRecord final : public IDLBase {
  static_assert(std::is_same<Key, IDLByteString>::value ||
                    std::is_same<Key, IDLString>::value ||
                    std::is_same<Key, IDLUSVString>::value,
                "IDLRecord keys must be of a WebIDL string type");

  using ImplType =
      VectorOfPairs<String, typename NativeValueTraits<Value>::ImplType>;
};

// Nullable (T?).
// https://heycam.github.io/webidl/#idl-nullable-type
// Types without a built-in notion of nullability are mapped to Optional<T>.
template <typename InnerType, typename = void>
struct IDLNullable final : public IDLBase {
 private:
  using InnerTraits = NativeValueTraits<InnerType>;
  using InnerResultType =
      decltype(InnerTraits::NativeValue(std::declval<v8::Isolate*>(),
                                        v8::Local<v8::Value>(),
                                        std::declval<ExceptionState&>()));

 public:
  using ResultType = WTF::Optional<std::decay_t<InnerResultType>>;
  using ImplType = ResultType;
  static inline ResultType NullValue() { return WTF::nullopt; }
};
template <typename InnerType>
struct IDLNullable<InnerType,
                   decltype(void(NativeValueTraits<InnerType>::NullValue()))>
    final : public IDLBase {
 private:
  using InnerTraits = NativeValueTraits<InnerType>;
  using InnerResultType =
      decltype(InnerTraits::NativeValue(std::declval<v8::Isolate*>(),
                                        v8::Local<v8::Value>(),
                                        std::declval<ExceptionState&>()));

 public:
  using ResultType = InnerResultType;
  using ImplType = typename InnerTraits::ImplType;
  static inline ResultType NullValue() { return InnerTraits::NullValue(); }
};

}  // namespace blink

#endif  // IDLTypes_h
