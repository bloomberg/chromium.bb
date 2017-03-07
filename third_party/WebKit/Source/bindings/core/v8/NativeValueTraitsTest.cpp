// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/NativeValueTraits.h"

#include <type_traits>
#include "bindings/core/v8/IDLTypesBase.h"

// No gtest tests; only static_assert checks.

namespace blink {

template <>
struct NativeValueTraits<bool> : public NativeValueTraitsBase<bool> {};

static_assert(std::is_same<NativeValueTraits<bool>::ImplType, bool>::value,
              "NativeValueTraitsBase works with non IDLBase-derived types");

struct MyIDLType final : public IDLBaseHelper<char> {};
template <>
struct NativeValueTraits<MyIDLType> : public NativeValueTraitsBase<MyIDLType> {
};

static_assert(std::is_same<NativeValueTraits<MyIDLType>::ImplType, char>::value,
              "NativeValueTraitsBase works with IDLBase-derived types");

}  // namespace blink
