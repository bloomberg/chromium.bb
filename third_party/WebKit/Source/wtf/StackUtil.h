// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StackUtil_h
#define StackUtil_h

#include "wtf/Compiler.h"
#include "wtf/WTFExport.h"
#include "wtf/build_config.h"
#include <stddef.h>
#include <stdint.h>

namespace WTF {

WTF_EXPORT size_t getUnderestimatedStackSize();
WTF_EXPORT void* getStackStart();

namespace internal {

WTF_EXPORT uintptr_t mainThreadUnderestimatedStackSize();

#if OS(WIN) && COMPILER(MSVC)
size_t threadStackSize();
#endif

// Returns true if the function is not called on the main thread. Note carefully
// that this function may have false positives, i.e. it can return true even if
// we are on the main thread. If the function returns false, we are certainly
// on the main thread. Must be WTF_EXPORT due to template inlining in
// ThreadSpecific.
inline WTF_EXPORT bool mayNotBeMainThread() {
  // getStackStart is exclusive, not inclusive (i.e. it points past the last
  // page of the stack in linear order). So, to ensure an inclusive comparison,
  // subtract here and in mainThreadUnderestimatedStackSize().
  static uintptr_t s_mainThreadStackStart =
      reinterpret_cast<uintptr_t>(getStackStart()) - sizeof(void*);
  static uintptr_t s_mainThreadUnderestimatedStackSize =
      mainThreadUnderestimatedStackSize();
  uintptr_t dummy;
  uintptr_t addressDiff =
      s_mainThreadStackStart - reinterpret_cast<uintptr_t>(&dummy);
  // This is a fast way to judge if we are in the main thread.
  // If |&dummy| is within |s_mainThreadUnderestimatedStackSize| byte from
  // the stack start of the main thread, we judge that we are in
  // the main thread.
  return addressDiff >= s_mainThreadUnderestimatedStackSize;
}

}  // namespace internal

}  // namespace WTF

#endif  // StackUtil_h
