// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDLTypes_h
#define IDLTypes_h

#include <type_traits>
#include "bindings/core/v8/IDLTypesBase.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/TypeTraits.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptPromise;

// Boolean
struct CORE_EXPORT IDLBoolean final : public IDLBaseHelper<bool> {};

// Integers
struct CORE_EXPORT IDLByte final : public IDLBaseHelper<int8_t> {};
struct CORE_EXPORT IDLOctet final : public IDLBaseHelper<uint8_t> {};
struct CORE_EXPORT IDLShort final : public IDLBaseHelper<int16_t> {};
struct CORE_EXPORT IDLUnsignedShort final : public IDLBaseHelper<uint16_t> {};
struct CORE_EXPORT IDLLong final : public IDLBaseHelper<int32_t> {};
struct CORE_EXPORT IDLUnsignedLong final : public IDLBaseHelper<uint32_t> {};
struct CORE_EXPORT IDLLongLong final : public IDLBaseHelper<int64_t> {};
struct CORE_EXPORT IDLUnsignedLongLong final : public IDLBaseHelper<uint64_t> {
};

// Strings
struct CORE_EXPORT IDLByteString final : public IDLBaseHelper<String> {};
struct CORE_EXPORT IDLString final : public IDLBaseHelper<String> {};
struct CORE_EXPORT IDLUSVString final : public IDLBaseHelper<String> {};

// Double
struct CORE_EXPORT IDLDouble final : public IDLBaseHelper<double> {};
struct CORE_EXPORT IDLUnrestrictedDouble final : public IDLBaseHelper<double> {
};

// Float
struct CORE_EXPORT IDLFloat final : public IDLBaseHelper<float> {};
struct CORE_EXPORT IDLUnrestrictedFloat final : public IDLBaseHelper<float> {};

struct CORE_EXPORT IDLDate final : public IDLBaseHelper<double> {};

// Promise
struct CORE_EXPORT IDLPromise final : public IDLBaseHelper<ScriptPromise> {};

// Sequence
template <typename T>
struct CORE_EXPORT IDLSequence final : public IDLBase {
 private:
  using CppType = typename NativeValueTraits<T>::ImplType;
  using MaybeWrappedCppType =
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
      std::is_same<MaybeWrappedCppType, Member<CppType>>::value ||
          std::is_class<typename V8TypeOf<CppType>::Type>::value,
      HeapVector<MaybeWrappedCppType>,
      Vector<CppType>>::type;
};

}  // namespace blink

#endif  // IDLTypes_h
