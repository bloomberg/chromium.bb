// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDLTypes_h
#define IDLTypes_h

#include <type_traits>
#include "bindings/core/v8/IDLTypesBase.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/TypeTraits.h"
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
 private:
  using CppType = typename NativeValueTraits<T>::ImplType;
  using MaybeMemberCppType =
      typename std::conditional<WTF::IsGarbageCollectedType<CppType>::value,
                                Member<CppType>,
                                CppType>::type;

 public:
  // Two kinds of types need HeapVector:
  // 1. Oilpan types, which are wrapped by Member<>.
  // 2. IDL unions and dictionaries, which are not Oilpan types themselves (and
  //    are thus not wrapped by Member<>) but have a trace() method and can
  //    contain Oilpan members. They both also happen to specialize V8TypeOf,
  //    which we use to recognize them.
  using ImplType = typename std::conditional<
      std::is_same<MaybeMemberCppType, Member<CppType>>::value ||
          std::is_class<typename V8TypeOf<CppType>::Type>::value,
      HeapVector<MaybeMemberCppType>,
      Vector<CppType>>::type;
};

// Record
template <typename Key, typename Value>
struct IDLRecord final : public IDLBase {
  static_assert(std::is_same<Key, IDLByteString>::value ||
                    std::is_same<Key, IDLString>::value ||
                    std::is_same<Key, IDLUSVString>::value,
                "IDLRecord keys must be of a WebIDL string type");

 private:
  // Record keys are always strings, so we do not need to introspect them.
  using ValueCppType = typename NativeValueTraits<Value>::ImplType;
  using MaybeMemberValueCppType = typename std::conditional<
      WTF::IsGarbageCollectedType<ValueCppType>::value,
      Member<ValueCppType>,
      ValueCppType>::type;

 public:
  // Two kinds of types need HeapVector:
  // 1. Oilpan types, which are wrapped by Member<>.
  // 2. IDL unions and dictionaries, which are not Oilpan types themselves (and
  //    are thus not wrapped by Member<>) but have a trace() method and can
  //    contain Oilpan members. They both also happen to specialize V8TypeOf,
  //    which we use to recognize them.
  using ImplType = typename std::conditional<
      std::is_same<MaybeMemberValueCppType, Member<ValueCppType>>::value ||
          std::is_class<typename V8TypeOf<ValueCppType>::Type>::value,
      HeapVector<std::pair<String, MaybeMemberValueCppType>>,
      Vector<std::pair<String, ValueCppType>>>::type;
};

}  // namespace blink

#endif  // IDLTypes_h
