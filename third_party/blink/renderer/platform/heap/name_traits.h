// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_NAME_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_NAME_TRAITS_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename T>
class NameTrait {
  STATIC_ONLY(NameTrait);

 public:
  static constexpr bool HideInternalName() {
#if defined(OFFICIAL_BUILD) || !(defined(COMPILER_GCC) || defined(__clang__))
    return true;
#else
    return false;
#endif
  }

  static const char* GetName(const void* obj) {
    return GetNameFor(static_cast<const T*>(obj));
  }

 private:
  static const char* GetNameFor(const NameClient* wrapper_tracable) {
    return wrapper_tracable->NameInHeapSnapshot();
  }

  static const char* GetNameFor(...) {
    // For non-official builds construct the name of a type from a compiler
    // intrinsic.
    //
    // Do not include such type information in official builds to
    // (a) safe binary size on string literals, and
    // (b) avoid exposing internal types until a proper DevTools frontend
    //     implementation is present.
#if defined(OFFICIAL_BUILD) || !(defined(COMPILER_GCC) || defined(__clang__))
    return "InternalNode";
#else
    DCHECK(!HideInternalName());
    static const char* leaky_class_name = nullptr;
    if (leaky_class_name)
      return leaky_class_name;

    // Parsing string of structure:
    //   const char *WTF::GetStringWithTypeName<TYPE>() [T = TYPE]
    // Note that this only works on clang or GCC builds.
    const std::string raw(WTF::GetStringWithTypeName<T>());
    const auto start_pos = raw.rfind("T = ") + 4;
    DCHECK(std::string::npos != start_pos);
    const auto len = raw.length() - start_pos - 1;
    const std::string name = raw.substr(start_pos, len).c_str();
    leaky_class_name = strcpy(new char[name.length() + 1], name.c_str());
    return leaky_class_name;
#endif
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_NAME_TRAITS_H_
